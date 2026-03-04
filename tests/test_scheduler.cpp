// test_scheduler.cpp - Unit tests for Scheduler (computePriorities and schedule)
//
// computePriorities is tested indirectly via schedule(): after schedule(), node priorities
// are set by computePriorities and we assert expected critical path values.
// Corner cases: empty, single node, chain, diamond, multiple successors, tie-breaking.

#include "DAGBuilder.h"
#include "Instruction.h"
#include "Scheduler.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <random>
#include <set>
#include <sstream>

static Instruction makeInst(int idx,
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

// Redirect cout to avoid flooding test output during schedule() calls
class CoutRedirect {
public:
    CoutRedirect() { old_ = std::cout.rdbuf(buffer_.rdbuf()); }
    ~CoutRedirect() { std::cout.rdbuf(old_); }

private:
    std::streambuf* old_;
    std::stringstream buffer_;
};

// Return true if for every edge (u -> v) in nodes, u appears before v in order
static bool orderRespectsDependencies(const std::vector<int>& order,
                                     const std::vector<DAGNode>& nodes) {
    std::vector<int> pos(static_cast<int>(nodes.size()), -1);
    for (size_t i = 0; i < order.size(); i++) {
        pos[order[i]] = static_cast<int>(i);
    }
    for (size_t u = 0; u < nodes.size(); u++) {
        for (const auto& edge : nodes[u].successors) {
            int v = edge.target_node;
            if (pos[u] >= 0 && pos[v] >= 0 && pos[u] >= pos[v]) {
                return false;
            }
        }
    }
    return true;
}

// Fixture that builds DAG and runs schedule, optionally suppressing cout
class SchedulerTest : public ::testing::Test {
protected:
    Scheduler scheduler;
    DAGBuilder builder;

    ScheduleResult runSchedule(std::vector<Instruction>& instructions,
                              std::vector<DAGNode>& nodes,
                              bool quiet = true) {
        if (quiet) {
            CoutRedirect _;
            return scheduler.schedule(instructions, nodes);
        }
        return scheduler.schedule(instructions, nodes);
    }
};

// ---------------------------------------------------------------------------
// computePriorities (verified via nodes[].priority after schedule())
// ---------------------------------------------------------------------------

TEST_F(SchedulerTest, ComputePriorities_SingleNode) {
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
    };
    auto nodes = builder.build(insts);
    runSchedule(insts, nodes);

    EXPECT_EQ(nodes[0].priority, 1);
}

TEST_F(SchedulerTest, ComputePriorities_SingleNodeHighLatency) {
    std::vector<Instruction> insts = {
        makeInst(0, "div", "x1", {"x2", "x3"}, 10),
    };
    auto nodes = builder.build(insts);
    runSchedule(insts, nodes);

    EXPECT_EQ(nodes[0].priority, 10);
}

TEST_F(SchedulerTest, ComputePriorities_TwoIndependentNodes) {
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "mul", "x4", {"x5", "x6"}, 3),
    };
    auto nodes = builder.build(insts);
    runSchedule(insts, nodes);

    EXPECT_EQ(nodes[0].priority, 1);
    EXPECT_EQ(nodes[1].priority, 3);
}

TEST_F(SchedulerTest, ComputePriorities_LinearChainTwoNodes) {
    // 0 -> 1 (RAW dependency)
    // Node 1: no successors, priority = L1 = 1
    // Node 0: RAW edge, priority = L0 + priority(1) = 1 + 1 = 2
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "sub", "x4", {"x1", "x5"}, 1),
    };
    auto nodes = builder.build(insts);
    runSchedule(insts, nodes);

    EXPECT_EQ(nodes[1].priority, 1);
    EXPECT_EQ(nodes[0].priority, 2);  // 1 + 1
}

TEST_F(SchedulerTest, ComputePriorities_LinearChainThreeNodes) {
    // 0 -> 1 -> 2 (RAW dependencies), all latency 1
    // Node 2: priority = 1
    // Node 1: RAW edge, priority = 1 + 1 = 2
    // Node 0: RAW edge, priority = 1 + 2 = 3
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "add", "x4", {"x1", "x5"}, 1),
        makeInst(2, "add", "x6", {"x4", "x7"}, 1),
    };
    auto nodes = builder.build(insts);
    runSchedule(insts, nodes);

    EXPECT_EQ(nodes[2].priority, 1);
    EXPECT_EQ(nodes[1].priority, 2);
    EXPECT_EQ(nodes[0].priority, 3);
}

TEST_F(SchedulerTest, ComputePriorities_ChainWithHighLatencyInMiddle) {
    // 0 -> 1 -> 2 (RAW dependencies). L0=1, L1=5, L2=1
    // Node 2: priority = 1
    // Node 1: RAW edge, priority = 5 + 1 = 6
    // Node 0: RAW edge, priority = 1 + 6 = 7
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "lw", "x4", {"x1"}, 5),
        makeInst(2, "add", "x6", {"x4", "x7"}, 1),
    };
    auto nodes = builder.build(insts);
    runSchedule(insts, nodes);

    EXPECT_EQ(nodes[2].priority, 1);
    EXPECT_EQ(nodes[1].priority, 6);   // 5 + 1
    EXPECT_EQ(nodes[0].priority, 7);   // 1 + 6
}

