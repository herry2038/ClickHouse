#include "StorageScadb.h"

#include <Storages/StorageFactory.h>
#include <Storages/transformQueryForExternalDatabase.h>
#include <Processors/Sources/MySQLSource.h>
#include <Interpreters/evaluateConstantExpression.h>
#include <Core/Settings.h>
#include <Interpreters/Context.h>
#include <DataTypes/DataTypeString.h>
#include <Formats/FormatFactory.h>
#include <Processors/Formats/IOutputFormat.h>
#include <Common/parseAddress.h>
#include <IO/Operators.h>
#include <IO/WriteHelpers.h>
#include <Parsers/ASTLiteral.h>
#include <Parsers/ASTCreateQuery.h>
#include <mysqlxx/Transaction.h>
#include <Processors/Sinks/SinkToStorage.h>
#include <QueryPipeline/Pipe.h>
#include <Common/parseRemoteDescription.h>
#include <base/logger_useful.h>
#include <Common/HashTable/HashMap.h>

namespace DB
{

namespace ErrorCodes
{
    extern const int NUMBER_OF_ARGUMENTS_DOESNT_MATCH;
    extern const int BAD_ARGUMENTS;
}

static String backQuoteMySQL(const String & x)
{
    String res(x.size(), '\0');
    {
        WriteBufferFromString wb(res);
        writeBackQuotedStringMySQL(x, wb);
    }
    return res;
}

StorageScadb::StorageScadb(
    const StorageID & table_id_,
    ScadbMetadata &&scadbMetadata_,
    //const std::string & remote_database_name_,
    //const std::string & remote_table_name_,
    const bool replace_query_,
    const std::string & on_duplicate_clause_,
    const ColumnsDescription & columns_,
    const ConstraintsDescription & constraints_,
    const String & comment,
    ContextPtr context_,
    const MySQLSettings & mysql_settings_)
    : IStorage(table_id_)
    , WithContext(context_->getGlobalContext())
    //, remote_database_name(remote_database_name_)
    //, remote_table_name(remote_table_name_)
    , metadata_(std::make_shared<ScadbMetadata>(scadbMetadata_))
    , replace_query{replace_query_}
    , on_duplicate_clause{on_duplicate_clause_}        
    , mysql_settings(mysql_settings_)
    , log(&Poco::Logger::get("StorageScadb (" + table_id_.table_name + ")"))
{
    StorageInMemoryMetadata storage_metadata;
    storage_metadata.setColumns(columns_);
    storage_metadata.setConstraints(constraints_);
    storage_metadata.setComment(comment);
    setInMemoryMetadata(storage_metadata);
}


Pipe StorageScadb::read(
    const Names & column_names_,
    const StorageMetadataPtr & metadata_snapshot,
    SelectQueryInfo & query_info_,
    ContextPtr context_,
    QueryProcessingStage::Enum /*processed_stage*/,
    size_t /*max_block_size*/,
    unsigned)
{
    metadata_snapshot->check(column_names_, getVirtuals(), getStorageID());
    
    Pipes pipes;

    Block sample_block;
    for (const String & column_name : column_names_)
    {
        auto column_data = metadata_snapshot->getColumns().getPhysical(column_name);

        WhichDataType which(column_data.type);
        /// Convert enum to string.
        if (which.isEnum())
            column_data.type = std::make_shared<DataTypeString>();
        sample_block.insert({ column_data.type, column_data.name });
    }

    for (auto& pc : metadata_->getMetadata().partitions) {        
        auto& segment = pc.second ;
        String query = transformQueryForExternalDatabase(
                query_info_,
                metadata_snapshot->getColumns().getOrdinary(),
                IdentifierQuotingStyle::BackticksMySQL,
                metadata_->getDatabase(),
                //metadata_->getPartitionTable(partitionId),
                segment.table,
                context_);
        LOG_TRACE(log, "Query: {}", query);
        mysqlxx::PoolWithFailoverPtr pool = metadata_->getPool(segment.backend);
        StreamSettings mysql_input_stream_settings(context_->getSettingsRef(),
            mysql_settings.connection_auto_close);
        auto source = std::make_shared<MySQLWithFailoverSource>(pool, query, sample_block, mysql_input_stream_settings);
        pipes.emplace_back(source);
    }
    /*
    auto& segment = metadata_->getMetadata().partitions.at(0);
    String query = transformQueryForExternalDatabase(
            query_info_,
            metadata_snapshot->getColumns().getOrdinary(),
            IdentifierQuotingStyle::BackticksMySQL,
            metadata_->getDatabase(),
            //metadata_->getPartitionTable(partitionId),
            segment.table,
            context_);    
    LOG_TRACE(log, "Query: {}", query);

    Block sample_block;
    for (const String & column_name : column_names_)
    {
        auto column_data = metadata_snapshot->getColumns().getPhysical(column_name);

        WhichDataType which(column_data.type);
        /// Convert enum to string.
        if (which.isEnum())
            column_data.type = std::make_shared<DataTypeString>();
        sample_block.insert({ column_data.type, column_data.name });
    }

    mysqlxx::PoolWithFailoverPtr pool = metadata_->getPool(segment.backend);
    StreamSettings mysql_input_stream_settings(context_->getSettingsRef(),
        mysql_settings.connection_auto_close);
    return Pipe(std::make_shared<MySQLWithFailoverSource>(pool, query, sample_block, mysql_input_stream_settings));
    */
    return Pipe::unitePipes(std::move(pipes)) ;
}

