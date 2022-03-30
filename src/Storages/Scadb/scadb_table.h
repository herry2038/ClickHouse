#include <string>
#include <vector>
#include <map>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>


class ScadbPartitionConfig {
public:
    std::string     table;
    std::string     segment;
    std::string     backend;
    std::string     listValue;
};

class ScadbSegment {
public:    
    std::string     table;      // 实际分区表
    std::string     backend;    // 后端
    int             segment;    // 对应分区号，暂时先这样，这个将于key中的id一样
};

class ScadbTable 
{
public:
    std::string     name  ;
    int             slots ;
    int             blocks;
    std::string     shardType;
    std::string     shardKey;
    int              shardKeyType;
    std::vector<ScadbPartitionConfig> partitionsConfig;
    std::map<int, ScadbSegment> partitions;
    
    void load(const std::string& tableConfig);
    
    void print() ;
private:
    void buildSegments() ;
};