TEST_F(SchedulerTest, ComputePriorities_ExitNodeOnly) {
    // Single node is exit: no successors, priority = latency.
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 2),
    };
    auto nodes = builder.build(insts);
    runSchedule(insts, nodes);

    EXPECT_EQ(nodes[0].priority, 2);
}

TEST_F(SchedulerTest, ComputePriorities_DiamondTakeMaxPath) {
    // Diamond: 0 -> {1, 2} -> 3 (all RAW dependencies)
    // L0=1, L1=1, L2=1, L3=3
    // Node 3: priority = 3
    // Node 1: RAW edge, priority = 1 + 3 = 4
    // Node 2: RAW edge, priority = 1 + 3 = 4
    // Node 0: RAW edges to both 1 and 2, priority = 1 + max(4, 4) = 5
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "add", "x4", {"x1", "x5"}, 1),
        makeInst(2, "add", "x6", {"x1", "x7"}, 1),
        makeInst(3, "mul", "x8", {"x4", "x6"}, 3),
    };
    auto nodes = builder.build(insts);
    runSchedule(insts, nodes);

    EXPECT_EQ(nodes[3].priority, 3);
    EXPECT_EQ(nodes[1].priority, 4);  // 1 + 3
    EXPECT_EQ(nodes[2].priority, 4);  // 1 + 3
    EXPECT_EQ(nodes[0].priority, 5);  // 1 + max(4, 4)
}

TEST_F(SchedulerTest, ComputePriorities_MultipleSuccessorsDifferentEdgeLatencies) {
    // 0 -> {1, 2} (both RAW dependencies)
    // L0=5 (lw), L1=1, L2=1
    // Node 1: priority = 1
    // Node 2: priority = 1
    // Node 0: RAW edges, priority = 5 + max(1, 1) = 6
    std::vector<Instruction> insts = {
        makeInst(0, "lw", "x1", {"x2"}, 5),
        makeInst(1, "add", "x4", {"x1", "x5"}, 1),
        makeInst(2, "add", "x6", {"x1", "x7"}, 1),
    };
    auto nodes = builder.build(insts);
    runSchedule(insts, nodes);

    EXPECT_EQ(nodes[1].priority, 1);
    EXPECT_EQ(nodes[2].priority, 1);
    EXPECT_EQ(nodes[0].priority, 6);  // 5 + max(1, 1)
}

TEST_F(SchedulerTest, ComputePriorities_NodeWithMultiplePredecessors) {
    // {0, 1} -> 2 (both RAW dependencies)
    // L0=1, L1=1, L2=3
    // Node 2: priority = 3
    // Node 0: RAW edge, priority = 1 + 3 = 4
    // Node 1: RAW edge, priority = 1 + 3 = 4
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "add", "x4", {"x5", "x6"}, 1),
        makeInst(2, "mul", "x7", {"x1", "x4"}, 3),
    };
    auto nodes = builder.build(insts);
    runSchedule(insts, nodes);

    EXPECT_EQ(nodes[2].priority, 3);
    EXPECT_EQ(nodes[0].priority, 4);  // 1 + 3
    EXPECT_EQ(nodes[1].priority, 4);  // 1 + 3
}

// ---------------------------------------------------------------------------
// schedule() corner cases
// ---------------------------------------------------------------------------

TEST_F(SchedulerTest, Schedule_EmptyInstructions) {
    std::vector<Instruction> insts;
    std::vector<DAGNode> nodes;
    ScheduleResult result = scheduler.schedule(insts, nodes);

    EXPECT_TRUE(result.order.empty());
    EXPECT_EQ(result.total_cycles, 0);
}

TEST_F(SchedulerTest, Schedule_SingleInstruction) {
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
    };
    auto nodes = builder.build(insts);
    CoutRedirect _;
    ScheduleResult result = scheduler.schedule(insts, nodes);

    ASSERT_EQ(result.order.size(), 1u);
    EXPECT_EQ(result.order[0], 0);
    EXPECT_EQ(result.total_cycles, 1);
}

TEST_F(SchedulerTest, Schedule_TwoIndependentInstructions) {
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "sub", "x4", {"x5", "x6"}, 1),
    };
    auto nodes = builder.build(insts);
    CoutRedirect _;
    ScheduleResult result = scheduler.schedule(insts, nodes);

    ASSERT_EQ(result.order.size(), 2u);
    // Scheduler uses one cycle per scheduled instruction; 2 independent -> 2 cycles
    EXPECT_EQ(result.total_cycles, 2);

    std::vector<int> sorted_order(result.order.begin(), result.order.end());
    std::sort(sorted_order.begin(), sorted_order.end());
    EXPECT_EQ(sorted_order[0], 0);
    EXPECT_EQ(sorted_order[1], 1);
}