    struct BlockWithPartitionTable
    {
        Block block;
        std::string partition_table;   
        std::string backend ;     

        BlockWithPartitionTable(Block && block_, std::string && partition_, std::string && backend_)
            : block(block_), partition_table(std::move(partition_)), backend(std::move(backend_))
        {
        }
    };
    using BlocksWithPartitionTable = std::vector<BlockWithPartitionTable>;


class StorageScadbSink : public SinkToStorage
{
public:
    explicit StorageScadbSink(
        const StorageScadb & storage_,
        const StorageMetadataPtr & metadata_snapshot_,
        std::shared_ptr<ScadbMetadata> metadata_,
        //const mysqlxx::PoolWithFailover::Entry & entry_,
        const size_t & mysql_max_rows_to_insert,
        Poco::Logger * log_)
        : SinkToStorage(metadata_snapshot_->getSampleBlock())
        , storage{storage_}
        , metadata_snapshot{metadata_snapshot_}
        , metadata{metadata_}        
        //, entry{entry_}
        , max_batch_rows{mysql_max_rows_to_insert}
        , log(log_)
    {
    }

    String getName() const override { return "StorageScadbSink"; }

    void consume(Chunk chunk) override
    {
        auto block = getHeader().cloneWithColumns(chunk.detachColumns());
        auto partitionBlocks = splitBlockIntoPartitions(block);        

        for (const BlockWithPartitionTable& blockWithPartition: partitionBlocks) 
        {            
            auto blocks = splitBlocks(blockWithPartition.block, max_batch_rows);
            mysqlxx::PoolWithFailoverPtr p = metadata->getPool(blockWithPartition.backend); 
            auto entry = p->get();
            mysqlxx::Transaction trans(entry);
            try
            {
                for (const Block & batch_data : blocks)
                {
                    writeBlockData(batch_data, blockWithPartition.partition_table, entry);
                }
                trans.commit();
            }
            catch (...)
            {
                trans.rollback();
                throw;
            }
        }
    }


