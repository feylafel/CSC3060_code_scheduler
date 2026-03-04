#include "Scheduler.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <queue>

ScheduleResult Scheduler::schedule(const std::vector<Instruction>& instructions,
                                   std::vector<DAGNode>& nodes,
                                   TieBreakingPolicy policy) {
    // TODO: Implement this function using provided API
    ScheduleResult result;
    throw std::runtime_error("schedule not implemented");
    return result;
}

ScheduleResult Scheduler::scheduleFast(const std::vector<Instruction>& instructions,
                                       std::vector<DAGNode>& nodes,
                                       TieBreakingPolicy policy,
                                       bool verbose) {
    ScheduleResult result;
    // TODO: BONUS: Optimize code as fast as you can
    throw std::runtime_error("scheduleFast not implemented");
    return result;
}

void Scheduler::computePriorities(const std::vector<Instruction>& instructions,
                                  std::vector<DAGNode>& nodes) {
    std::vector<bool> visited(nodes.size(), false);

    // Calculate priority for each node
    for (size_t i = 0; i < nodes.size(); i++) {
        if (!visited[i]) {
            computeCriticalPath(static_cast<int>(i), instructions, nodes, visited);
        }
    }
}

int Scheduler::computeCriticalPath(int nodeIdx,
                                   const std::vector<Instruction>& instructions,
                                   std::vector<DAGNode>& nodes,
                                   std::vector<bool>& visited) {
    // TODO: Implement this function
    throw std::runtime_error("computeCriticalPath not implemented");
    return 0;
}

std::vector<int> Scheduler::getReadyNodes(const std::vector<DAGNode>& nodes) {
    std::vector<int> ready;

    for (size_t i = 0; i < nodes.size(); i++) {
        // Node ready conditions:
        // 1. Not scheduled
        // 2. All predecessors scheduled (unscheduled_predecessors == 0)
        if (!nodes[i].scheduled && nodes[i].unscheduled_predecessors == 0) {
            ready.push_back(static_cast<int>(i));
        }
    }

    return ready;
}

int Scheduler::selectBestNode(const std::vector<int>& readyNodes,
                              const std::vector<DAGNode>& nodes,
                              const std::vector<Instruction>& instructions,
                              TieBreakingPolicy policy) {
    // Select node with highest priority
    // If priorities are equal, use tie-breaking policy
    // TODO: Implement Other Tie Condition
    int bestNode = readyNodes[0];
    int bestPriority = nodes[bestNode].priority;

    for (int nodeIdx : readyNodes) {
        int currentPriority = nodes[nodeIdx].priority;

        if (currentPriority > bestPriority) {
            bestPriority = currentPriority;
            bestNode = nodeIdx;
        } else if (currentPriority == bestPriority) {
            // Tie-breaking based on policy
            bool shouldReplace = false;

            switch (policy) {
                case TieBreakingPolicy::SMALLER_INDEX:
                    // Select node with smaller original index
                    shouldReplace = (nodeIdx < bestNode);
                    break;
            }

            if (shouldReplace) {
                bestNode = nodeIdx;
            }
        }
    }

    return bestNode;
}

void Scheduler::updateSuccessors(int nodeIdx, std::vector<DAGNode>& nodes) {
    for (const auto& edge : nodes[nodeIdx].successors) {
        int succIdx = edge.target_node;
        nodes[succIdx].unscheduled_predecessors--;
    }
}

void Scheduler::printSchedule(const ScheduleResult& result,
                              const std::vector<Instruction>& instructions,
                              const std::vector<DAGNode>& nodes,
                              std::ostream& os) {
    os << "\n";
    os << "Scheduling Result\n\n";

    // Print original order
    os << "Original instruction order:\n";
    os << "─────────────────────────────────────────────────────────────────\n";
    for (size_t i = 0; i < instructions.size(); i++) {
        os << "  " << std::setw(2) << i << ": " << instructions[i].raw_text << "\n";
    }

    os << "\n";

    // Print scheduled order
    os << "Scheduled instruction order:\n";
    os << "─────────────────────────────────────────────────────────────────\n";
    for (size_t i = 0; i < result.order.size(); i++) {
        int instIdx = result.order[i];
        os << "  " << std::setw(2) << i << ": [Orig " << std::setw(2) << instIdx << "] "
           << instructions[instIdx].raw_text << " (priority: " << nodes[instIdx].priority << ")\n";
    }

    os << "\n";
    os << "Scheduling statistics:\n";
    os << "─────────────────────────────────────────────────────────────────\n";
    os << "  Instruction count: " << instructions.size() << "\n";
    os << "\n";
}

