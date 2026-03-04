// test_instruction.cpp - Unit tests for Instruction class using Google Test

#include "Instruction.h"

#include <gtest/gtest.h>

// Test fixture for Instruction tests
class InstructionTest : public ::testing::Test {
protected:
    Instruction inst;
};

// usesReg tests

TEST_F(InstructionTest, UsesReg_Basic) {
    inst.src_regs = {"x1", "x2"};

    EXPECT_TRUE(inst.usesReg("x1"));
    EXPECT_TRUE(inst.usesReg("x2"));
    EXPECT_FALSE(inst.usesReg("x3"));
}

TEST_F(InstructionTest, UsesReg_Empty) {
    // No source registers
    EXPECT_FALSE(inst.usesReg("x1"));
    EXPECT_FALSE(inst.usesReg("x0"));
}

TEST_F(InstructionTest, UsesReg_Single) {
    inst.src_regs = {"x5"};

    EXPECT_TRUE(inst.usesReg("x5"));
    EXPECT_FALSE(inst.usesReg("x1"));
}

TEST_F(InstructionTest, UsesReg_Duplicates) {
    inst.src_regs = {"x1", "x1", "x2"};

    EXPECT_TRUE(inst.usesReg("x1"));
    EXPECT_TRUE(inst.usesReg("x2"));
}

// definesReg tests

TEST_F(InstructionTest, DefinesReg_Basic) {
    inst.dest_reg = "x1";

    EXPECT_TRUE(inst.definesReg("x1"));
    EXPECT_FALSE(inst.definesReg("x2"));
}

TEST_F(InstructionTest, DefinesReg_NoDest) {
    // No destination register (e.g., sw instruction)
    EXPECT_FALSE(inst.definesReg("x1"));
}

TEST_F(InstructionTest, DefinesReg_WithSources) {
    inst.dest_reg = "x1";
    inst.src_regs = {"x2", "x3"};

    EXPECT_TRUE(inst.definesReg("x1"));
    EXPECT_FALSE(inst.definesReg("x2"));  // x2 is source, not dest
    EXPECT_FALSE(inst.definesReg("x3"));  // x3 is source, not dest
}

// Dependency simulation tests

TEST_F(InstructionTest, RAW_Simulation) {
    // add x1, x2, x3
    Instruction add_inst;
    add_inst.opcode = "add";
    add_inst.dest_reg = "x1";
    add_inst.src_regs = {"x2", "x3"};

    // sub x4, x1, x5 (uses x1 that add defines)
    Instruction sub_inst;
    sub_inst.opcode = "sub";
    sub_inst.dest_reg = "x4";
    sub_inst.src_regs = {"x1", "x5"};

    // Check RAW: sub uses x1, add defines x1
    EXPECT_TRUE(add_inst.definesReg("x1"));
    EXPECT_TRUE(sub_inst.usesReg("x1"));
}

TEST_F(InstructionTest, WAW_Simulation) {
    // add x1, x2, x3
    Instruction add_inst;
    add_inst.dest_reg = "x1";

    // sub x1, x4, x5 (also writes to x1)
    Instruction sub_inst;
    sub_inst.dest_reg = "x1";

    // Check WAW: both define x1
    EXPECT_TRUE(add_inst.definesReg("x1"));
    EXPECT_TRUE(sub_inst.definesReg("x1"));
}

TEST_F(InstructionTest, WAR_Simulation) {
    // add x1, x2, x3 (reads x2)
    Instruction add_inst;
    add_inst.dest_reg = "x1";
    add_inst.src_regs = {"x2", "x3"};

    // sub x2, x4, x5 (writes x2)
    Instruction sub_inst;
    sub_inst.dest_reg = "x2";
    sub_inst.src_regs = {"x4", "x5"};

    // Check WAR: add uses x2, sub defines x2
    EXPECT_TRUE(add_inst.usesReg("x2"));
    EXPECT_TRUE(sub_inst.definesReg("x2"));
}

// Latency tests

TEST(LatencyTest, MultiplicationInstructions) {
    EXPECT_EQ(getInstructionLatency("mul"), 3);
    EXPECT_EQ(getInstructionLatency("mulh"), 3);
    EXPECT_EQ(getInstructionLatency("mulw"), 3);
}

TEST(LatencyTest, DivisionInstructions) {
    EXPECT_EQ(getInstructionLatency("div"), 10);
    EXPECT_EQ(getInstructionLatency("divu"), 10);
    EXPECT_EQ(getInstructionLatency("rem"), 10);
}

TEST(LatencyTest, MemoryInstructions) {
    EXPECT_EQ(getInstructionLatency("lw"), 5);
    EXPECT_EQ(getInstructionLatency("ld"), 5);
    EXPECT_EQ(getInstructionLatency("sw"), 1);
    EXPECT_EQ(getInstructionLatency("sd"), 1);
}
