#include "DAGBuilder.h"

#include <iomanip>
#include <iostream>
#include <set>

std::vector<DAGNode> DAGBuilder::build(const std::vector<Instruction>& instructions) {
    std::vector<DAGNode> nodes;
    nodes.reserve(instructions.size());

    for (size_t i = 0; i < instructions.size(); i++) {
        nodes.emplace_back(static_cast<int>(i));
    }

    detectRAW(instructions, nodes);
    detectWAR(instructions, nodes);
    detectWAW(instructions, nodes);

    for (auto& node : nodes) {
        node.unscheduled_predecessors = static_cast<int>(node.predecessors.size());
    }

    return nodes;
}

// Optimization: Only need to track the last write to each register
void DAGBuilder::detectRAW(const std::vector<Instruction>& instructions,
                           std::vector<DAGNode>& nodes) {
    std::map<std::string, int> lastWriter;

    for (size_t i = 0; i < instructions.size(); i++) {
        const Instruction& inst = instructions[i];

        for (const auto& srcReg : inst.src_regs) {
            auto it = lastWriter.find(srcReg);
            if (it != lastWriter.end()) {
                // Found RAW dependency
                int writerIdx = it->second;
                addEdge(nodes, writerIdx, static_cast<int>(i), DependencyType::RAW, srcReg);
            }
        }

        // Update last writer
        if (inst.hasDestReg()) {
            lastWriter[inst.dest_reg] = static_cast<int>(i);
        }
    }
}

void DAGBuilder::detectWAR(const std::vector<Instruction>& instructions,
                           std::vector<DAGNode>& nodes) {
    // TODO: Implement this function
    // Hint:
    // You need Track all readers of each register (after the last write)
    // Iterate each instruction, If current instruction writes a register, check previous readers
    // Add WAR dependency to all previous readers
    // WAR latency is 0 because read happens at instruction start
    throw std::runtime_error("WAR dependency detection not implemented");
}

void DAGBuilder::detectWAW(const std::vector<Instruction>& instructions,
                           std::vector<DAGNode>& nodes) {
    // TODO: Implement this function
    // Hint:
    // Track the last instruction that wrote to each register
    // WAW latency is 1, only need to guarantee order
    throw std::runtime_error("WAW dependency detection not implemented");
}

void DAGBuilder::addEdge(
    std::vector<DAGNode>& nodes, int from, int to, DependencyType type, const std::string& reg) {
    for (const auto& edge : nodes[from].successors) {
        if (edge.target_node == to && edge.type == type && edge.reg == reg) {
            return;  // Edge already exists
        }
    }

    // from -> to
    nodes[from].successors.emplace_back(to, type, reg);

    // to <- from
    nodes[to].predecessors.emplace_back(from, type, reg);
}

void DAGBuilder::printDAG(const std::vector<DAGNode>& nodes,
                          const std::vector<Instruction>& instructions,
                          std::ostream& os) {
    os << "\n";
    os << "Dependency Graph (DAG)\n";
    os << "  [ <- ] Predecessors (instructions that must be completed first):\n";
    os << "  [ -> ] Successors (instructions that depend on this instruction):\n";
    os << "\n";

    for (size_t i = 0; i < nodes.size(); i++) {
        const auto& node = nodes[i];
        os << "Instruction " << i << ": " << instructions[i].raw_text << "\n";

        if (!node.predecessors.empty()) {
            for (const auto& edge : node.predecessors) {
                os << "   <- [" << edge.target_node << "] " << dependencyTypeToString(edge.type)
                   << " on " << edge.reg << "\n";
            }
        }

        if (!node.successors.empty()) {
            for (const auto& edge : node.successors) {
                os << "    -> [" << edge.target_node << "] " << dependencyTypeToString(edge.type)
                   << " on " << edge.reg << "\n";
            }
        }

        if (node.predecessors.empty() && node.successors.empty()) {
            os << "  (no dependencies)\n";
        }

        os << "\n";
    }
}
