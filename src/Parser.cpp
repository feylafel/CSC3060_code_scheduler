#include "Parser.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>

std::vector<Instruction> Parser::parse(const std::vector<std::string>& lines) {
    std::vector<Instruction> instructions;
    int index = 0;

    for (const auto& line : lines) {
        std::string trimmed = trim(line);

        // Skip empty lines and comments
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }

        // Skip label lines (ending with colon)
        if (trimmed.back() == ':') {
            continue;
        }

        // Remove inline comments
        size_t commentPos = trimmed.find('#');
        if (commentPos != std::string::npos) {
            trimmed = trim(trimmed.substr(0, commentPos));
        }

        if (!trimmed.empty()) {
            Instruction inst = parseLine(trimmed, index);
            inst.latency = getInstructionLatency(inst.opcode);
            instructions.push_back(inst);
            index++;
        }
    }

    return instructions;
}

Instruction Parser::parseLine(const std::string& line, int index) {
    std::string trimmed = trim(line);
    Instruction inst(index, trimmed, "");

    // Find first space to separate opcode and operands
    size_t spacePos = trimmed.find_first_of(" \t");

    if (spacePos == std::string::npos) {
        // Instruction with no operands (e.g., nop, ret)
        inst.opcode = trimmed;
        return inst;
    }

    inst.opcode = trimmed.substr(0, spacePos);
    std::string operands = trim(trimmed.substr(spacePos + 1));

    // Split operands
    std::vector<std::string> ops = split(operands, ',');

    if (isRType(inst.opcode)) {
        // e.g. add rd, rs1, rs2
        // rd is destination, rs1 and rs2 are sources
        if (ops.size() >= 3) {
            inst.dest_reg = cleanRegister(ops[0]);
            if (isRegister(cleanRegister(ops[1]))) {
                inst.src_regs.push_back(cleanRegister(ops[1]));
            }
            if (isRegister(cleanRegister(ops[2]))) {
                inst.src_regs.push_back(cleanRegister(ops[2]));
            }
        }
    } else if (isLoad(inst.opcode)) {
        // e.g. lw rd, offset(rs1)
        // rd is destination, rs1 is source
        if (ops.size() >= 2) {
            inst.dest_reg = cleanRegister(ops[0]);
            // Extract rs1 from offset(rs1)
            std::string memOp = ops[1];
            size_t parenStart = memOp.find('(');
            size_t parenEnd = memOp.find(')');
            if (parenStart != std::string::npos && parenEnd != std::string::npos) {
                std::string baseReg = memOp.substr(parenStart + 1, parenEnd - parenStart - 1);
                baseReg = cleanRegister(baseReg);
                if (isRegister(baseReg)) {
                    inst.src_regs.push_back(baseReg);
                }
            }
        }
    } else if (isStore(inst.opcode)) {
        // e.g. sw rs2, offset(rs1)
        // No destination register, rs2 and rs1 are both sources
        if (ops.size() >= 2) {
            // rs2 is the value to store
            std::string valReg = cleanRegister(ops[0]);
            if (isRegister(valReg)) {
                inst.src_regs.push_back(valReg);
            }
            // Extract rs1 from offset(rs1)
            std::string memOp = ops[1];
            size_t parenStart = memOp.find('(');
            size_t parenEnd = memOp.find(')');
            if (parenStart != std::string::npos && parenEnd != std::string::npos) {
                std::string baseReg = memOp.substr(parenStart + 1, parenEnd - parenStart - 1);
                baseReg = cleanRegister(baseReg);
                if (isRegister(baseReg)) {
                    inst.src_regs.push_back(baseReg);
                }
            }
        }
    } else if (isIType(inst.opcode)) {
        // e.g. addi rd, rs1, imm
        // rd is destination, rs1 is source
        if (ops.size() >= 2) {
            inst.dest_reg = cleanRegister(ops[0]);
            std::string src = cleanRegister(ops[1]);
            if (isRegister(src)) {
                inst.src_regs.push_back(src);
            }
        }
    } else if (isBranch(inst.opcode)) {
        // e.g. beq rs1, rs2, label
        // Two source registers, no destination
        if (ops.size() >= 2) {
            std::string r1 = cleanRegister(ops[0]);
            std::string r2 = cleanRegister(ops[1]);
            if (isRegister(r1)) {
                inst.src_regs.push_back(r1);
            }
            if (isRegister(r2)) {
                inst.src_regs.push_back(r2);
            }
        }
    } else if (inst.opcode == "mv") {
        // e.g. mv rd, rs
        if (ops.size() >= 2) {
            inst.dest_reg = cleanRegister(ops[0]);
            std::string src = cleanRegister(ops[1]);
            if (isRegister(src)) {
                inst.src_regs.push_back(src);
            }
        }
    } else if (inst.opcode == "li" || inst.opcode == "lui" || inst.opcode == "auipc") {
        // e.g. li/lui/auipc only destination register
        if (ops.size() >= 1) {
            inst.dest_reg = cleanRegister(ops[0]);
        }
    } else if (inst.opcode == "jal") {
        // e.g. jal rd, label (or jal label)
        if (ops.size() >= 1) {
            std::string first = cleanRegister(ops[0]);
            if (isRegister(first)) {
                inst.dest_reg = first;
            }
        }
    } else if (inst.opcode == "jalr") {
        // e.g. jalr rd, rs1, offset
        if (ops.size() >= 2) {
            inst.dest_reg = cleanRegister(ops[0]);
            std::string src = cleanRegister(ops[1]);
            if (isRegister(src)) {
                inst.src_regs.push_back(src);
            }
        }
    } else {
        // first operand is destination, rest are sources
        for (size_t i = 0; i < ops.size(); i++) {
            std::string reg = cleanRegister(ops[i]);
            if (isRegister(reg)) {
                if (i == 0) {
                    inst.dest_reg = reg;
                } else {
                    inst.src_regs.push_back(reg);
                }
            }
        }
    }

    return inst;
}

