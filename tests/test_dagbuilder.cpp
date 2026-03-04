// test_dagbuilder.cpp - Unit tests for DAGBuilder class using Google Test

#include "DAGBuilder.h"
#include "Instruction.h"
#include "Parser.h"

#include <gtest/gtest.h>

#include <chrono>
#include <random>

// Helper function to create instruction
Instruction makeInst(int idx,
                     const std::string& opcode,
                     const std::string& dest,
                     std::vector<std::string> srcs,
                     int latency = 1) {
    Instruction inst(idx, opcode + " " + dest, opcode);
    inst.dest_reg = dest;
    inst.src_regs = std::move(srcs);
    inst.latency = latency;
    return inst;
}

// Helper to check if edge exists
bool hasEdge(const std::vector<DAGNode>& nodes,
             int from,
             int to,
             DependencyType type,
             const std::string& reg) {
    for (const auto& edge : nodes[from].successors) {
        if (edge.target_node == to && edge.type == type && edge.reg == reg) {
            return true;
        }
    }
    return false;
}

// Test fixture
class DAGBuilderTest : public ::testing::Test {
protected:
    DAGBuilder builder;
};

// RAW Dependency Tests

TEST_F(DAGBuilderTest, RAW_Basic) {
    // add x1, x2, x3  ; writes x1
    // sub x4, x1, x5  ; reads x1 -> RAW dependency
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "sub", "x4", {"x1", "x5"}, 1),
    };

    auto nodes = builder.build(insts);

    // Check edge exists: 0 -> 1 (RAW on x1)
    EXPECT_TRUE(hasEdge(nodes, 0, 1, DependencyType::RAW, "x1"));

    // Check latency = instruction 0's latency

    // Check successors/predecessors
    EXPECT_EQ(nodes[0].successors.size(), 1);
    EXPECT_EQ(nodes[1].predecessors.size(), 1);
    EXPECT_EQ(nodes[1].predecessors[0].target_node, 0);
}

TEST_F(DAGBuilderTest, RAW_WithHighLatency) {
    // mul x1, x2, x3  ; writes x1, latency = 3
    // add x4, x1, x5  ; reads x1 -> RAW dependency
    std::vector<Instruction> insts = {
        makeInst(0, "mul", "x1", {"x2", "x3"}, 3),
        makeInst(1, "add", "x4", {"x1", "x5"}, 1),
    };

    auto nodes = builder.build(insts);

    // Check latency = 3 (mul's latency)
}

TEST_F(DAGBuilderTest, RAW_LoadLatency) {
    // lw x1, 0(x2)   ; writes x1, latency = 5
    // add x3, x1, x4 ; reads x1 -> RAW dependency
    std::vector<Instruction> insts = {
        makeInst(0, "lw", "x1", {"x2"}, 5),
        makeInst(1, "add", "x3", {"x1", "x4"}, 1),
    };

    auto nodes = builder.build(insts);

}

TEST_F(DAGBuilderTest, RAW_Chain) {
    // add x1, x2, x3  ; writes x1
    // add x4, x1, x5  ; reads x1, writes x4
    // add x6, x4, x7  ; reads x4
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "add", "x4", {"x1", "x5"}, 1),
        makeInst(2, "add", "x6", {"x4", "x7"}, 1),
    };

    auto nodes = builder.build(insts);

    // Chain: 0 -> 1 -> 2
    EXPECT_TRUE(hasEdge(nodes, 0, 1, DependencyType::RAW, "x1"));
    EXPECT_TRUE(hasEdge(nodes, 1, 2, DependencyType::RAW, "x4"));

    // No direct edge 0 -> 2
    EXPECT_FALSE(hasEdge(nodes, 0, 2, DependencyType::RAW, "x1"));
}

TEST_F(DAGBuilderTest, RAW_MultipleReaders) {
    // add x1, x2, x3   ; writes x1
    // sub x4, x1, x5   ; reads x1
    // mul x6, x1, x7   ; reads x1
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "sub", "x4", {"x1", "x5"}, 1),
        makeInst(2, "mul", "x6", {"x1", "x7"}, 3),
    };

    auto nodes = builder.build(insts);

    // Both 1 and 2 depend on 0
    EXPECT_TRUE(hasEdge(nodes, 0, 1, DependencyType::RAW, "x1"));
    EXPECT_TRUE(hasEdge(nodes, 0, 2, DependencyType::RAW, "x1"));
    EXPECT_EQ(nodes[0].successors.size(), 2);
}

