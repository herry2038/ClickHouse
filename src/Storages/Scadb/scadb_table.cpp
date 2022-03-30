#include "scadb_table.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/json.hpp>
#include <boost/foreach.hpp>
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

std::string boost_json_string_to_std_string(const boost::json::string& str)
{
    return std::string (str.c_str(), str.size()) ;
    
}


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
    boost::json::error_code ec;
    auto decode_val = boost::json::parse(tableConfig, ec);
    
    if ( ec.failed() ) {
        std::cout << "parse tableConfig：" << tableConfig << " failed: <" << ec.message() << std::endl;        
        throw std::runtime_error("parse tableConfig：" + tableConfig + " failed: <" + ec.message());
    }
    
    auto& jo = decode_val.as_object();
    
    name = boost_json_string_to_std_string(jo.at("name").as_string());
    
    slots = jo.at("slots-readonly").as_int64();
    
    blocks = jo.at("blocks-readonly").as_int64();
    
    shardType = boost_json_string_to_std_string(jo.at("shardtype").as_string()) ;
    
    shardKey = boost_json_string_to_std_string(jo.at("shardkey").as_string()) ;
    //shardKeyType = jo.at("shardKeyType").as_int64(); 
     
    auto& pc = jo.at("partitions").as_array() ;
    for ( auto &p: pc ) {
        ScadbPartitionConfig c ;
        
        c.table = boost_json_string_to_std_string(p.at("table").as_string());
        c.segment = boost_json_string_to_std_string(p.at("segment").as_string());
        c.listValue = boost_json_string_to_std_string(p.at("listvalue").as_string());
        c.backend = boost_json_string_to_std_string(p.at("backend").as_string());
        partitionsConfig.push_back(c);
    } 
    buildSegments() ;
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
