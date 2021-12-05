#include <Processors/Executors/ExecutingGraph.h>

namespace DB
{

namespace ErrorCodes
{
    extern const int LOGICAL_ERROR;
}

ExecutingGraph::ExecutingGraph(const Processors & processors)
{
    uint64_t num_processors = processors.size();
    nodes.reserve(num_processors);

    /// Create nodes.
    for (uint64_t node = 0; node < num_processors; ++node)
    {
        IProcessor * proc = processors[node].get();
        processors_map[proc] = node;
        nodes.emplace_back(std::make_unique<Node>(proc, node));
    }

    /// Create edges.
    for (uint64_t node = 0; node < num_processors; ++node)
        addEdges(node);
}

ExecutingGraph::Edge & ExecutingGraph::addEdge(Edges & edges, Edge edge, const IProcessor * from, const IProcessor * to)
{
    auto it = processors_map.find(to);
    if (it == processors_map.end())
    {
        String msg = "Processor " + to->getName() + " was found as " + (edge.backward ? "input" : "output")
                     + " for processor " + from->getName() + ", but not found in list of processors.";

        throw Exception(msg, ErrorCodes::LOGICAL_ERROR);
    }

    edge.to = it->second;
    auto & added_edge = edges.emplace_back(std::move(edge));
    added_edge.update_info.id = &added_edge;
    return added_edge;
}

// 在构建ExeuctingGraph对象之前，会把各个Processor对象准备好，并且通过connect方法，实现Processor之间的连接。
// 在构建ExecutingGraph对象时，会调用addEdges为ExecutingGraph中的connection创建好对应的Edge对象。
// 其中每一个connection：连接关系为：OutputPort <-> InputPort
// 会分别创建一个前向Edge和一个后向Edge。
bool ExecutingGraph::addEdges(uint64_t node)
{
    IProcessor * from = nodes[node]->processor;

    bool was_edge_added = false;

    /// Add backward edges from input ports.  添加前向Edge，这里通过
    auto & inputs = from->getInputs();
    auto from_input = nodes[node]->back_edges.size();

    if (from_input < inputs.size())
    {
        was_edge_added = true;

        for (auto it = std::next(inputs.begin(), from_input); it != inputs.end(); ++it, ++from_input)
        {
            const IProcessor * to = &it->getOutputPort().getProcessor();
            auto output_port_number = to->getOutputPortNumber(&it->getOutputPort());
            // 后向Edge对象，这里的 to_ 参数是0，后面通过addEdge方法，找到to的编号，在传入对应的成员变量
            Edge edge(0, true, from_input, output_port_number, &nodes[node]->post_updated_input_ports);
            auto & added_edge = addEdge(nodes[node]->back_edges, std::move(edge), from, to);
            it->setUpdateInfo(&added_edge.update_info);
        }
    }

    /// Add direct edges form output ports.
    auto & outputs = from->getOutputs();
    auto from_output = nodes[node]->direct_edges.size();

    if (from_output < outputs.size())
    {
        was_edge_added = true;

        for (auto it = std::next(outputs.begin(), from_output); it != outputs.end(); ++it, ++from_output)
        {
            const IProcessor * to = &it->getInputPort().getProcessor();
            auto input_port_number = to->getInputPortNumber(&it->getInputPort());
            // 前向Edge对象，这里的 to_ 参数是0，后面通过addEdge方法，找到to的编号，在传入对应的成员变量
            Edge edge(0, false, input_port_number, from_output, &nodes[node]->post_updated_output_ports);
            auto & added_edge = addEdge(nodes[node]->direct_edges, std::move(edge), from, to);
            it->setUpdateInfo(&added_edge.update_info);
        }
    }

    return was_edge_added;
}

std::vector<uint64_t> ExecutingGraph::expandPipeline(const Processors & processors)
{
    uint64_t num_processors = processors.size();
    nodes.reserve(num_processors);

    while (nodes.size() < num_processors)
    {
        auto * processor = processors[nodes.size()].get();
        if (processors_map.count(processor))
            throw Exception("Processor " + processor->getName() + " was already added to pipeline.",
                            ErrorCodes::LOGICAL_ERROR);

        processors_map[processor] = nodes.size();
        nodes.emplace_back(std::make_unique<Node>(processor, nodes.size()));
    }

    std::vector<uint64_t> updated_nodes;

    for (uint64_t node = 0; node < num_processors; ++node)
    {
        if (addEdges(node))
            updated_nodes.push_back(node);
    }

    return updated_nodes;
}

}