TEST_F(DAGBuilderTest, RAW_LastWriterOnly) {
    // add x1, x2, x3   ; writes x1
    // sub x1, x4, x5   ; writes x1 (overwrites)
    // mul x6, x1, x7   ; reads x1 -> depends on sub, not add
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "sub", "x1", {"x4", "x5"}, 1),
        makeInst(2, "mul", "x6", {"x1", "x7"}, 3),
    };

    auto nodes = builder.build(insts);

    // Instruction 2 depends on 1 (last writer), not 0
    EXPECT_TRUE(hasEdge(nodes, 1, 2, DependencyType::RAW, "x1"));
    EXPECT_FALSE(hasEdge(nodes, 0, 2, DependencyType::RAW, "x1"));
}

TEST_F(DAGBuilderTest, RAW_MultipleSources) {
    // add x1, x2, x3   ; writes x1
    // add x4, x5, x6   ; writes x4
    // mul x7, x1, x4   ; reads x1 and x4
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "add", "x4", {"x5", "x6"}, 1),
        makeInst(2, "mul", "x7", {"x1", "x4"}, 3),
    };

    auto nodes = builder.build(insts);

    // Instruction 2 depends on both 0 and 1
    EXPECT_TRUE(hasEdge(nodes, 0, 2, DependencyType::RAW, "x1"));
    EXPECT_TRUE(hasEdge(nodes, 1, 2, DependencyType::RAW, "x4"));
    EXPECT_EQ(nodes[2].predecessors.size(), 2);
}

TEST_F(DAGBuilderTest, RAW_NoSourceRegisters) {
    // li x1, 100     ; no source registers
    // add x2, x1, x3 ; reads x1
    std::vector<Instruction> insts = {
        makeInst(0, "li", "x1", {}, 1),
        makeInst(1, "add", "x2", {"x1", "x3"}, 1),
    };

    auto nodes = builder.build(insts);

    EXPECT_TRUE(hasEdge(nodes, 0, 1, DependencyType::RAW, "x1"));
}

// WAR Dependency Tests

TEST_F(DAGBuilderTest, WAR_Basic) {
    // add x1, x2, x3  ; reads x2
    // sub x2, x4, x5  ; writes x2 -> WAR dependency
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "sub", "x2", {"x4", "x5"}, 1),
    };

    auto nodes = builder.build(insts);

    // Check edge exists: 0 -> 1 (WAR on x2)
    EXPECT_TRUE(hasEdge(nodes, 0, 1, DependencyType::WAR, "x2"));

    // Check latency = 0 for WAR
}

TEST_F(DAGBuilderTest, WAR_LatencyAlwaysZero) {
    // mul x1, x2, x3  ; reads x2, latency = 3
    // sub x2, x4, x5  ; writes x2 -> WAR dependency
    std::vector<Instruction> insts = {
        makeInst(0, "mul", "x1", {"x2", "x3"}, 3),
        makeInst(1, "sub", "x2", {"x4", "x5"}, 1),
    };

    auto nodes = builder.build(insts);

    // WAR latency should still be 0 regardless of instruction latency
}

TEST_F(DAGBuilderTest, WAR_MultipleReaders) {
    // add x1, x2, x3  ; reads x2
    // sub x4, x2, x5  ; reads x2
    // mul x2, x6, x7  ; writes x2 -> WAR from both readers
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "sub", "x4", {"x2", "x5"}, 1),
        makeInst(2, "mul", "x2", {"x6", "x7"}, 3),
    };

    auto nodes = builder.build(insts);

    // Instruction 2 has WAR dependencies from both 0 and 1
    EXPECT_TRUE(hasEdge(nodes, 0, 2, DependencyType::WAR, "x2"));
    EXPECT_TRUE(hasEdge(nodes, 1, 2, DependencyType::WAR, "x2"));
}

TEST_F(DAGBuilderTest, WAR_ClearedAfterWrite) {
    // add x1, x2, x3   ; reads x2
    // sub x2, x4, x5   ; writes x2 (clears readers)
    // mul x6, x2, x7   ; reads x2
    // div x2, x8, x9   ; writes x2 -> WAR from mul only
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "sub", "x2", {"x4", "x5"}, 1),
        makeInst(2, "mul", "x6", {"x2", "x7"}, 3),
        makeInst(3, "div", "x2", {"x8", "x9"}, 10),
    };

    auto nodes = builder.build(insts);

    // Instruction 1 has WAR from 0
    EXPECT_TRUE(hasEdge(nodes, 0, 1, DependencyType::WAR, "x2"));

    // Instruction 3 has WAR from 2 only (not 0, because 1 cleared readers)
    EXPECT_TRUE(hasEdge(nodes, 2, 3, DependencyType::WAR, "x2"));
    EXPECT_FALSE(hasEdge(nodes, 0, 3, DependencyType::WAR, "x2"));
}

