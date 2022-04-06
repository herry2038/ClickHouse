#include "scadb_table.h"
//#include <boost/property_tree/ptree.hpp>
//#include <boost/property_tree/json_parser.hpp>
//#include <boost/json.hpp>
#include <boost/foreach.hpp>
#include <Poco/JSON/JSON.h>
#include <Poco/JSON/Parser.h>
#include <Poco/Dynamic/Var.h>

#include <iostream>
#include <sstream>


std::vector<std::string> split(const std::string& s, char delimiter)
{
   std::vector<std::string> tokens;
   std::string token;
   std::istringstream tokenStream(s);
   while (std::getline(tokenStream, token, delimiter))
   {
      tokens.push_back(token);
   }
   return tokens;
}

/*
std::string boost_json_string_to_std_string(const boost::json::string& str)
{
    return std::string (str.c_str(), str.size()) ;
    
}
std::string GetValue(Poco::JSON::Object::Ptr aoJsonObject, const char *aszKey) {
    Poco::Dynamic::Var loVariable;
    string lsReturn;
    string lsKey(aszKey);

    // Get the member Variable    
    loVariable = aoJsonObject->get(lsKey);    

    // Get the Value from the Variable
    //
    lsReturn = loVariable.convert<std::string>();

    return lsReturn;
}

int GetValueInt(Poco::JSON::Object::Ptr aoJsonObject, const char *aszKey) {
    Poco::Dynamic::Var loVariable;
    int lsReturn;
    string lsKey(aszKey);

    // Get the member Variable    
    loVariable = aoJsonObject->get(lsKey);    

    // Get the Value from the Variable
    //
    lsReturn = loVariable.convert<int>();
    return lsReturn;
}
*/


void ScadbTable::print() 
{
    std::cout << "table:" << name << std::endl
        << "slots:" << slots << std::endl
        << "blocks:" << blocks << std::endl
        << "shardType:" << shardType << std::endl
        << "shardKey:" << shardKey << std::endl
        << "shardKeyType:" << shardKeyType << std::endl ;
    for (auto &p: partitionsConfig) {
        std::cout << "\t" << p.table << "," << p.backend << "," << p.segment << std::endl;
    }

    for (auto &i: partitions) {
        std::cout << "segment: "  << i.first << "==>"
            << i.second.table<< "," << i.second.backend  <<std::endl;
    }
}


void ScadbTable::load(const std::string& tableConfig) 
{
    // std::stringstream ss ;
    // ss << tableConfig ;
    // boost::property_tree::ptree pt ;
    // boost::property_tree::read_json(ss, pt);    
    //print(pt);
    //boost::json::error_code ec;
    //auto decode_val = boost::json::parse(tableConfig, ec);
    
    try {
        Poco::JSON::Parser loParser;
        Poco::Dynamic::Var loParsedJson = loParser.parse(tableConfig);
        
        // if ( ec.failed() ) {
        //     std::cout << "parse tableConfig：" << tableConfig << " failed: <" << ec.message() << std::endl;        
        //     throw std::runtime_error("parse tableConfig：" + tableConfig + " failed: <" + ec.message());
        // }
        
        Poco::JSON::Object::Ptr result = loParsedJson.extract<Poco::JSON::Object::Ptr>();

        name = result->getValue<std::string>("name") ;
        slots = result->getValue<int>("slots-readonly");
        blocks = result->getValue<int>("blocks-readonly") ;
        shardType = result->getValue<std::string>("shardtype") ;
        shardKey = result->getValue<std::string>("shardkey") ;
        Poco::JSON::Array::Ptr ps = result->getArray("partitions") ;  
        
        
        for (std::size_t i=0 ;i< ps->size(); i++) {
            Poco::JSON::Object::Ptr p = ps->getObject(i) ;
            ScadbPartitionConfig c ;                        
            c.table = p->getValue<std::string>("table");
            c.segment = p->getValue<std::string>("segment");
            c.listValue = p->getValue<std::string>("listvalue");
            c.backend = p->getValue<std::string>("backend");
            partitionsConfig.push_back(c);
        } 
        buildSegments() ;
    } catch (std::exception& ex) {
        std::cout << "parse tableConfig：" << tableConfig << " failed: <" << ex.what() << std::endl;        
        throw std::runtime_error("parse tableConfig：" + tableConfig + " failed: <" + ex.what());
    }
}


void ScadbTable::buildSegments() 
{
    if ( shardType == "SINGLE" ) {
        if ( partitionsConfig.size() != 1) {
            throw std::runtime_error("invalid single table segments!");
        }
        ScadbSegment segment ; 
        segment.backend = partitionsConfig.at(0).backend ;
        segment.table = partitionsConfig.at(0).table;
        segment.segment = 0 ;
        partitions.insert(std::make_pair(0, segment));
    } else if ( shardType == "HASH2" ) {    
        for (auto &conf: partitionsConfig ) {            
            std::vector<std::string> lists = split(conf.segment, ',');
            char table[256];
            for ( auto & l: lists) {
                ScadbSegment segment ; 
                segment.backend = conf.backend;
                int index = atoi(l.c_str());                
                sprintf(table, "%s_%04d", name.c_str(), index) ;   
                segment.table = table ;             
                segment.segment = index ;
                partitions.insert(std::make_pair(atoi(l.c_str()), segment));
            }
        }
    } else {
        throw std::runtime_error("unknown shardType:[" + shardType + "]") ;
    }
}
