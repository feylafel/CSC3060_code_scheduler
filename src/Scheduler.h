#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "DAGNode.h"
#include "Instruction.h"

#include <string>
#include <vector>

struct SimulationResult {
    std::vector<int> start_cycle;  // Start cycle for each instruction
    std::vector<int> end_cycle;    // End cycle for each instruction
    int total_cycles;              // Total execution cycles

    SimulationResult() : total_cycles(0) {}
};

struct ScheduleResult {
    std::vector<int> order;  // Instruction order after scheduling (instruction indices)
    int total_cycles;        // Total execution cycles

    ScheduleResult() : total_cycles(0) {}
};

// Tie-breaking policy enumeration
enum class TieBreakingPolicy {
    SMALLER_INDEX,  // Select node with smaller index
    MOST_CHILD,     // Select node with most children (successors)
    LPT             // Slect Node with Longer Processing Time
};

class Scheduler {
public:
    // Input: instruction sequence and dependency graph
    // Output: scheduling result
    ScheduleResult schedule(const std::vector<Instruction>& instructions,
                            std::vector<DAGNode>& nodes,
                            TieBreakingPolicy policy = TieBreakingPolicy::SMALLER_INDEX);

    // Same interface as schedule(), but better performance
    ScheduleResult scheduleFast(const std::vector<Instruction>& instructions,
                                std::vector<DAGNode>& nodes,
                                TieBreakingPolicy policy = TieBreakingPolicy::SMALLER_INDEX,
                                bool verbose = false);

    // Print scheduling result
    void printSchedule(const ScheduleResult& result,
                       const std::vector<Instruction>& instructions,
                       const std::vector<DAGNode>& nodes,
                       std::ostream& os = std::cout);

    // Simulate instruction execution in specified order, return total cycles
    SimulationResult simulateExecution(const std::vector<int>& order,
                                       const std::vector<Instruction>& instructions,
                                       const std::vector<DAGNode>& nodes);

    // Print comparison analysis before and after scheduling
    void printComparison(const std::vector<Instruction>& instructions,
                         const std::vector<DAGNode>& nodes,
                         const ScheduleResult& scheduled_result,
                         std::ostream& os = std::cout);

private:
    // Helper function: print execution timeline
    void printTimeline(const std::vector<int>& order,
                       const std::vector<Instruction>& instructions,
                       const SimulationResult& sim,
                       std::ostream& os);

private:
    // Priority calculation

    // Calculate priority (critical path length) for all nodes
    void computePriorities(const std::vector<Instruction>& instructions,
                           std::vector<DAGNode>& nodes);

    // Recursively calculate critical path length for a single node
    int computeCriticalPath(int nodeIdx,
                            const std::vector<Instruction>& instructions,
                            std::vector<DAGNode>& nodes,
                            std::vector<bool>& visited);

    // Get all ready nodes (all predecessors scheduled)
    std::vector<int> getReadyNodes(const std::vector<DAGNode>& nodes);

    // Select highest priority node from ready nodes
    int selectBestNode(const std::vector<int>& readyNodes,
                       const std::vector<DAGNode>& nodes,
                       const std::vector<Instruction>& instructions,
                       TieBreakingPolicy policy = TieBreakingPolicy::SMALLER_INDEX);

    // Update dependency count of successors after scheduling a node
    void updateSuccessors(int nodeIdx, std::vector<DAGNode>& nodes);
};

#endif  // SCHEDULER_H