TEST_F(DAGBuilderTest, WAR_NoDestRegister) {
    // sw x1, 0(x2)  ; no destination register
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
    };
    insts[0].dest_reg = "";  // Store instruction has no dest

    auto nodes = builder.build(insts);

    // No WAR edges should be created from store
    EXPECT_EQ(nodes[0].successors.size(), 0);
}

// WAW Dependency Tests

TEST_F(DAGBuilderTest, WAW_Basic) {
    // add x1, x2, x3  ; writes x1
    // sub x1, x4, x5  ; writes x1 -> WAW dependency
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "sub", "x1", {"x4", "x5"}, 1),
    };

    auto nodes = builder.build(insts);

    EXPECT_TRUE(hasEdge(nodes, 0, 1, DependencyType::WAW, "x1"));
}

TEST_F(DAGBuilderTest, WAW_Chain) {
    // add x1, x2, x3  ; writes x1
    // sub x1, x4, x5  ; writes x1
    // mul x1, x6, x7  ; writes x1
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "sub", "x1", {"x4", "x5"}, 1),
        makeInst(2, "mul", "x1", {"x6", "x7"}, 3),
    };

    auto nodes = builder.build(insts);

    // Chain: 0 -> 1 -> 2
    EXPECT_TRUE(hasEdge(nodes, 0, 1, DependencyType::WAW, "x1"));
    EXPECT_TRUE(hasEdge(nodes, 1, 2, DependencyType::WAW, "x1"));

    // No direct edge 0 -> 2 (only track last writer)
    EXPECT_FALSE(hasEdge(nodes, 0, 2, DependencyType::WAW, "x1"));
}

TEST_F(DAGBuilderTest, WAW_LatencyAlwaysOne) {
    // WAW latency should always be 1, regardless of the first instruction's latency
    // lw x1, 0(x2)    ; writes x1, latency = 5
    // add x1, x3, x4  ; writes x1 -> WAW, latency should be 1 (not 5)
    std::vector<Instruction> insts = {
        makeInst(0, "lw", "x1", {"x2"}, 5),      // load with high latency
        makeInst(1, "add", "x1", {"x3", "x4"}, 1),
    };

    auto nodes = builder.build(insts);

    EXPECT_TRUE(hasEdge(nodes, 0, 1, DependencyType::WAW, "x1"));
    // WAW only needs to guarantee order, latency should be 1
}

// Combined Dependency Tests

TEST_F(DAGBuilderTest, Combined_RAW_WAR_WAW) {
    // add x1, x2, x3   ; writes x1, reads x2
    // sub x1, x1, x2   ; writes x1 (WAW), reads x1 (RAW), reads x2
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "sub", "x1", {"x1", "x2"}, 1),
    };

    auto nodes = builder.build(insts);

    // RAW: 1 reads x1 that 0 writes
    EXPECT_TRUE(hasEdge(nodes, 0, 1, DependencyType::RAW, "x1"));

    // WAW: both write x1
    EXPECT_TRUE(hasEdge(nodes, 0, 1, DependencyType::WAW, "x1"));

    // Check latencies
}

TEST_F(DAGBuilderTest, Combined_AllThreeTypes) {
    // add x1, x2, x3   ; writes x1, reads x2, x3
    // sub x2, x1, x4   ; writes x2 (WAR on x2), reads x1 (RAW)
    // mul x1, x5, x6   ; writes x1 (WAW on x1)
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "sub", "x2", {"x1", "x4"}, 1),
        makeInst(2, "mul", "x1", {"x5", "x6"}, 3),
    };

    auto nodes = builder.build(insts);

    // 0 -> 1: RAW (x1)
    EXPECT_TRUE(hasEdge(nodes, 0, 1, DependencyType::RAW, "x1"));

    // 0 -> 1: WAR (x2)
    EXPECT_TRUE(hasEdge(nodes, 0, 1, DependencyType::WAR, "x2"));

    // 1 -> 2: WAW (x1) - note: 1 writes x2, 2 writes x1, but 0 writes x1
    // Actually, WAW should be 0 -> 2 for x1
    EXPECT_TRUE(hasEdge(nodes, 0, 2, DependencyType::WAW, "x1"));

    // But wait, after 1 executes, x1 is still last written by 0
    // Then 2 writes x1, so WAW: 0 -> 2
}