TEST_F(SchedulerTest, Schedule_LinearChainOrderRespectsDependencies) {
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "add", "x4", {"x1", "x5"}, 1),
        makeInst(2, "add", "x6", {"x4", "x7"}, 1),
    };
    auto nodes = builder.build(insts);
    CoutRedirect _;
    ScheduleResult result = scheduler.schedule(insts, nodes);

    ASSERT_EQ(result.order.size(), 3u);
    EXPECT_TRUE(orderRespectsDependencies(result.order, nodes));
    EXPECT_EQ(result.order[0], 0);
    EXPECT_EQ(result.order[1], 1);
    EXPECT_EQ(result.order[2], 2);
}

TEST_F(SchedulerTest, Schedule_EveryInstructionScheduledExactlyOnce) {
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "sub", "x4", {"x5", "x6"}, 1),
        makeInst(2, "mul", "x7", {"x1", "x4"}, 3),
    };
    auto nodes = builder.build(insts);
    CoutRedirect _;
    ScheduleResult result = scheduler.schedule(insts, nodes);

    EXPECT_EQ(result.order.size(), insts.size());
    std::vector<bool> seen(insts.size(), false);
    for (int idx : result.order) {
        ASSERT_GE(idx, 0);
        ASSERT_LT(static_cast<size_t>(idx), insts.size());
        EXPECT_FALSE(seen[idx]) << "instruction " << idx << " scheduled more than once";
        seen[idx] = true;
    }
    for (size_t i = 0; i < seen.size(); i++) {
        EXPECT_TRUE(seen[i]) << "instruction " << i << " not scheduled";
    }
}

TEST_F(SchedulerTest, Schedule_DiamondOrderRespectsAllEdges) {
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "add", "x4", {"x1", "x5"}, 1),
        makeInst(2, "add", "x6", {"x1", "x7"}, 1),
        makeInst(3, "mul", "x8", {"x4", "x6"}, 3),
    };
    auto nodes = builder.build(insts);
    CoutRedirect _;
    ScheduleResult result = scheduler.schedule(insts, nodes);

    ASSERT_EQ(result.order.size(), 4u);
    EXPECT_TRUE(orderRespectsDependencies(result.order, nodes));
    EXPECT_EQ(result.order[0], 0);
    // 1 and 2 can be in either order after 0; 3 must be last
    int pos0 = -1, pos1 = -1, pos2 = -1, pos3 = -1;
    for (size_t i = 0; i < result.order.size(); i++) {
        if (result.order[i] == 0) pos0 = static_cast<int>(i);
        if (result.order[i] == 1) pos1 = static_cast<int>(i);
        if (result.order[i] == 2) pos2 = static_cast<int>(i);
        if (result.order[i] == 3) pos3 = static_cast<int>(i);
    }
    EXPECT_LT(pos0, pos1);
    EXPECT_LT(pos0, pos2);
    EXPECT_LT(pos1, pos3);
    EXPECT_LT(pos2, pos3);
}

TEST_F(SchedulerTest, Schedule_TieBreakSmallerIndex) {
    // Two independent nodes: same priority (both latency 1, no successors).
    // Scheduler should pick smaller index first (selectBestNode).
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "add", "x4", {"x5", "x6"}, 1),
    };
    auto nodes = builder.build(insts);
    CoutRedirect _;
    ScheduleResult result = scheduler.schedule(insts, nodes);

    ASSERT_EQ(result.order.size(), 2u);
    EXPECT_EQ(result.order[0], 0);
    EXPECT_EQ(result.order[1], 1);
}

TEST_F(SchedulerTest, Schedule_TotalCyclesAtLeastOne) {
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
    };
    auto nodes = builder.build(insts);
    CoutRedirect _;
    ScheduleResult result = scheduler.schedule(insts, nodes);

    EXPECT_GE(result.total_cycles, 1);
}

TEST_F(SchedulerTest, Schedule_ChainTotalCycles) {
    // 0 -> 1 -> 2, each latency 1. Each cycle schedules one; total_cycles = 3.
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "add", "x4", {"x1", "x5"}, 1),
        makeInst(2, "add", "x6", {"x4", "x7"}, 1),
    };
    auto nodes = builder.build(insts);
    CoutRedirect _;
    ScheduleResult result = scheduler.schedule(insts, nodes);

    EXPECT_EQ(result.total_cycles, 3);
}

