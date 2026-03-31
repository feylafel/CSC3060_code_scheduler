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
    // throw std::runtime_error("schedule not implemented");
    if (instructions.empty() || nodes.empty()) {
        return result;
    }

    // init the node state
    for (size_t i = 0; i < nodes.size(); i++) {
        DAGNode& n = nodes[i];
        n.scheduled = false;
        n.schedule_cycle = -1;
        n.unscheduled_predecessors = static_cast<int>(n.predecessors.size());
    }

    // compute priorities
    computePriorities(instructions, nodes);

    using FinishEvent = std::pair<int, int>;
    std::priority_queue<FinishEvent, std::vector<FinishEvent>, std::greater<FinishEvent>> finishing;

    std::vector<int> issue_time(instructions.size(), -1);
    int t = 0;

    // scheduling loop
    while (result.order.size() < instructions.size()) {
        while (!finishing.empty() && finishing.top().first <= t) {  // retire finishe instruction
            const int u = finishing.top().second;
            finishing.pop();
            for (const auto& edge : nodes[u].successors) {
                if (edge.type == DependencyType::RAW) {
                    nodes[edge.target_node].unscheduled_predecessors--;
                }
            }
        }

        std::vector<int> ready = getReadyNodes(nodes);

        if (!ready.empty()) {  // if have ready instructions
            const int k = selectBestNode(ready, nodes, instructions, policy);
            issue_time[k] = t;
            nodes[k].scheduled = true;
            nodes[k].schedule_cycle = static_cast<int>(result.order.size());
            result.order.push_back(k);

            for (const auto& edge : nodes[k].successors) {
                if (edge.type != DependencyType::RAW) {
                    nodes[edge.target_node].unscheduled_predecessors--;
                }
            }
            finishing.push({t + instructions[k].latency, k});
            t++;
        } else {  // if no ready instryctions
            if (finishing.empty()) {
                break;
            }
            t = finishing.top().first;
        }
    }

    // compute the total cycles here
    int total = 0;
    for (size_t i = 0; i < instructions.size(); i++) {
        total = std::max(total, issue_time[static_cast<int>(i)] + instructions[i].latency);
    }
    result.total_cycles = total;
    return result;
}

ScheduleResult Scheduler::scheduleFast(const std::vector<Instruction>& instructions,
                                       std::vector<DAGNode>& nodes,
                                       TieBreakingPolicy policy,
                                       bool verbose) {
    (void)verbose;  // not used
    ScheduleResult result;
    if (instructions.empty() || nodes.empty()) {
        return result;
    }

    // reset node state for a fresh schedule run
    for (size_t i = 0; i < nodes.size(); i++) {
        nodes[i].scheduled = false;
        nodes[i].schedule_cycle = -1;
        nodes[i].unscheduled_predecessors = static_cast<int>(nodes[i].predecessors.size());
    }

    computePriorities(instructions, nodes);

    // ready set use heap (no scanning all nodes each time)
    // Use selectBestNode() for all tie-breaking logic.
    auto readyCmp = [&](int a, int b) -> bool {
        // For a std::priority_queue with this comparator, "top" should be the
        // element that wins if we only compare (a,b).
        const std::vector<int> candidates = {a, b};
        const int best = selectBestNode(candidates, nodes, instructions, policy);

        // Return true if a should come after b in the heap (i.e., b is better).
        return best == b;
    };
    std::priority_queue<int, std::vector<int>, decltype(readyCmp)> ready(readyCmp);

    // inflight completion min-heap for RAW release: (finish_time, node_idx)
    using FinishEvent = std::pair<int, int>;
    std::priority_queue<FinishEvent, std::vector<FinishEvent>, std::greater<FinishEvent>> finishing;

    // when to issue each instruction
    std::vector<int> issue_time(instructions.size(), -1);

    // init the ready heap with nodes that have no predecessors
    for (size_t i = 0; i < nodes.size(); i++) {
        if (nodes[i].unscheduled_predecessors == 0) {
            ready.push(static_cast<int>(i));
        }
    }

    int t = 0;
    while (result.order.size() < instructions.size()) {
        // release RAW successors whose predecessors have finished by time t
        while (!finishing.empty() && finishing.top().first <= t) {
            const int u = finishing.top().second;
            finishing.pop();

            for (const auto& edge : nodes[u].successors) {
                if (edge.type != DependencyType::RAW) {
                    continue;
                }

                const int v = edge.target_node;
                nodes[v].unscheduled_predecessors--;
                if (nodes[v].unscheduled_predecessors == 0 && !nodes[v].scheduled) {
                    ready.push(v);
                }
            }
        }

        if (!ready.empty()) {
            const int k = ready.top();
            ready.pop();

            // below is just double-check before issuing
            if (nodes[k].scheduled || nodes[k].unscheduled_predecessors != 0) {
                continue;
            }

            issue_time[k] = t;
            nodes[k].scheduled = true;
            nodes[k].schedule_cycle = static_cast<int>(result.order.size());
            result.order.push_back(k);

            // release WAR/WAW successors immediately at issue
            for (const auto& edge : nodes[k].successors) {
                if (edge.type == DependencyType::RAW) {
                    continue;
                }

                const int v = edge.target_node;
                nodes[v].unscheduled_predecessors--;
                if (nodes[v].unscheduled_predecessors == 0 && !nodes[v].scheduled) {
                    ready.push(v);
                }
            }

            // raW successors can only be released when this node finishes
            finishing.push({t + instructions[k].latency, k});
            t++;
            continue;
        }

        // no ready nodes: jump to the next finishing event (RAW release time)
        if (finishing.empty()) {
            break;
        }
        t = finishing.top().first;
    }

    // compute total cycles
    int total = 0;
    for (size_t i = 0; i < instructions.size(); i++) {
        if (issue_time[i] >= 0) {
            total = std::max(total, issue_time[i] + instructions[i].latency);
        }
    }
    result.total_cycles = total;
    return result;
}

