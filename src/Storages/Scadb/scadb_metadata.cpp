#include "scadb_metadata.h"
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <iostream>
#include <sstream>
namespace DB {

void print(boost::property_tree::ptree const& pt)
{
    using boost::property_tree::ptree;
    ptree::const_iterator end = pt.end();
    for (ptree::const_iterator it = pt.begin(); it != end; ++it) {
        std::cout << it->first << ": " << it->second.get_value<std::string>() << std::endl;
        print(it->second);
    }
}


void ScadbMetadata::load()
{
    std::string path = SCASAL_BUSINESS_PATH + business + "/backends/";
    etcd::Response response = etcd->ls(path).get();
    for (size_t i = 0 ; i< response.keys().size() ; ++i )
    {
        std::string key = response.key(i);
        if ( key.compare(0, path.length(), path) == 0) {
            // std::string database = path.substr(path.length());            
        }
    }

    path = SCASAL_BUSINESS_PATH + business + "/dbs/" + database + "/" + table ;
    std::cout << path << std::endl; 
    response = etcd->get(path).get(); 
    if (!response.is_ok() ) {
        std::cout << "not ok" << std::endl;         
        throw std::runtime_error("read metadata etcd " + path + " failed " + response.error_message());
    }
    std::string value = response.value().as_string();
    /*
    std::cout << response.value().as_string() << std::endl;    
    std::stringstream ss ;
    ss << value ;    
    boost::property_tree::ptree pt ;
    boost::property_tree::read_json(ss, pt);    
    //print(pt);

    using boost::property_tree::ptree;
    ptree::const_iterator end = pt.end();
    for (ptree::const_iterator it = pt.begin(); it != end; ++it) {        
        std::cout << it->first << ": " << it->second.get_value<std::string>() << std::endl;
        print(it->second);
    }
    */

    tableMeta.load(value);
    tableMeta.print() ;

    std::pair<std::string, uint16_t> address = std::make_pair(CDB_ADDRESS, 8066);
    mysqlxx::PoolWithFailover::RemoteDescription raddress ;
    raddress.push_back(address) ;
    for ( auto& pc: tableMeta.partitionsConfig ) {
        if ( backends.find(pc.backend) == backends.end()) {        
            mysqlxx::PoolWithFailover p(
                database, raddress,
                std::string("myshard@") + pc.backend + "@fix", "44poopvRgOlwStt2gfxRGNPv",
                MYSQLXX_POOL_WITH_FAILOVER_DEFAULT_START_CONNECTIONS,
                mysql_settings.connection_pool_size,
                mysql_settings.connection_max_tries,
                mysql_settings.connection_wait_timeout);
        
            mysqlxx::PoolWithFailoverPtr pool = std::make_shared<mysqlxx::PoolWithFailover>(std::move(p));
            backends.insert(std::make_pair(pc.backend, pool))  ;
        }
    }

}

// std::string ScadbMetadata::getPartitionTable(int partition) {
//     return boost::format{"%s_%04d"} % table % partition ;
// }
}