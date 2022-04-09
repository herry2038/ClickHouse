#pragma once

#include "config_core.h"


#include <base/shared_ptr_helper.h>

#include <Storages/IStorage.h>
#include <Storages/MySQL/MySQLSettings.h>
#include <Storages/ExternalDataSourceConfiguration.h>
#include <mysqlxx/PoolWithFailover.h>
#include <Storages/Scadb/scadb_metadata.h>

namespace Poco
{
class Logger;
}

namespace DB
{

/** Implements storage in the Scasql database.
  * Use ENGINE = scadb(database_name, table_name)
  * Read only.
  */
class StorageScadb final : public shared_ptr_helper<StorageScadb>, public IStorage, WithContext
{
    friend struct shared_ptr_helper<StorageScadb>;
public:
    StorageScadb(
        const StorageID & table_id_,        
        ScadbMetadata &&scadbMetadata_,
        //const std::string & remote_database_name_,
        //const std::string & remote_table_name_,
        bool replace_query_,
        const std::string & on_duplicate_clause_,
        const ColumnsDescription & columns_,
        const ConstraintsDescription & constraints_,
        const String & comment,
        ContextPtr context_,
        const MySQLSettings & mysql_settings_);

    std::string getName() const override { return "Scasql"; }

    Pipe read(
        const Names & column_names,
        const StorageMetadataPtr & /*metadata_snapshot*/,
        SelectQueryInfo & query_info,
        ContextPtr context,
        QueryProcessingStage::Enum processed_stage,
        size_t max_block_size,
        unsigned num_streams) override;

    SinkToStoragePtr write(const ASTPtr & query, const StorageMetadataPtr & /*metadata_snapshot*/, ContextPtr context) override;

    static StorageMySQLConfiguration getConfiguration(ASTs engine_args, ContextPtr context_);

private:
    friend class StorageScadbSink;
    
    std::shared_ptr<ScadbMetadata> metadata_ ;
    //std::string remote_database_name;
    //std::string remote_table_name;
    bool replace_query;
    std::string on_duplicate_clause;

    MySQLSettings mysql_settings;
    //mysqlxx::PoolWithFailoverPtr pool;

    Poco::Logger * log;
};

}

