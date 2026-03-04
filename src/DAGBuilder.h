#ifndef DAG_BUILDER_H
#define DAG_BUILDER_H

#include "DAGNode.h"
#include "Instruction.h"

#include <map>
#include <string>
#include <vector>

class DAGBuilder {
public:
    // Build dependency graph from vector of instructions
    // Output: initialized DAG node vector
    std::vector<DAGNode> build(const std::vector<Instruction>& instructions);

    // Print dependency graph (for debugging)
    void printDAG(const std::vector<DAGNode>& nodes,
                  const std::vector<Instruction>& instructions,
                  std::ostream& os = std::cout);

private:
    // Detect RAW dependencies (Read After Write)
    void detectRAW(const std::vector<Instruction>& instructions, std::vector<DAGNode>& nodes);

    // Detect WAR dependencies (Write After Read)
    void detectWAR(const std::vector<Instruction>& instructions, std::vector<DAGNode>& nodes);

    // Detect WAW dependencies (Write After Write)
    void detectWAW(const std::vector<Instruction>& instructions, std::vector<DAGNode>& nodes);

    // Add dependency edge (also update predecessor and successor lists)
    void addEdge(
        std::vector<DAGNode>& nodes, int from, int to, DependencyType type, const std::string& reg);
};

#endif  // DAG_BUILDER_H
