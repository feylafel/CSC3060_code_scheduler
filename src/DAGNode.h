#ifndef DAG_NODE_H
#define DAG_NODE_H

#include <string>
#include <vector>

enum class DependencyType {
    RAW,  // Read After Write (True dependency)
    WAR,  // Write After Read (Anti-dependency)
    WAW   // Write After Write (Output dependency)
};

// Convert dependency type to string (for debug output)
inline std::string dependencyTypeToString(DependencyType type) {
    switch (type) {
        case DependencyType::RAW:
            return "RAW";
        case DependencyType::WAR:
            return "WAR";
        case DependencyType::WAW:
            return "WAW";
    }
    return "UNKNOWN";
}

// Represents a dependency from one node to another
struct DependencyEdge {
    int target_node;      // Target node index
    DependencyType type;  // Dependency type
    std::string reg;      // Involved register

    DependencyEdge(int target, DependencyType t, const std::string& r)
        : target_node(target),
          type(t),
          reg(r) {}
};

struct DAGNode {
    int inst_index;
    std::vector<DependencyEdge> successors;

    std::vector<DependencyEdge> predecessors;

    int unscheduled_predecessors;
    int priority;

    bool scheduled;

    int schedule_cycle;

    DAGNode(int idx = -1)
        : inst_index(idx),
          unscheduled_predecessors(0),
          priority(0),
          scheduled(false),
          schedule_cycle(-1) {}
};

#endif  // DAG_NODE_H
