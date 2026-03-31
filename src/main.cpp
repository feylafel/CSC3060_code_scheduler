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
    // std::cout << "TODO: runScheduler not implemented\n";
    // I've added  “Quick analysis” section after each example run for myself
    std::cout << "============================================================\n"; // just for readability

    Parser parser;
    DAGBuilder builder;
    Scheduler scheduler;

    // parse assembly text into Instruction objects.
    auto instructions = parser.parse(code);

    // build dependency DAG.
    auto nodes = builder.build(instructions);

    // run list scheduling.
    auto scheduled_result = scheduler.schedule(instructions, nodes);

    // print scheduled order.
    scheduler.printSchedule(scheduled_result, instructions, nodes);

    // print comparison between original order and scheduled order.
    scheduler.printComparison(instructions, nodes, scheduled_result);

    // brief analysis
    std::vector<int> original_order;
    original_order.reserve(instructions.size());
    for (size_t i = 0; i < instructions.size(); i++) {
        original_order.push_back(static_cast<int>(i));
    }

    auto original_sim = scheduler.simulateExecution(original_order, instructions, nodes);
    const int saved = original_sim.total_cycles - scheduled_result.total_cycles;

    // my own quick analysis
    std::cout << "quick analysis:\n";
        if (saved > 0) {
            std::cout << "  Improvement comes from overlapping independent work with long-latency dependent operations.\n";
        } else if (saved == 0) {
            std::cout << "  No improvement because dependencies already limit reordering opportunities.\n";
        } else {
            std::cout << "  Reordering did not help because the original order was already close to the critical path.\n";
        }
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