    BlocksWithPartitionTable splitBlockIntoPartitions(const Block & block)
    {
        auto & key = metadata->getMetadata().shardKey ;
        const DB::ColumnWithTypeAndName* column = nullptr; 
        for ( size_t i = 0 ; i< block.columns(); i++ )
        {
            auto & columnAndPos = block.getByPosition(i) ;
            if ( columnAndPos.name == key )
            {
                column = &columnAndPos ;
            }
        }
        if ( column == nullptr ) 
        {
            throw Exception(ErrorCodes::BAD_ARGUMENTS,
                        "Scadb insert must has partition key: {}", key);
        }
        
        PODArray<size_t> partition_num_to_partition_id;
        IColumn::Selector selector;
        buildPartitionSelector(*column, partition_num_to_partition_id, selector);
        BlocksWithPartitionTable result;
        size_t partitions_count = partition_num_to_partition_id.size();
        result.reserve(partitions_count);

        auto get_partition_table_name = [&](size_t num)
        {
            auto it = metadata->getMetadata().partitions.find(partition_num_to_partition_id[num]);
            if ( it == metadata->getMetadata().partitions.end() ) 
            {
                // IMPOSSIBLE
                throw Exception(ErrorCodes::BAD_ARGUMENTS,
                        "Scadb partition invalide : {} invalid!" , partition_num_to_partition_id[0]);
            }            
            return std::pair<std::string, std::string>(it->second.table, it->second.backend);
        };
        
        if ( partitions_count == 1) 
        {        
            auto pair = get_partition_table_name(0) ;
            result.emplace_back(Block(block), std::string(pair.first), std::string(pair.second));  
            return result;
        }


        for (size_t i = 0; i < partitions_count; ++i)
        {
            auto pair = get_partition_table_name(i) ;
            result.emplace_back(block.cloneEmpty(), std::string(pair.first), std::string(pair.second));  
        }

        for (size_t col = 0; col < block.columns(); ++ col)
        {
            MutableColumns scattered = block.getByPosition(col).column->scatter(partitions_count, selector);
            for (size_t i = 0; i < partitions_count; ++i)
                result[i].block.getByPosition(col).column = std::move(scattered[i]);
        }
        return result;

    }


    size_t scadbHash(size_t row, const DB::ColumnWithTypeAndName& column)
    {
        // 先支持INT型的hash，TODO: string型的分区键，后面在支持
        auto ti = column.type->getTypeId();        
        if ( ti >= TypeIndex::UInt8 && ti <= TypeIndex::Int256) 
        {
            return column.column->getUInt(row) % (metadata->getMetadata().partitions.size());
        }
        else if ( ti >= TypeIndex::String && ti <= TypeIndex::FixedString )
        {   
            return static_cast<size_t> (atoi(column.column->getRawData().data)) % (metadata->getMetadata().partitions.size()) ;
        }

        throw Exception(ErrorCodes::BAD_ARGUMENTS,
                        "Scadb partition value type : {} invalid!" , ti);
    }

    void buildPartitionSelector(const DB::ColumnWithTypeAndName& column, PODArray<size_t> & partition_num_to_first_row, IColumn::Selector & selector)
    {
        using Data = HashMap<size_t, size_t>;
        Data partitions_map ;

        size_t num_rows = column.column->size() ;

        size_t partitions_count = 0;
        for (size_t i=0 ;i <num_rows; ++i)
        {
            Data::key_type key = scadbHash(i, column);
            typename Data::LookupResult it ; 
            bool inserted ;
            partitions_map.emplace(key, it, inserted); 

            if ( inserted )
            {
                partition_num_to_first_row.push_back(key);
                it->getMapped() = partitions_count;
                
                ++ partitions_count;
                /// Optimization for common case when there is only one partition - defer selector initialization.
                if ( partitions_map.size() == 2 )
                {
                    selector = IColumn::Selector(num_rows);
                    std::fill(selector.begin(), selector.begin() + i, 0);
                }
            }

            if (partitions_count > 1) 
                selector[i] = it->getMapped(); 
        }
    }