void Scheduler::computePriorities(const std::vector<Instruction>& instructions,
                                  std::vector<DAGNode>& nodes) {
    // TODO: Implement this function
    // throw std::runtime_error("computeCriticalPath not implemented");
    // return 0;
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
    if (visited[static_cast<size_t>(nodeIdx)]) {
        return nodes[static_cast<size_t>(nodeIdx)].priority;
    }
    visited[static_cast<size_t>(nodeIdx)] = true;

    const Instruction& inst = instructions[static_cast<size_t>(nodeIdx)];
    int best = inst.latency;

    for (const auto& edge : nodes[static_cast<size_t>(nodeIdx)].successors) {
        const int w = (edge.type == DependencyType::RAW) ? inst.latency : 1;
        const int sub =
            computeCriticalPath(edge.target_node, instructions, nodes, visited);  // sub is subpath
        best = std::max(best, w + sub);
    }

    nodes[static_cast<size_t>(nodeIdx)].priority = best;
    return best;
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
            // tiebreaking based on policy
            bool shouldReplace = false;

            switch (policy) {
                case TieBreakingPolicy::SMALLER_INDEX:
                    // pick node with smaller original index
                    shouldReplace = (nodeIdx < bestNode);
                    break;
                case TieBreakingPolicy::MOST_CHILD: {
                    // prefer node with more successors -> more immediate available work
                    const size_t bestChildCount = nodes[bestNode].successors.size();
                    const size_t currentChildCount = nodes[nodeIdx].successors.size();

                    if (currentChildCount > bestChildCount) {
                        shouldReplace = true;
                    } else if (currentChildCount == bestChildCount) {
                        // if still tied, fall back to smaller index.
                        shouldReplace = (nodeIdx < bestNode);
                    }
                    break;
                }
                case TieBreakingPolicy::LPT: {
                    // LPT = longer processing time -> higher latency first.
                    const int bestLatency = instructions[bestNode].latency;
                    const int currentLatency = instructions[nodeIdx].latency;

                    if (currentLatency > bestLatency) {
                        shouldReplace = true;
                    } else if (currentLatency == bestLatency) {
                        // if still tied use smaller index.
                        shouldReplace = (nodeIdx < bestNode);
                    }
                    break;
                }
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
