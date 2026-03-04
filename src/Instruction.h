#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <iostream>
#include <string>
#include <vector>

class Instruction {
public:
    // Instruction index in original sequence (for debugging and output)
    int index;

    std::string raw_text;

    // e.g., "add", "lw", "mul"
    std::string opcode;

    // Destination register (optional, some instructions like sw have no destination)
    std::string dest_reg;

    // Source register list
    std::vector<std::string> src_regs;

    // Instruction latency (execution cycles)
    int latency;

    Instruction();
    Instruction(int idx, const std::string& raw, const std::string& op);

    bool hasDestReg() const;

    bool usesReg(const std::string& reg) const;

    // Check if instruction defines a register
    bool definesReg(const std::string& reg) const;

    void print(std::ostream& os = std::cout) const;
};

int getInstructionLatency(const std::string& opcode);

#endif  // INSTRUCTION_H