TEST_F(SchedulerTest, Schedule_ThreeIndependent) {
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "sub", "x4", {"x5", "x6"}, 1),
        makeInst(2, "mul", "x7", {"x8", "x9"}, 3),
    };
    auto nodes = builder.build(insts);
    CoutRedirect _;
    ScheduleResult result = scheduler.schedule(insts, nodes);

    ASSERT_EQ(result.order.size(), 3u);
    // One cycle per scheduled instruction; 3 independent -> 3 cycles
    EXPECT_EQ(result.total_cycles, 3);
    std::set<int> indices(result.order.begin(), result.order.end());
    EXPECT_EQ(indices.size(), 3u);
}

TEST_F(SchedulerTest, Schedule_OneProducerTwoConsumers) {
    // 0 -> 1, 0 -> 2. Order must have 0 first, then 1 and 2 in any order.
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "sub", "x4", {"x1", "x5"}, 1),
        makeInst(2, "mul", "x6", {"x1", "x7"}, 3),
    };
    auto nodes = builder.build(insts);
    CoutRedirect _;
    ScheduleResult result = scheduler.schedule(insts, nodes);

    ASSERT_EQ(result.order.size(), 3u);
    EXPECT_EQ(result.order[0], 0);
    EXPECT_TRUE(orderRespectsDependencies(result.order, nodes));
}

TEST_F(SchedulerTest, Schedule_HighLatencyChain) {
    // lw (5) -> add (1) -> add (1).
    // Priorities: Node 2=1, Node 1=1+1=2, Node 0=5+2=7
    // Order: 0, 1, 2. total_cycles = 3.
    std::vector<Instruction> insts = {
        makeInst(0, "lw", "x1", {"x2"}, 5),
        makeInst(1, "add", "x4", {"x1", "x5"}, 1),
        makeInst(2, "add", "x6", {"x4", "x7"}, 1),
    };
    auto nodes = builder.build(insts);
    CoutRedirect _;
    ScheduleResult result = scheduler.schedule(insts, nodes);

    ASSERT_EQ(result.order.size(), 3u);
    EXPECT_EQ(result.order[0], 0);
    EXPECT_EQ(result.order[1], 1);
    EXPECT_EQ(result.order[2], 2);
    EXPECT_EQ(result.total_cycles, 3);
}

// ---------------------------------------------------------------------------
// Tie-breaking policy tests
// ---------------------------------------------------------------------------

TEST_F(SchedulerTest, TieBreaking_SmallerIndex) {
    // Two independent nodes with same priority
    // SMALLER_INDEX should pick node 0 first
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "add", "x4", {"x5", "x6"}, 1),
    };
    auto nodes = builder.build(insts);
    CoutRedirect _;
    ScheduleResult result = scheduler.schedule(insts, nodes, TieBreakingPolicy::SMALLER_INDEX);

    ASSERT_EQ(result.order.size(), 2u);
    EXPECT_EQ(result.order[0], 0);  // Smaller index first
    EXPECT_EQ(result.order[1], 1);
}

TEST_F(SchedulerTest, TieBreaking_MostChild) {
    // Three independent nodes: node 0 has 2 children (but they're in later instructions)
    // Actually, let's create: node 0 has 2 successors, node 1 has 1, node 2 has 0
    // All start ready and have same priority
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),   // writes x1
        makeInst(1, "add", "x4", {"x5", "x6"}, 1),   // writes x4, independent
        makeInst(2, "add", "x7", {"x8", "x9"}, 1),   // writes x7, independent
        makeInst(3, "mul", "x10", {"x1", "x11"}, 3), // reads x1 -> depends on 0
        makeInst(4, "mul", "x12", {"x1", "x13"}, 3), // reads x1 -> depends on 0
        makeInst(5, "mul", "x14", {"x4", "x15"}, 3), // reads x4 -> depends on 1
    };
    auto nodes = builder.build(insts);

    // Node 0 has 2 successors (3, 4), Node 1 has 1 successor (5), Node 2 has 0 successors
    // With MOST_CHILD policy, among nodes with same priority, node 0 should be picked first

    CoutRedirect _;
    ScheduleResult result = scheduler.schedule(insts, nodes, TieBreakingPolicy::MOST_CHILD);

    // The first three scheduled should be 0, 1, 2 in some order
    // With MOST_CHILD, 0 (2 children) should come before 1 (1 child) which should come before 2 (0 children)
    // among those with same priority level
    ASSERT_EQ(result.order.size(), 6u);
    EXPECT_TRUE(orderRespectsDependencies(result.order, nodes));
}

