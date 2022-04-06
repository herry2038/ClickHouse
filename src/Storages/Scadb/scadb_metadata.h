#include "etcd/Client.hpp"
#include "scadb_table.h"


#define CDB_INFO_PATH  "/scasql/admin/cdb"
#define SCASAL_BUSINESS_PATH "/scasql/businesses/"

#define CDB_ADDRESS "cdboffline.dbms.self.yydevops.com"

class ScadbMetadata
{
public:
    explicit ScadbMetadata(
        etcd::Client& etcd_,
        const std::string& business_,
        const std::string& database_,
        const std::string& table_        
    ) : etcd(etcd_), business(business_),database(database_), table(table_) 
    {
    }

    void load() ;
private:
//  Input param
    etcd::Client&  etcd;
    const std::string   business;
    const std::string   database;
    const std::string   table;
// Real Metadata
    //std::map<std::string, std::string> cdbs;
    std::vector<std::string>           backends;
    ScadbTable          tableMeta;
};