// Simulation model:
// Assume infinite functional units (ignore resource conflicts)
// RAW dependency: successor must wait for predecessor start + edge latency (data ready)
// Edge latency = predecessor instruction execution latency, represents time to produce result
// WAR/WAW dependency: only need to guarantee order, successor can start after predecessor starts
// Instruction start time = max(issue position, all dependency ready times)
// Instruction completion time = start time + instruction latency
// Total cycles = completion time of last instruction
//
// This model shows how scheduling can hide latency by reordering instructions
SimulationResult Scheduler::simulateExecution(const std::vector<int>& order,
                                              const std::vector<Instruction>& instructions,
                                              const std::vector<DAGNode>& nodes) {
    SimulationResult result;
    int n = static_cast<int>(instructions.size());

    result.start_cycle.resize(n, 0);
    result.end_cycle.resize(n, 0);

    std::vector<int> order_position(n, -1);
    for (size_t i = 0; i < order.size(); i++) {
        order_position[order[i]] = static_cast<int>(i);
    }

    for (int pos = 0; pos < static_cast<int>(order.size()); pos++) {
        int inst_idx = order[pos];
        const Instruction& inst = instructions[inst_idx];
        const DAGNode& node = nodes[inst_idx];

        // Consider all predecessor completion times + edge latency
        int earliest_start = pos;  // At least wait for own issue slot

        for (const auto& pred_edge : node.predecessors) {
            int pred_idx = pred_edge.target_node;

            // Predecessor must have been scheduled (before current instruction)
            if (order_position[pred_idx] < pos) {
                int ready_time;
                if (pred_edge.type == DependencyType::RAW) {
                    ready_time = result.start_cycle[pred_idx] + instructions[pred_idx].latency;
                } else {
                    ready_time = result.start_cycle[pred_idx] + 1;
                }
                earliest_start = std::max(earliest_start, ready_time);
            }
        }

        result.start_cycle[inst_idx] = earliest_start;
        result.end_cycle[inst_idx] = earliest_start + inst.latency;
    }

    // Total cycles = completion time of last completed instruction
    result.total_cycles = 0;
    for (int i = 0; i < n; i++) {
        result.total_cycles = std::max(result.total_cycles, result.end_cycle[i]);
    }

    return result;
}

void Scheduler::printComparison(const std::vector<Instruction>& instructions,
                                const std::vector<DAGNode>& nodes,
                                const ScheduleResult& scheduled_result,
                                std::ostream& os) {
    std::vector<int> original_order;
    for (size_t i = 0; i < instructions.size(); i++) {
        original_order.push_back(static_cast<int>(i));
    }

    SimulationResult original_sim = simulateExecution(original_order, instructions, nodes);

    SimulationResult scheduled_sim = simulateExecution(scheduled_result.order, instructions, nodes);

    os << "\n";
    os << "Scheduling Effectiveness Comparison\n\n";

    os << "Original order execution timeline:\n";
    os << "─────────────────────────────────────────────────────────────────\n";
    printTimeline(original_order, instructions, original_sim, os);

    os << "\n";

    os << "Scheduled order execution timeline:\n";
    os << "─────────────────────────────────────────────────────────────────\n";
    printTimeline(scheduled_result.order, instructions, scheduled_sim, os);

    os << "\n";

    os << "Performance comparison:\n";
    os << "─────────────────────────────────────────────────────────────────\n";
    os << "  Original order total cycles: " << original_sim.total_cycles << " cycles\n";
    os << "  Scheduled order total cycles: " << scheduled_sim.total_cycles << " cycles\n";

    int saved = original_sim.total_cycles - scheduled_sim.total_cycles;
    if (saved > 0) {
        double improvement = (static_cast<double>(saved) / original_sim.total_cycles) * 100.0;
        os << "  ─────────────────────────────────────────────────────────────\n";
        os << "  Cycles saved: " << saved << " cycles\n";
        os << "  Performance improvement: " << std::fixed << std::setprecision(1) << improvement
           << "%\n";
        os << "  ─────────────────────────────────────────────────────────────\n";
    } else if (saved < 0) {
        os << "  ─────────────────────────────────────────────────────────────\n";
        os << "  Note: Scheduled order cycles increased by " << (-saved) << " cycles\n";
        os << "  This may be because the original order was already optimal\n";
    } else {
        os << "  ─────────────────────────────────────────────────────────────\n";
        os << "  Same cycle count before and after scheduling, original order was already near "
              "optimal\n";
    }

    os << "\n";
}

void Scheduler::printTimeline(const std::vector<int>& order,
                              const std::vector<Instruction>& instructions,
                              const SimulationResult& sim,
                              std::ostream& os) {
    // Find max cycles to determine timeline width
    int max_cycle = sim.total_cycles;
    int timeline_width = std::min(max_cycle + 1, 40);  // Limit width

    os << "  Instruction" << std::string(14, ' ') << "Timeline (each unit=1 cycle)\n";
    os << "  " << std::string(26, ' ');

    for (int c = 0; c < timeline_width; c++) {
        if (c % 5 == 0) {
            os << c % 10;
        } else {
            os << " ";
        }
    }
    os << "\n";
    os << "  " << std::string(26, ' ') << std::string(timeline_width, '-') << "\n";

    for (int inst_idx : order) {
        const Instruction& inst = instructions[inst_idx];
        int start = sim.start_cycle[inst_idx];
        int end = sim.end_cycle[inst_idx];

        std::string inst_text = inst.raw_text;
        if (inst_text.length() > 20) {
            inst_text = inst_text.substr(0, 17) + "...";
        }

        os << "  [" << std::setw(2) << inst_idx << "] " << std::left << std::setw(20) << inst_text
           << std::right;

        for (int c = 0; c < timeline_width; c++) {
            if (c >= start && c < end) {
                os << "█";
            } else if (c < start) {
                os << "·";
            } else {
                os << " ";
            }
        }

        os << " [" << start << "-" << end << "]\n";
    }

    os << "  " << std::string(26, ' ') << std::string(timeline_width, '-') << "\n";
    os << "  Total execution cycles: " << max_cycle << " cycles\n";
}
