#include <Core/Block.h>
#include <Columns/ColumnVector.h>

// I know that inclusion of .cpp is not good at all
#include <Storages/MergeTree/MergeTreeDataPartWriterOnDisk.cpp> // NOLINT
#include <Storages/MergeTree/MergeTreeDataPartWriterWide.cpp> // NOLINT

using namespace DB;
static Block getBlockWithSize(size_t required_size_in_bytes, size_t size_of_row_in_bytes)
{

    ColumnsWithTypeAndName cols;
    size_t rows = required_size_in_bytes / size_of_row_in_bytes;
    for (size_t i = 0; i < size_of_row_in_bytes; i += sizeof(UInt64))
    {
        auto column = ColumnUInt64::create(rows, 0);
        cols.emplace_back(std::move(column), std::make_shared<DataTypeUInt64>(), "column" + std::to_string(i));
    }
    return Block(cols);
}


int main(int, char **)
{
    auto block1 = getBlockWithSize(80, 8);
    
    { /// Granularity bytes are not set. Take default index_granularity.
        MergeTreeIndexGranularity index_granularity;
        auto granularity = computeIndexGranularityImpl(block1, 0, 100, false, false);
        fillIndexGranularityImpl(index_granularity, 0, granularity, block1.rows());
        std::cout << index_granularity.getMarksCount() << ", equals to 1";
        std::cout << index_granularity.getMarkRows(0) << ", equals to 100";        
    }
}
