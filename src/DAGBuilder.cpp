#include "DAGBuilder.h"

#include <iomanip>
#include <iostream>
#include <set>
#include <unordered_map>
#include <unordered_set>

std::vector<DAGNode> DAGBuilder::build(const std::vector<Instruction>& instructions) {
    std::vector<DAGNode> nodes;
    nodes.reserve(instructions.size());

    for (size_t i = 0; i < instructions.size(); i++) {
        nodes.emplace_back(static_cast<int>(i));
    }

    // // the old multi-pass implementation
    // detectRAW(instructions, nodes);
    // detectWAR(instructions, nodes);
    // detectWAW(instructions, nodes);

    // ONEPASS DETECTION, the bonus part
    std::unordered_map<std::string, int> lastWriter;
    std::unordered_map<std::string, std::unordered_set<int>> readersSinceLastWrite;

    for (size_t i = 0; i < instructions.size(); i++) {
        const Instruction& inst = instructions[i];
        const int currentIdx = static_cast<int>(i);

        // RAW: current source depends on the last writer of the register we read from
        for (const auto& srcReg : inst.src_regs) {
            auto it = lastWriter.find(srcReg);
            if (it != lastWriter.end()) {
                addEdge(nodes, it->second, currentIdx, DependencyType::RAW, srcReg);
            }
        }

        if (inst.hasDestReg()) {
            const std::string& dest = inst.dest_reg;

            // WAW: current write depends on previous write in ordering
            auto writerIt = lastWriter.find(dest);
            if (writerIt != lastWriter.end()) {
                addEdge(nodes, writerIt->second, currentIdx, DependencyType::WAW, dest);
            }

            // WAR: current write depends on all readers since the last write
            auto readersIt = readersSinceLastWrite.find(dest);
            if (readersIt != readersSinceLastWrite.end()) {
                for (int readerIdx : readersIt->second) {
                    addEdge(nodes, readerIdx, currentIdx, DependencyType::WAR, dest);
                }
                readersIt->second.clear();
            }

            lastWriter[dest] = currentIdx;
        }

        // record what reg this instruction reads
        for (const auto& srcReg : inst.src_regs) {
            readersSinceLastWrite[srcReg].insert(currentIdx);
        }
    }

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
                // found RAW dependency
                int writerIdx = it->second;
                addEdge(nodes, writerIdx, static_cast<int>(i), DependencyType::RAW, srcReg);
            }
        }

        // update last writer
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
    // throw std::runtime_error("WAR dependency detection not implemented");

    // Track readers observed since the last write for each register.
    std::map<std::string, std::set<int>> readersSinceLastWrite;

    for (size_t i = 0; i < instructions.size(); i++) {
        const Instruction& inst = instructions[i];
        const int currentIdx = static_cast<int>(i);

        if (inst.hasDestReg()) {
            const std::string& dest = inst.dest_reg;

            auto it = readersSinceLastWrite.find(dest);
            if (it != readersSinceLastWrite.end()) {
                for (int readerIdx : it->second) {
                    addEdge(nodes, readerIdx, currentIdx, DependencyType::WAR, dest);
                }
                it->second.clear();
            }
        }

        for (const auto& srcReg : inst.src_regs) {
            readersSinceLastWrite[srcReg].insert(currentIdx);
        }
    }
}

void DAGBuilder::detectWAW(const std::vector<Instruction>& instructions,
                           std::vector<DAGNode>& nodes) {
    // TODO: Implement this function
    // Hint:
    // Track the last instruction that wrote to each register
    // WAW latency is 1, only need to guarantee order
    // throw std::runtime_error("WAW dependency detection not implemented");

    std::map<std::string, int> lastWriter;

    for (size_t i = 0; i < instructions.size(); i++) {
        const Instruction& inst = instructions[i];
        if (!inst.hasDestReg()) {
            continue;
        }

        auto it = lastWriter.find(inst.dest_reg);
        if (it != lastWriter.end()) {
            addEdge(nodes, it->second, static_cast<int>(i), DependencyType::WAW, inst.dest_reg);
        }

        lastWriter[inst.dest_reg] = static_cast<int>(i);
    }
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
