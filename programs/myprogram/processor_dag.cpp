#include <Processors/ISource.h>
#include <Processors/IProcessor.h>
#include <Processors/ISink.h>
#include <Processors/Executors/PipelineExecutor.h>
#include <Columns/ColumnsNumber.h>
#include <DataTypes/DataTypesNumber.h>
#include <IO/WriteBufferFromFileDescriptor.h>
#include <IO/WriteHelpers.h>

using namespace DB;


class MySource : public ISource
{
public:
    String getName() const override { return "MySource"; }

    MySource(UInt64 end_)
        : ISource(Block({ColumnWithTypeAndName{ColumnUInt64::create(), std::make_shared<DataTypeUInt64>(), "number"}})), end(end_)
    {
    }

private:
    UInt64 end;
    bool done = false;

    Chunk generate() override
    {
        if (done)
        {
            return Chunk();
        }
        MutableColumns columns;
        columns.emplace_back(ColumnUInt64::create());
        for (auto i = 0U; i < end; i++)
            columns[0]->insert(i);

        done = true;
        return Chunk(std::move(columns), end);
    }
};


class MyAddTransformer : public IProcessor
{
public:
    String getName() const override { return "MyAddTransformer"; }

    MyAddTransformer()
        : IProcessor(
            {Block({ColumnWithTypeAndName{ColumnUInt64::create(), std::make_shared<DataTypeUInt64>(), "number"}})},
            {Block({ColumnWithTypeAndName{ColumnUInt64::create(), std::make_shared<DataTypeUInt64>(), "number"}})})
        , input(inputs.front())
        , output(outputs.front())
    {
    }

    Status prepare() override
    {
        if (output.isFinished())
        {
            input.close();
            return Status::Finished;
        }

        if (!output.canPush())
        {
            input.setNotNeeded();
            return Status::PortFull;
        }

        if (has_process_data)
        {
            output.push(std::move(current_chunk));
            has_process_data = false;
        }

        if (input.isFinished())
        {
            output.finish();
            return Status::Finished;
        }

        if (!input.hasData())
        {
            input.setNeeded();
            return Status::NeedData;
        }
        current_chunk = input.pull(false);
        return Status::Ready;
    }

    void work() override
    {
        auto num_rows = current_chunk.getNumRows();
        auto result_columns = current_chunk.cloneEmptyColumns();
        auto columns = current_chunk.detachColumns();
        for (auto i = 0U; i < num_rows; i++)
        {
            auto val = columns[0]->getUInt(i);
            result_columns[0]->insert(val+1);
        }
        current_chunk.setColumns(std::move(result_columns), num_rows);
        has_process_data = true;
    }

    InputPort & getInputPort() { return input; }
    OutputPort & getOutputPort() { return output; }

protected:
    bool has_input = false;
    bool has_process_data = false;
    Chunk current_chunk;
    InputPort & input;
    OutputPort & output;
};

class MySink : public ISink
{
public:
    String getName() const override { return "MySinker"; }

    MySink() : ISink(Block({ColumnWithTypeAndName{ColumnUInt64::create(), std::make_shared<DataTypeUInt64>(), "number"}})) { }

private:
    WriteBufferFromFileDescriptor out{STDOUT_FILENO};
    FormatSettings settings;

    void consume(Chunk chunk) override
    {
        size_t rows = chunk.getNumRows();
        size_t columns = chunk.getNumColumns();

        for (size_t row_num = 0; row_num < rows; ++row_num)
        {
            writeString("prefix-", out);
            for (size_t column_num = 0; column_num < columns; ++column_num)
            {
                if (column_num != 0)
                    writeChar('\t', out);
                getPort()
                    .getHeader()
                    .getByPosition(column_num)
                    .type
		    ->getDefaultSerialization()
                    ->serializeText(*chunk.getColumns()[column_num], row_num, out, settings);
            }
            writeChar('\n', out);
        }

        out.next();
    }
};


int main(int, char **)
{
    auto source0 = std::make_shared<MySource>(5);
    auto add0 = std::make_shared<MyAddTransformer>();
    auto sinker0 = std::make_shared<MySink>();

    /// Connect.
    connect(source0->getPort(), add0->getInputPort());
    connect(add0->getOutputPort(), sinker0->getPort());

    std::vector<ProcessorPtr> processors = {source0, add0, sinker0};
    PipelineExecutor executor(processors);
    executor.execute(1);
}
