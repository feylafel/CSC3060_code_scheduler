# CSC3060 Spring 2026 Project 3: Code Scheduling

For a full description of the project, see the project handout at
[csc3060_spring2026_project3_cs.pdf](./csc3060_spring2026_project3_cs.pdf)

If you see any issues with the instruction or code, please feel free to
raise a GitHub issue or open a pull request with a fix.

## Setup

### Environment

- CMake 3.14 or higher version
- Compiler should support C++17 (GCC 7+, Clang 5+, MSVC 2017+)

```sh
mkdir build
cd build
cmake ..
make -j4
```

### Run unit tests

```sh
cd build
ctest                        # run all test
ctest -R Latency             # pattern matching "Latency"
ctest -V                     # verbose output
ctest -j4                    # parallel execution
```

Initially, all tests should fail with before you implement the function

## Reference

- [Engineering a Compiler (2nd Edition)](https://www.elsevier.com/books/engineering-a-compiler/cooper/978-0-12-088478-0) - Chapter 12: Instruction Scheduling
- [LLVM MachineScheduler](https://llvm.org/docs/MIRLangRef.html#machine-scheduler)
- [RISC-V Specifications](https://riscv.org/technical/specifications/)

## License

MIT License