std::string Parser::trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::vector<std::string> Parser::split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(s);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        std::string trimmed = trim(token);
        if (!trimmed.empty()) {
            tokens.push_back(trimmed);
        }
    }
    return tokens;
}

std::string Parser::cleanRegister(const std::string& s) {
    std::string result = trim(s);
    // 移除括号
    size_t parenPos = result.find('(');
    if (parenPos != std::string::npos) {
        result = result.substr(0, parenPos);
    }
    return trim(result);
}

bool Parser::isRegister(const std::string& s) {
    if (s.empty())
        return false;

    // RISC-V registers: x0-x31, zero, ra, sp, gp, tp, t0-t6, s0-s11, a0-a7, fp
    if (s[0] == 'x' && s.length() > 1) {
        // x0 - x31
        for (size_t i = 1; i < s.length(); i++) {
            if (!std::isdigit(s[i]))
                return false;
        }
        return true;
    }

    // ABI names
    static const std::set<std::string> abiNames = {"zero", "ra", "sp",  "gp",  "tp", "fp", "t0",
                                                   "t1",   "t2", "t3",  "t4",  "t5", "t6", "s0",
                                                   "s1",   "s2", "s3",  "s4",  "s5", "s6", "s7",
                                                   "s8",   "s9", "s10", "s11", "a0", "a1", "a2",
                                                   "a3",   "a4", "a5",  "a6",  "a7"};

    return abiNames.count(s) > 0;
}

bool Parser::isRType(const std::string& opcode) {
    static const std::set<std::string> rTypeOps = {
        "add",  "sub",  "sll",  "slt",  "sltu", "xor",   "srl",  "sra",    "or",    "and",
        "addw", "subw", "sllw", "srlw", "sraw", "mul",   "mulh", "mulhsu", "mulhu", "div",
        "divu", "rem",  "remu", "mulw", "divw", "divuw", "remw", "remuw"};
    return rTypeOps.count(opcode) > 0;
}

bool Parser::isIType(const std::string& opcode) {
    static const std::set<std::string> iTypeOps = {"addi",
                                                   "slti",
                                                   "sltiu",
                                                   "xori",
                                                   "ori",
                                                   "andi",
                                                   "slli",
                                                   "srli",
                                                   "srai",
                                                   "addiw",
                                                   "slliw",
                                                   "srliw",
                                                   "sraiw"};
    return iTypeOps.count(opcode) > 0;
}

bool Parser::isLoad(const std::string& opcode) {
    static const std::set<std::string> loadOps = {"lb", "lh", "lw", "ld", "lbu", "lhu", "lwu"};
    return loadOps.count(opcode) > 0;
}

bool Parser::isStore(const std::string& opcode) {
    static const std::set<std::string> storeOps = {"sb", "sh", "sw", "sd"};
    return storeOps.count(opcode) > 0;
}

bool Parser::isBranch(const std::string& opcode) {
    static const std::set<std::string> branchOps = {"beq", "bne", "blt", "bge", "bltu", "bgeu"};
    return branchOps.count(opcode) > 0;
}
