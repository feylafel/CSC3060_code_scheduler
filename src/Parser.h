#ifndef PARSER_H
#define PARSER_H

#include "Instruction.h"

#include <string>
#include <vector>

class Parser {
public:
    std::vector<Instruction> parse(const std::vector<std::string>& lines);

    Instruction parseLine(const std::string& line, int index);

private:
    std::string trim(const std::string& s);
    std::vector<std::string> split(const std::string& s, char delimiter);
    std::string cleanRegister(const std::string& s);
    bool isRegister(const std::string& s);
    bool isRType(const std::string& opcode);
    bool isIType(const std::string& opcode);
    bool isLoad(const std::string& opcode);
    bool isStore(const std::string& opcode);
    bool isBranch(const std::string& opcode);
};

#endif  // PARSER_H