    void writeBlockData(const Block & block, const std::string& table, mysqlxx::PoolWithFailover::Entry& entry)
    {
        WriteBufferFromOwnString sqlbuf;
        sqlbuf << (storage.replace_query ? "REPLACE" : "INSERT") << " INTO ";
        if (!metadata->getDatabase().empty())
            sqlbuf << backQuoteMySQL(metadata->getDatabase()) << ".";
        sqlbuf << backQuoteMySQL(table);
        sqlbuf << " (" << dumpNamesWithBackQuote(block) << ") VALUES ";

        auto writer = FormatFactory::instance().getOutputFormat("Values", sqlbuf, metadata_snapshot->getSampleBlock(), storage.getContext());
        writer->write(block);

        if (!storage.on_duplicate_clause.empty())
            sqlbuf << " ON DUPLICATE KEY " << storage.on_duplicate_clause;

        sqlbuf << ";";
        LOG_TRACE(log, "Query: {}", sqlbuf.str());

        auto query = entry->query(sqlbuf.str());
        query.execute();
    }

    Blocks splitBlocks(const Block & block, const size_t & max_rows) const
    {
        /// Avoid Excessive copy when block is small enough
        if (block.rows() <= max_rows)
            return Blocks{std::move(block)};

        const size_t split_block_size = ceil(block.rows() * 1.0 / max_rows);
        Blocks split_blocks(split_block_size);

        for (size_t idx = 0; idx < split_block_size; ++idx)
            split_blocks[idx] = block.cloneEmpty();

        const size_t columns = block.columns();
        const size_t rows = block.rows();
        size_t offsets = 0;
        UInt64 limits = max_batch_rows;
        for (size_t idx = 0; idx < split_block_size; ++idx)
        {
            /// For last batch, limits should be the remain size
            if (idx == split_block_size - 1) limits = rows - offsets;
            for (size_t col_idx = 0; col_idx < columns; ++col_idx)
            {
                split_blocks[idx].getByPosition(col_idx).column = block.getByPosition(col_idx).column->cut(offsets, limits);
            }
            offsets += max_batch_rows;
        }

        return split_blocks;
    }

