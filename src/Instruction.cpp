#include "Instruction.h"

#include <algorithm>
#include <unordered_map>

Instruction::Instruction() : index(-1), latency(1) {}

Instruction::Instruction(int idx, const std::string& raw, const std::string& op)
    : index(idx),
      raw_text(raw),
      opcode(op),
      latency(1) {}

bool Instruction::hasDestReg() const {
    return !dest_reg.empty();
}

bool Instruction::usesReg(const std::string& reg) const {
    // Check if reg is in source register list
    // TODO: Implement this function
    // throw std::runtime_error("usesReg not implemented");
    return std::find(src_regs.begin(), src_regs.end(), reg) != src_regs.end();
}

bool Instruction::definesReg(const std::string& reg) const {
    // Check if instruction defines the specified register
    // Here "define" simply means "write to" the register
    // For example, "add x1, x2, x3" defines x1
    // TODO: Implement this function
    // throw std::runtime_error("definesReg not implemented");
    return !dest_reg.empty() && dest_reg == reg;  // hasDestReg() = !dest_reg.empty()
}

void Instruction::print(std::ostream& os) const {
    os << "[" << index << "] " << raw_text;
    os << "\n    Opcode: " << opcode;
    os << ", Dest: " << (dest_reg.empty() ? "(none)" : dest_reg);
    os << ", Src: {";
    for (size_t i = 0; i < src_regs.size(); ++i) {
        if (i > 0)
            os << ", ";
        os << src_regs[i];
    }
    os << "}, Latency: " << latency;
}

// Latency model implementation
// This function returns the execution latency for a given opcode.
// We use a simplified static latency model here.
int getInstructionLatency(const std::string& opcode) {
    // TODO: there are some bugs in the current implementation, can you fix the bugs and pass the
    // tests?

    // Latency lookup table
    // Unknown instruction default latency is 1
    static const std::unordered_map<std::string, int> latencyTable = {
        // Arithmetic and logic instructions - 1 cycle
        {   "add",  1},
        {  "addi",  1},
        {  "addw",  1},
        { "addiw",  1},
        {   "sub",  1},
        {  "subw",  1},
        {   "and",  1},
        {  "andi",  1},
        {    "or",  1},
        {   "ori",  1},
        {   "xor",  1},
        {  "xori",  1},
        {   "sll",  1},
        {  "slli",  1},
        {  "sllw",  1},
        { "slliw",  1},
        {   "srl",  1},
        {  "srli",  1},
        {  "srlw",  1},
        { "srliw",  1},
        {   "sra",  1},
        {  "srai",  1},
        {  "sraw",  1},
        { "sraiw",  1},
        {   "slt",  1},
        {  "slti",  1},
        {  "sltu",  1},
        { "sltiu",  1},
        {   "lui",  1},
        { "auipc",  1},

        // Multiplication instructions - 3 cycles
        {  "mulh",  3},
        {"mulhsu",  3},
        { "mulhu",  3},
        {  "mulw",  3},
        {   "mul",  3}, // added

        // Division instructions - 10 cycles (division is typically slow)
        {   "div", 10},
        {  "divu", 10},
        {  "divw", 10},
        { "divuw", 10},
        {  "remu", 10},
        {  "remw", 10},
        { "remuw", 10},
        {   "rem", 10}, // added

        // Load instructions - 5 cycles (assuming cache hit)
        {    "lb",  5},
        {   "lbu",  5},
        {    "lh",  5},
        {   "lhu",  5},
        {   "lwu",  5},
        {    "ld",  5},
        {    "lw",  5}, // added

        // Store instructions - 1 cycle (assuming write buffer available)
        {    "sb",  1},
        {    "sh",  1},
        {    "sd",  1},
        {    "sw",  1}, // added

        // Branch and jump - 1 cycle
        {   "beq",  1},
        {   "bne",  1},
        {   "blt",  1},
        {   "bge",  1},
        {  "bltu",  1},
        {  "bgeu",  1},
        {   "jal",  1},
        {  "jalr",  1},

        // Pseudo-instructions
        {    "mv",  1},
        {    "li",  1},
        {    "la",  1},
        {   "nop",  1},
    };

    auto it = latencyTable.find(opcode);
    if (it != latencyTable.end()) {
        return it->second;
    }

    return 1;
}