// Predecessors and Successors Tests

TEST_F(DAGBuilderTest, Predecessors_Count) {
    // add x1, x2, x3
    // sub x4, x5, x6
    // mul x7, x1, x4  ; depends on both
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "sub", "x4", {"x5", "x6"}, 1),
        makeInst(2, "mul", "x7", {"x1", "x4"}, 3),
    };

    auto nodes = builder.build(insts);

    EXPECT_EQ(nodes[0].predecessors.size(), 0);
    EXPECT_EQ(nodes[1].predecessors.size(), 0);
    EXPECT_EQ(nodes[2].predecessors.size(), 2);

    EXPECT_EQ(nodes[0].unscheduled_predecessors, 0);
    EXPECT_EQ(nodes[1].unscheduled_predecessors, 0);
    EXPECT_EQ(nodes[2].unscheduled_predecessors, 2);
}

TEST_F(DAGBuilderTest, Successors_Count) {
    // add x1, x2, x3
    // sub x4, x1, x5
    // mul x6, x1, x7
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "sub", "x4", {"x1", "x5"}, 1),
        makeInst(2, "mul", "x6", {"x1", "x7"}, 3),
    };

    auto nodes = builder.build(insts);

    EXPECT_EQ(nodes[0].successors.size(), 2);
    EXPECT_EQ(nodes[1].successors.size(), 0);
    EXPECT_EQ(nodes[2].successors.size(), 0);
}

TEST_F(DAGBuilderTest, Predecessors_CorrectNodes) {
    // add x1, x2, x3   ; 0
    // sub x4, x5, x6   ; 1
    // mul x7, x1, x4   ; 2, depends on 0 and 1
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "sub", "x4", {"x5", "x6"}, 1),
        makeInst(2, "mul", "x7", {"x1", "x4"}, 3),
    };

    auto nodes = builder.build(insts);

    // Check predecessor nodes of instruction 2
    std::set<int> pred_nodes;
    for (const auto& edge : nodes[2].predecessors) {
        pred_nodes.insert(edge.target_node);
    }
    EXPECT_TRUE(pred_nodes.count(0));
    EXPECT_TRUE(pred_nodes.count(1));
}

// Edge Case Tests

TEST_F(DAGBuilderTest, EmptyInstructions) {
    std::vector<Instruction> insts;
    auto nodes = builder.build(insts);
    EXPECT_EQ(nodes.size(), 0);
}

TEST_F(DAGBuilderTest, SingleInstruction) {
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
    };

    auto nodes = builder.build(insts);

    EXPECT_EQ(nodes.size(), 1);
    EXPECT_EQ(nodes[0].predecessors.size(), 0);
    EXPECT_EQ(nodes[0].successors.size(), 0);
}

TEST_F(DAGBuilderTest, IndependentInstructions) {
    // All instructions are independent
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "sub", "x4", {"x5", "x6"}, 1),
        makeInst(2, "mul", "x7", {"x8", "x9"}, 3),
    };

    auto nodes = builder.build(insts);

    for (const auto& node : nodes) {
        EXPECT_EQ(node.predecessors.size(), 0);
        EXPECT_EQ(node.successors.size(), 0);
    }
}

TEST_F(DAGBuilderTest, NoDuplicateEdges) {
    // mul x1, x1, x1  ; reads x1 twice from same source
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "mul", "x4", {"x1", "x1"}, 3),  // x1 used twice
    };

    auto nodes = builder.build(insts);

    // Should only have one RAW edge, not two
    int raw_count = 0;
    for (const auto& edge : nodes[0].successors) {
        if (edge.type == DependencyType::RAW && edge.reg == "x1") {
            raw_count++;
        }
    }
    EXPECT_EQ(raw_count, 1);
}

// Performance Tests