    static std::string dumpNamesWithBackQuote(const Block & block)
    {
        WriteBufferFromOwnString out;
        for (auto it = block.begin(); it != block.end(); ++it)
        {
            if (it != block.begin())
                out << ", ";
            out << backQuoteMySQL(it->name);
        }
        return out.str();
    }

private:
    const StorageScadb & storage;
    StorageMetadataPtr metadata_snapshot;
    //std::string remote_database_name;
    //std::string remote_table_name;
    std::shared_ptr<ScadbMetadata> metadata ;
    //mysqlxx::PoolWithFailover::Entry entry;
    size_t max_batch_rows;
    Poco::Logger * log;
};


SinkToStoragePtr StorageScadb::write(const ASTPtr & /*query*/, const StorageMetadataPtr & metadata_snapshot, ContextPtr local_context)
{
    return std::make_shared<StorageScadbSink>(
        *this,
        metadata_snapshot,
        metadata_,        
        //pool->get(), 
        //mysqlxx::PoolWithFailover::Entry(),  //只为让编译通过
        local_context->getSettingsRef().mysql_max_rows_to_insert, log);
}


StorageMySQLConfiguration StorageScadb::getConfiguration(ASTs engine_args, ContextPtr context_)
{
    StorageMySQLConfiguration configuration;

    if (auto named_collection = getExternalDataSourceConfiguration(engine_args, context_))
    {
        auto [common_configuration, storage_specific_args] = named_collection.value();
        configuration.set(common_configuration);
        configuration.addresses = {std::make_pair(configuration.host, configuration.port)};

        for (const auto & [arg_name, arg_value] : storage_specific_args)
        {
            if (arg_name == "replace_query")
                configuration.replace_query = arg_value->as<ASTLiteral>()->value.safeGet<bool>();
            else if (arg_name == "on_duplicate_clause")
                configuration.on_duplicate_clause = arg_value->as<ASTLiteral>()->value.safeGet<String>();
            else
                throw Exception(ErrorCodes::BAD_ARGUMENTS,
                        "Unexpected key-value argument."
                        "Got: {}, but expected one of:"
                        "host, port, username, password, database, table, replace_query, on_duplicate_clause.", arg_name);
        }
    }
    else
    {
        if (engine_args.size() < 5 || engine_args.size() > 7)
            throw Exception(
                "Storage MySQL requires 5-7 parameters: MySQL('host:port' (or 'addresses_pattern'), database, table, 'user', 'password'[, replace_query, 'on_duplicate_clause']).",
                ErrorCodes::NUMBER_OF_ARGUMENTS_DOESNT_MATCH);

        for (auto & engine_arg : engine_args)
            engine_arg = evaluateConstantExpressionOrIdentifierAsLiteral(engine_arg, context_);

        const auto & host_port = engine_args[0]->as<ASTLiteral &>().value.safeGet<String>();
        size_t max_addresses = context_->getSettingsRef().glob_expansion_max_elements;

        configuration.addresses = parseRemoteDescriptionForExternalDatabase(host_port, max_addresses, 3306);
        configuration.database = engine_args[1]->as<ASTLiteral &>().value.safeGet<String>();
        configuration.table = engine_args[2]->as<ASTLiteral &>().value.safeGet<String>();
        configuration.username = engine_args[3]->as<ASTLiteral &>().value.safeGet<String>();
        configuration.password = engine_args[4]->as<ASTLiteral &>().value.safeGet<String>();

        if (engine_args.size() >= 6)
            configuration.replace_query = engine_args[5]->as<ASTLiteral &>().value.safeGet<UInt64>();
        if (engine_args.size() == 7)
            configuration.on_duplicate_clause = engine_args[6]->as<ASTLiteral &>().value.safeGet<String>();
    }

    if (configuration.replace_query && !configuration.on_duplicate_clause.empty())
        throw Exception(ErrorCodes::BAD_ARGUMENTS,
                        "Only one of 'replace_query' and 'on_duplicate_clause' can be specified, or none of them");

    return configuration;
}


std::unique_ptr<etcd::Client> etcdGlobalClient_ ;


void registerStorageScadb(StorageFactory & factory)
{
    if ( !etcdGlobalClient_.get() ) {
        etcdGlobalClient_.reset(new etcd::Client("http://herrypc:2379"));
    }

    factory.registerStorage("Scadb", [](const StorageFactory::Arguments & args)
    {
        auto configuration = StorageScadb::getConfiguration(args.engine_args, args.getLocalContext());

        MySQLSettings mysql_settings; /// TODO: move some arguments from the arguments to the SETTINGS.
        if (args.storage_def->settings)
            mysql_settings.loadFromQuery(*args.storage_def);

        if (!mysql_settings.connection_pool_size)
            throw Exception("connection_pool_size cannot be zero.", ErrorCodes::BAD_ARGUMENTS);
        
        ScadbMetadata metadata(mysql_settings, etcdGlobalClient_.get(), "online_test", configuration.database, configuration.table);
        metadata.load() ;
        /*
        mysqlxx::PoolWithFailover pool(
            configuration.database, configuration.addresses,
            configuration.username, configuration.password,
            MYSQLXX_POOL_WITH_FAILOVER_DEFAULT_START_CONNECTIONS,
            mysql_settings.connection_pool_size,
            mysql_settings.connection_max_tries,
            mysql_settings.connection_wait_timeout);
        */
        return StorageScadb::create(
            args.table_id,
            std::move(metadata),
            //configuration.database,
            //configuration.table,
            configuration.replace_query,
            configuration.on_duplicate_clause,
            args.columns,
            args.constraints,
            args.comment,
            args.getContext(),
            mysql_settings);
    },
    {
        .supports_settings = true,
        .source_access_type = AccessType::MYSQL,
    });
}

}