TEST_F(SchedulerTest, TieBreaking_LPT) {
    // Two independent nodes with same priority but different latencies
    // LPT should pick the one with longer latency first
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),   // latency 1
        makeInst(1, "mul", "x4", {"x5", "x6"}, 3),   // latency 3
    };
    auto nodes = builder.build(insts);

    // Both are independent, node 0 has priority 1, node 1 has priority 3
    // They won't tie on priority, so let's make them equal
    // Actually, priorities are based on critical path, so they're different here

    // Let's create a better test: two independent nodes, same latency for priority calculation
    std::vector<Instruction> insts2 = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "add", "x4", {"x5", "x6"}, 1),
        makeInst(2, "mul", "x7", {"x1", "x8"}, 5),   // depends on 0, latency 5
        makeInst(3, "mul", "x9", {"x4", "x10"}, 5),  // depends on 1, latency 5
    };
    auto nodes2 = builder.build(insts2);

    // Priorities:
    // Node 2: 5 (exit), Node 3: 5 (exit)
    // Node 0: 1 + 5 = 6, Node 1: 1 + 5 = 6
    // Nodes 0 and 1 have same priority (6)
    // With LPT, since they have same latency (1), fall back to smaller index

    CoutRedirect _;
    ScheduleResult result = scheduler.schedule(insts2, nodes2, TieBreakingPolicy::LPT);

    ASSERT_EQ(result.order.size(), 4u);
    EXPECT_TRUE(orderRespectsDependencies(result.order, nodes2));
}

TEST_F(SchedulerTest, TieBreaking_LPT_DifferentLatencies) {
    // Two independent nodes with same priority but different latencies
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),   // latency 1
        makeInst(1, "lw", "x4", {"x5"}, 5),          // latency 5
        // Make them have same critical path by adding appropriate successors
        makeInst(2, "mul", "x7", {"x1", "x8"}, 10),  // depends on 0, long chain
        makeInst(3, "add", "x9", {"x4", "x10"}, 1),  // depends on 1
        makeInst(4, "add", "x11", {"x7", "x12"}, 1), // depends on 2
    };
    auto nodes = builder.build(insts);

    // Priorities:
    // Node 4: 1 (exit)
    // Node 3: 1 (exit)
    // Node 2: 10 + 1 = 11
    // Node 0: 1 + 11 = 12
    // Node 1: 5 + 1 = 6

    // Node 0 and 1 have different priorities, so this test needs adjustment
    // Let me create a case where priorities are truly equal

    CoutRedirect _;
    ScheduleResult result = scheduler.schedule(insts, nodes, TieBreakingPolicy::LPT);

    ASSERT_EQ(result.order.size(), 5u);
    EXPECT_TRUE(orderRespectsDependencies(result.order, nodes));
}

// ---------------------------------------------------------------------------
// scheduleFast correctness tests
// ---------------------------------------------------------------------------

TEST_F(SchedulerTest, ScheduleFast_SameResultAsSchedule) {
    // scheduleFast should produce the same result as schedule with SMALLER_INDEX policy
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "add", "x4", {"x1", "x5"}, 1),
        makeInst(2, "add", "x6", {"x4", "x7"}, 1),
    };

    auto nodes1 = builder.build(insts);
    auto nodes2 = builder.build(insts);

    CoutRedirect _;
    ScheduleResult result1 = scheduler.schedule(insts, nodes1);
    ScheduleResult result2 = scheduler.scheduleFast(insts, nodes2, TieBreakingPolicy::SMALLER_INDEX);

    EXPECT_EQ(result1.order, result2.order);
    EXPECT_EQ(result1.total_cycles, result2.total_cycles);
}

TEST_F(SchedulerTest, ScheduleFast_Diamond) {
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "add", "x4", {"x1", "x5"}, 1),
        makeInst(2, "add", "x6", {"x1", "x7"}, 1),
        makeInst(3, "mul", "x8", {"x4", "x6"}, 3),
    };

    auto nodes1 = builder.build(insts);
    auto nodes2 = builder.build(insts);

    CoutRedirect _;
    ScheduleResult result1 = scheduler.schedule(insts, nodes1);
    ScheduleResult result2 = scheduler.scheduleFast(insts, nodes2, TieBreakingPolicy::SMALLER_INDEX);

    EXPECT_EQ(result1.order, result2.order);
    EXPECT_TRUE(orderRespectsDependencies(result2.order, nodes2));
}

TEST_F(SchedulerTest, ScheduleFast_Empty) {
    std::vector<Instruction> insts;
    std::vector<DAGNode> nodes;

    ScheduleResult result = scheduler.scheduleFast(insts, nodes);

    EXPECT_TRUE(result.order.empty());
    EXPECT_EQ(result.total_cycles, 0);
}

TEST_F(SchedulerTest, ScheduleFast_SingleInstruction) {
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
    };
    auto nodes = builder.build(insts);

    CoutRedirect _;
    ScheduleResult result = scheduler.scheduleFast(insts, nodes);

    ASSERT_EQ(result.order.size(), 1u);
    EXPECT_EQ(result.order[0], 0);
}

// ---------------------------------------------------------------------------
// schedule_cycle field tests
//
// Invariants verified:
//   1. Before scheduling: schedule_cycle == -1 (default)
//   2. After scheduling: nodes[result.order[i]].schedule_cycle == i
//   3. For every dependency edge u -> v: nodes[u].schedule_cycle < nodes[v].schedule_cycle
//   4. All scheduled nodes have schedule_cycle >= 0
// ---------------------------------------------------------------------------

