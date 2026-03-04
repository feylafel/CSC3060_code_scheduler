#include "DAGBuilder.h"
#include "Instruction.h"
#include "Parser.h"
#include "Scheduler.h"

#include <iostream>
#include <string>
#include <vector>

// Example RISC-V code
// This code is used to demonstrate how the scheduler handles RAW, WAR, WAW
// dependencies. You can use this code to test the scheduler.
std::vector<std::string> getExampleCode() {
    return {
        "lw    x10, 0(x5)",
        "lw    x11, 4(x5)",
        "lw    x12, 8(x5)",
        "lw    x13, 12(x5)",
        "mul   x1, x10, x11",
        "mul   x2, x12, x13",
        "lw    x14, 16(x5)",
        "add   x3, x1, x2",
        "mul   x20, x3, x14",
        "sw    x20, 20(x5)",
    };
}

std::vector<std::string> getExampleCode2() {
    return {
        "add   x1, x2, x3",
        "add   x4, x1, x5",
        "add   x1, x6, x7",
        "add   x8, x1, x9",
        "mul   x10, x4, x8",
    };
}

std::vector<std::string> getExampleCode3_BadOrder() {
    return {
        "lw    x1, 0(x10)",
        "mul   x5, x1, x1",
        "lw    x2, 4(x10)",
        "mul   x6, x2, x2",
        "lw    x3, 8(x10)",
        "mul   x7, x3, x3",
        "lw    x4, 12(x10)",
        "mul   x8, x4, x4",
        "add   x9, x5, x6",
        "add   x11, x7, x8",
        "add   x12, x9, x11",
        "sw    x12, 0(x20)",
    };
}

std::vector<std::string> getExampleCode4_CriticalPath() {
    return {
        "add   x1, x10, x11",
        "add   x2, x12, x13",
        "add   x3, x14, x15",
        "mul   x4, x1, x2",
        "mul   x5, x2, x3",
        "div   x6, x16, x17",
        "add   x7, x6, x18",
        "mul   x8, x7, x7",
        "add   x9, x4, x5",
        "add   x20, x8, x9",
        "sw    x20, 0(x21)",
    };
}

void printWelcome() {
    std::cout << R"(
╔══════════════════════════════════════════════════════════════════════════════════════╗
║                                                                                      ║
║     ███╗   ███╗██╗███╗   ██╗██╗ ██████╗ ██████╗ ██████╗ ███████╗                     ║
║     ████╗ ████║██║████╗  ██║██║██╔════╝██╔═══██╗██╔══██╗██╔════╝                     ║
║     ██╔████╔██║██║██╔██╗ ██║██║██║     ██║   ██║██║  ██║█████╗                       ║
║     ██║╚██╔╝██║██║██║╚██╗██║██║██║     ██║   ██║██║  ██║██╔══╝                       ║
║     ██║ ╚═╝ ██║██║██║ ╚████║██║╚██████╗╚██████╔╝██████╔╝███████╗                     ║
║     ╚═╝     ╚═╝╚═╝╚═╝  ╚═══╝╚═╝ ╚═════╝ ╚═════╝ ╚═════╝ ╚══════╝                     ║
║                                                                                      ║
║         ███████╗ ██████╗██╗  ██╗███████╗██████╗ ██╗   ██╗██╗     ███████╗██████╗     ║
║         ██╔════╝██╔════╝██║  ██║██╔════╝██╔══██╗██║   ██║██║     ██╔════╝██╔══██╗    ║
║         ███████╗██║     ███████║█████╗  ██║  ██║██║   ██║██║     █████╗  ██████╔╝    ║
║         ╚════██║██║     ██╔══██║██╔══╝  ██║  ██║██║   ██║██║     ██╔══╝  ██╔══██╗    ║
║         ███████║╚██████╗██║  ██║███████╗██████╔╝╚██████╔╝███████╗███████╗██║  ██║    ║
║         ╚══════╝ ╚═════╝╚═╝  ╚═╝╚══════╝╚═════╝  ╚═════╝ ╚══════╝╚══════╝╚═╝  ╚═╝    ║
║                                                                                      ║
║                                                                                      ║
║                        A Teaching-Oriented List Scheduler                            ║
║                                                                                      ║
╚══════════════════════════════════════════════════════════════════════════════════════╝
)" << std::endl;
}

void runScheduler(const std::string& name, const std::vector<std::string>& code) {
    // TODO: Put All things together
    std::cout << "\n";
    std::cout << name << "\n";

    std::cout << "TODO: runScheduler not implemented\n";
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    printWelcome();

    runScheduler("Example 1: Expression Calculation", getExampleCode());
    runScheduler("Example 2: Dependency Type Demonstration", getExampleCode2());
    runScheduler("Example 3: Load-Use Hazard Hidden", getExampleCode3_BadOrder());
    runScheduler("Example 4: Early Start Critical Operation", getExampleCode4_CriticalPath());

    std::cout << "\n";
    std::cout << "Scheduling Completed!\n";

    return 0;
}
