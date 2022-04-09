#include "etcd/Client.hpp"
#include "scadb_table.h"
#include <mysqlxx/PoolWithFailover.h>
#include <Storages/MySQL/MySQLSettings.h>

#define CDB_INFO_PATH  "/scasql/admin/cdb"
#define SCASAL_BUSINESS_PATH "/scasql/businesses/"

#define CDB_ADDRESS "cdboffline.dbms.self.yydevops.com"
namespace DB
{    
class ScadbMetadata
{
public:
    explicit ScadbMetadata(
        MySQLSettings& mysql_settings_,
        etcd::Client* etcd_,
        const std::string& business_,
        const std::string& database_,
        const std::string& table_        
    ) : mysql_settings(mysql_settings_), etcd(etcd_), business(business_),database(database_), table(table_) 
    {
    }

    void load() ;

    const std::string& getDatabase() const { return database ;}
    const std::string& getTable() const { return table ;}
    //std::string getPartitionTable(int partition) ;
    const ScadbTable& getMetadata() const {  return tableMeta; }    
    mysqlxx::PoolWithFailoverPtr getPool(const std::string& backend) {
        std::map<std::string, mysqlxx::PoolWithFailoverPtr>::const_iterator it = backends.find(backend) ;
        if ( it != backends.end() ) {
            return it->second ;
        }
        return NULL;
    }
private:
    MySQLSettings& mysql_settings;
//  Input param
    etcd::Client*  etcd;
    const std::string   business;
    const std::string   database;
    const std::string   table;
// Real Metadata
    //std::map<std::string, std::string> cdbs;
    //std::vector<std::string>           backends;
    std::map<std::string, mysqlxx::PoolWithFailoverPtr> backends;
    ScadbTable          tableMeta;
};

}