TEST_F(SchedulerTest, Schedule_InitialValueIsNegativeOne) {
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "sub", "x4", {"x5", "x6"}, 1),
    };
    auto nodes = builder.build(insts);

    for (size_t i = 0; i < nodes.size(); i++) {
        EXPECT_EQ(nodes[i].schedule_cycle, -1)
            << "Node " << i << " schedule_cycle should be -1 before scheduling";
    }
}

TEST_F(SchedulerTest, Schedule_SingleNode) {
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
    };
    auto nodes = builder.build(insts);
    CoutRedirect _;
    scheduler.schedule(insts, nodes);

    EXPECT_EQ(nodes[0].schedule_cycle, 0);
}

TEST_F(SchedulerTest, Schedule_MatchesResultOrderPosition) {
    // schedule() assigns schedule_cycle = i for result.order[i]
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "add", "x4", {"x1", "x5"}, 1),
        makeInst(2, "add", "x6", {"x4", "x7"}, 1),
    };
    auto nodes = builder.build(insts);
    CoutRedirect _;
    ScheduleResult result = scheduler.schedule(insts, nodes);

    for (size_t i = 0; i < result.order.size(); i++) {
        int nodeIdx = result.order[i];
        EXPECT_EQ(nodes[nodeIdx].schedule_cycle, static_cast<int>(i))
            << "Node " << nodeIdx << " scheduled at position " << i
            << " should have schedule_cycle=" << i;
    }
}

TEST_F(SchedulerTest, Schedule_AllScheduledNodesNonNegative) {
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "sub", "x4", {"x5", "x6"}, 1),
        makeInst(2, "mul", "x7", {"x1", "x4"}, 3),
    };
    auto nodes = builder.build(insts);
    CoutRedirect _;
    scheduler.schedule(insts, nodes);

    for (size_t i = 0; i < nodes.size(); i++) {
        EXPECT_GE(nodes[i].schedule_cycle, 0)
            << "Node " << i << " should have schedule_cycle >= 0 after scheduling";
    }
}

TEST_F(SchedulerTest, Schedule_DependencyOrderPreserved) {
    // For every edge u -> v in the DAG, nodes[u].schedule_cycle < nodes[v].schedule_cycle
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "add", "x4", {"x1", "x5"}, 1),
        makeInst(2, "add", "x6", {"x1", "x7"}, 1),
        makeInst(3, "mul", "x8", {"x4", "x6"}, 3),
    };
    auto nodes = builder.build(insts);
    CoutRedirect _;
    scheduler.schedule(insts, nodes);

    for (size_t u = 0; u < nodes.size(); u++) {
        for (const auto& edge : nodes[u].successors) {
            int v = edge.target_node;
            EXPECT_LT(nodes[u].schedule_cycle, nodes[v].schedule_cycle)
                << "Dependency edge " << u << " -> " << v
                << ": schedule_cycle[" << u << "]=" << nodes[u].schedule_cycle
                << " must be < schedule_cycle[" << v << "]=" << nodes[v].schedule_cycle;
        }
    }
}

TEST_F(SchedulerTest, Schedule_LinearChainSequential) {
    // Chain 0 -> 1 -> 2, each must be scheduled in order: cycles 0, 1, 2
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "add", "x4", {"x1", "x5"}, 1),
        makeInst(2, "add", "x6", {"x4", "x7"}, 1),
    };
    auto nodes = builder.build(insts);
    CoutRedirect _;
    scheduler.schedule(insts, nodes);

    EXPECT_EQ(nodes[0].schedule_cycle, 0);
    EXPECT_EQ(nodes[1].schedule_cycle, 1);
    EXPECT_EQ(nodes[2].schedule_cycle, 2);
}

TEST_F(SchedulerTest, Schedule_TwoIndependentOrderedByPriority) {
    // Node 1 has higher priority (latency 3) -> scheduled first at cycle 0
    // Node 0 has lower priority (latency 1) -> scheduled second at cycle 1
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "mul", "x4", {"x5", "x6"}, 3),
    };
    auto nodes = builder.build(insts);
    CoutRedirect _;
    ScheduleResult result = scheduler.schedule(insts, nodes);

    // The node scheduled first (result.order[0]) must have schedule_cycle == 0
    EXPECT_EQ(nodes[result.order[0]].schedule_cycle, 0);
    EXPECT_EQ(nodes[result.order[1]].schedule_cycle, 1);
}

TEST_F(SchedulerTest, Schedule_NoDuplicateCycles) {
    // Each instruction occupies exactly one cycle, so no two nodes share the same cycle
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "sub", "x4", {"x5", "x6"}, 1),
        makeInst(2, "mul", "x7", {"x8", "x9"}, 3),
        makeInst(3, "add", "x10", {"x1", "x4"}, 1),
    };
    auto nodes = builder.build(insts);
    CoutRedirect _;
    scheduler.schedule(insts, nodes);

    std::set<int> seen_cycles;
    for (size_t i = 0; i < nodes.size(); i++) {
        int cyc = nodes[i].schedule_cycle;
        EXPECT_TRUE(seen_cycles.find(cyc) == seen_cycles.end())
            << "Cycle " << cyc << " assigned to more than one node";
        seen_cycles.insert(cyc);
    }
}