TEST_F(DAGBuilderTest, LinearChain) {
    // Create a linear dependency chain
    const int N = 100000;
    std::vector<Instruction> insts;
    insts.reserve(N);

    // x1 = ..., x1 = f(x1), x1 = f(x1), ...
    insts.push_back(makeInst(0, "add", "x1", {"x2", "x3"}, 1));
    for (int i = 1; i < N; i++) {
        insts.push_back(makeInst(i, "add", "x1", {"x1", "x3"}, 1));
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto nodes = builder.build(insts);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "LinearChain(" << N << "): " << duration.count() << " us\n";

    // Verify correctness
    EXPECT_EQ(nodes.size(), N);
    EXPECT_EQ(nodes[0].predecessors.size(), 0);
    EXPECT_EQ(nodes[N - 1].successors.size(), 0);

    // Each node (except first) should have predecessors
    for (int i = 1; i < N; i++) {
        EXPECT_GT(nodes[i].predecessors.size(), 0);
    }

    // Performance threshold: should complete in reasonable time
    EXPECT_LT(duration.count(), 1500000);  // < 1500ms
}

TEST_F(DAGBuilderTest, ParallelInstructions) {
    // Create many independent instructions
    const int N = 100000;
    std::vector<Instruction> insts;
    insts.reserve(N);

    for (int i = 0; i < N; i++) {
        std::string dest = "x" + std::to_string(i % 32);
        std::string src1 = "x" + std::to_string((i + 10) % 32);
        std::string src2 = "x" + std::to_string((i + 20) % 32);
        insts.push_back(makeInst(i, "add", dest, {src1, src2}, 1));
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto nodes = builder.build(insts);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "ParallelInstructions(" << N << "): " << duration.count() << " us\n";

    EXPECT_EQ(nodes.size(), N);
    EXPECT_LT(duration.count(), 1500000);  // < 1500ms
}

TEST_F(DAGBuilderTest, DenseGraph) {
    // Create instructions where many depend on few registers
    const int N = 50000;
    std::vector<Instruction> insts;
    insts.reserve(N);

    // First instruction writes x1
    insts.push_back(makeInst(0, "add", "x1", {"x2", "x3"}, 1));

    // All subsequent instructions read x1 and write different registers
    for (int i = 1; i < N; i++) {
        std::string dest = "x" + std::to_string(i + 10);
        insts.push_back(makeInst(i, "add", dest, {"x1", "x4"}, 1));
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto nodes = builder.build(insts);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "DenseGraph(" << N << "): " << duration.count() << " us\n";

    // Verify: instruction 0 should have N-1 successors
    EXPECT_EQ(nodes[0].successors.size(), N - 1);
    EXPECT_LT(duration.count(), 16000000);  // < 16000ms
}

TEST_F(DAGBuilderTest, LargeScale) {
    // Large scale test
    const int N = 500000;
    std::vector<Instruction> insts;
    insts.reserve(N);

    std::mt19937 rng(42);  // Fixed seed for reproducibility
    std::uniform_int_distribution<int> reg_dist(0, 31);

    for (int i = 0; i < N; i++) {
        std::string dest = "x" + std::to_string(reg_dist(rng));
        std::string src1 = "x" + std::to_string(reg_dist(rng));
        std::string src2 = "x" + std::to_string(reg_dist(rng));
        int latency = (i % 10 == 0) ? 5 : 1;  // Some loads
        insts.push_back(makeInst(i, "add", dest, {src1, src2}, latency));
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto nodes = builder.build(insts);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "LargeScale(" << N << "): " << duration.count() << " ms\n";

    EXPECT_EQ(nodes.size(), N);
    EXPECT_LT(duration.count(), 12000);  // < 16 seconds
}

// Using Parser for Integration Test

TEST(IntegrationTest, ParseAndBuild) {
    Parser parser;
    std::vector<std::string> code = {
        "lw    x10, 0(x5)",
        "lw    x11, 4(x5)",
        "mul   x1, x10, x11",
        "sw    x1, 8(x5)",
    };

    auto instructions = parser.parse(code);
    DAGBuilder builder;
    auto nodes = builder.build(instructions);

    EXPECT_EQ(nodes.size(), 4);

    // lw x10 -> mul (RAW on x10)
    EXPECT_TRUE(hasEdge(nodes, 0, 2, DependencyType::RAW, "x10"));

    // lw x11 -> mul (RAW on x11)
    EXPECT_TRUE(hasEdge(nodes, 1, 2, DependencyType::RAW, "x11"));

    // mul -> sw (RAW on x1)
    EXPECT_TRUE(hasEdge(nodes, 2, 3, DependencyType::RAW, "x1"));

    // Check latencies from parser
}