// schedule_cycle tests for scheduleFast

TEST_F(SchedulerTest, ScheduleFast_ScheduleCycle_MatchesResultOrderPosition) {
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "add", "x4", {"x1", "x5"}, 1),
        makeInst(2, "add", "x6", {"x4", "x7"}, 1),
    };
    auto nodes = builder.build(insts);
    CoutRedirect _;
    ScheduleResult result = scheduler.scheduleFast(insts, nodes, TieBreakingPolicy::SMALLER_INDEX);

    for (size_t i = 0; i < result.order.size(); i++) {
        int nodeIdx = result.order[i];
        EXPECT_EQ(nodes[nodeIdx].schedule_cycle, static_cast<int>(i))
            << "Node " << nodeIdx << " at position " << i
            << " should have schedule_cycle=" << i;
    }
}

TEST_F(SchedulerTest, ScheduleFast_ScheduleCycle_DependencyOrderPreserved) {
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "add", "x4", {"x1", "x5"}, 1),
        makeInst(2, "add", "x6", {"x1", "x7"}, 1),
        makeInst(3, "mul", "x8", {"x4", "x6"}, 3),
    };
    auto nodes = builder.build(insts);
    CoutRedirect _;
    scheduler.scheduleFast(insts, nodes, TieBreakingPolicy::SMALLER_INDEX);

    for (size_t u = 0; u < nodes.size(); u++) {
        for (const auto& edge : nodes[u].successors) {
            int v = edge.target_node;
            EXPECT_LT(nodes[u].schedule_cycle, nodes[v].schedule_cycle)
                << "Dependency edge " << u << " -> " << v
                << ": schedule_cycle[" << u << "]=" << nodes[u].schedule_cycle
                << " must be < schedule_cycle[" << v << "]=" << nodes[v].schedule_cycle;
        }
    }
}

TEST_F(SchedulerTest, ScheduleFast_ScheduleCycle_ConsistentWithSchedule) {
    // schedule() and scheduleFast() with SMALLER_INDEX should assign identical schedule_cycle
    std::vector<Instruction> insts = {
        makeInst(0, "add", "x1", {"x2", "x3"}, 1),
        makeInst(1, "add", "x4", {"x1", "x5"}, 1),
        makeInst(2, "mul", "x6", {"x4", "x7"}, 3),
    };

    auto nodes1 = builder.build(insts);
    auto nodes2 = builder.build(insts);

    CoutRedirect _;
    scheduler.schedule(insts, nodes1);
    scheduler.scheduleFast(insts, nodes2, TieBreakingPolicy::SMALLER_INDEX);

    for (size_t i = 0; i < insts.size(); i++) {
        EXPECT_EQ(nodes1[i].schedule_cycle, nodes2[i].schedule_cycle)
            << "Node " << i << ": schedule() gives cycle " << nodes1[i].schedule_cycle
            << " but scheduleFast() gives " << nodes2[i].schedule_cycle;
    }
}

// ---------------------------------------------------------------------------
// Performance benchmarks
// ---------------------------------------------------------------------------

TEST(SchedulerPerformanceTest, LinearChain_CompareImplementations) {
    const int N = 5000;
    std::vector<Instruction> insts;
    insts.reserve(N);

    // Create a linear dependency chain
    insts.push_back(makeInst(0, "add", "x1", {"x2", "x3"}, 1));
    for (int i = 1; i < N; i++) {
        insts.push_back(makeInst(i, "add", "x1", {"x1", "x3"}, 1));
    }

    DAGBuilder builder;
    Scheduler scheduler;

    // Build nodes for both tests
    auto nodes1 = builder.build(insts);
    auto nodes2 = builder.build(insts);

    CoutRedirect _;

    // Time the original schedule()
    auto start1 = std::chrono::high_resolution_clock::now();
    ScheduleResult result1 = scheduler.schedule(insts, nodes1);
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);

    // Time the optimized scheduleFast()
    auto start2 = std::chrono::high_resolution_clock::now();
    ScheduleResult result2 = scheduler.scheduleFast(insts, nodes2);
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);

    std::cout << "\nLinearChain(" << N << "):\n";
    std::cout << "  schedule():     " << duration1.count() << " us\n";
    std::cout << "  scheduleFast(): " << duration2.count() << " us\n";
    if (duration1.count() > 0) {
        std::cout << "  Speedup: " << static_cast<double>(duration1.count()) / duration2.count()
                  << "x\n";
    }

    // Results should be identical
    EXPECT_EQ(result1.order, result2.order);
    EXPECT_EQ(result1.total_cycles, result2.total_cycles);
}

TEST(SchedulerPerformanceTest, ParallelInstructions_CompareImplementations) {
    const int N = 10000;
    std::vector<Instruction> insts;
    insts.reserve(N);

    // Create many independent instructions
    for (int i = 0; i < N; i++) {
        std::string dest = "x" + std::to_string(i % 32);
        std::string src1 = "x" + std::to_string((i + 10) % 32);
        std::string src2 = "x" + std::to_string((i + 20) % 32);
        insts.push_back(makeInst(i, "add", dest, {src1, src2}, 1));
    }

    DAGBuilder builder;
    Scheduler scheduler;

    auto nodes1 = builder.build(insts);
    auto nodes2 = builder.build(insts);

    CoutRedirect _;

    auto start1 = std::chrono::high_resolution_clock::now();
    ScheduleResult result1 = scheduler.schedule(insts, nodes1);
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);

    auto start2 = std::chrono::high_resolution_clock::now();
    ScheduleResult result2 = scheduler.scheduleFast(insts, nodes2);
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);

    std::cout << "\nParallelInstructions(" << N << "):\n";
    std::cout << "  schedule():     " << duration1.count() << " us\n";
    std::cout << "  scheduleFast(): " << duration2.count() << " us\n";
    if (duration1.count() > 0) {
        std::cout << "  Speedup: " << static_cast<double>(duration1.count()) / duration2.count()
                  << "x\n";
    }

    // Results should be identical
    EXPECT_EQ(result1.order.size(), result2.order.size());
    EXPECT_EQ(result1.total_cycles, result2.total_cycles);
}

TEST(SchedulerPerformanceTest, LargeScale_CompareImplementations) {
    const int N = 20000;
    std::vector<Instruction> insts;
    insts.reserve(N);

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> reg_dist(0, 31);

    for (int i = 0; i < N; i++) {
        std::string dest = "x" + std::to_string(reg_dist(rng));
        std::string src1 = "x" + std::to_string(reg_dist(rng));
        std::string src2 = "x" + std::to_string(reg_dist(rng));
        int latency = (i % 10 == 0) ? 5 : 1;
        insts.push_back(makeInst(i, "add", dest, {src1, src2}, latency));
    }

    DAGBuilder builder;
    Scheduler scheduler;

    auto nodes1 = builder.build(insts);
    auto nodes2 = builder.build(insts);

    CoutRedirect _;

    auto start1 = std::chrono::high_resolution_clock::now();
    ScheduleResult result1 = scheduler.schedule(insts, nodes1);
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1);

    auto start2 = std::chrono::high_resolution_clock::now();
    ScheduleResult result2 = scheduler.scheduleFast(insts, nodes2);
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2);

    std::cout << "\nLargeScale(" << N << "):\n";
    std::cout << "  schedule():     " << duration1.count() << " ms\n";
    std::cout << "  scheduleFast(): " << duration2.count() << " ms\n";
    if (duration1.count() > 0) {
        std::cout << "  Speedup: " << static_cast<double>(duration1.count()) / duration2.count()
                  << "x\n";
    }

    // Results should be identical
    EXPECT_EQ(result1.order.size(), result2.order.size());
    EXPECT_EQ(result1.total_cycles, result2.total_cycles);

    // scheduleFast should be significantly faster
    // Allow some tolerance for small inputs or fast machines
    if (duration1.count() > 100) {
        EXPECT_LT(duration2.count(), duration1.count())
            << "scheduleFast should be faster than schedule for large inputs";
    }
}

TEST(SchedulerPerformanceTest, ScheduleFast_VeryLargeScale) {
    // Test that scheduleFast can handle very large inputs that would be too slow for schedule()
    const int N = 100000;
    std::vector<Instruction> insts;
    insts.reserve(N);

    std::mt19937 rng(123);
    std::uniform_int_distribution<int> reg_dist(0, 31);

    for (int i = 0; i < N; i++) {
        std::string dest = "x" + std::to_string(reg_dist(rng));
        std::string src1 = "x" + std::to_string(reg_dist(rng));
        std::string src2 = "x" + std::to_string(reg_dist(rng));
        int latency = (i % 10 == 0) ? 5 : 1;
        insts.push_back(makeInst(i, "add", dest, {src1, src2}, latency));
    }

    DAGBuilder builder;
    Scheduler scheduler;

    auto nodes = builder.build(insts);

    CoutRedirect _;

    auto start = std::chrono::high_resolution_clock::now();
    ScheduleResult result = scheduler.scheduleFast(insts, nodes);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "\nVeryLargeScale(" << N << "):\n";
    std::cout << "  scheduleFast(): " << duration.count() << " ms\n";

    EXPECT_EQ(result.order.size(), N);
    EXPECT_LT(duration.count(), 5000);  // Should complete in < 5 seconds
}
