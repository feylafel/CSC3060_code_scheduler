cmake_minimum_required(VERSION 3.14)
project(miniCodeScheduler 
    VERSION 1.0.0
    DESCRIPTION "A minimal RISC-V instruction list scheduler"
    LANGUAGES CXX
)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Source files (library code, without main.cpp)
set(SCHEDULER_LIB_SOURCES
    src/Instruction.cpp
    src/Parser.cpp
    src/DAGBuilder.cpp
    src/Scheduler.cpp
)

# Header directory
include_directories(${CMAKE_SOURCE_DIR}/src)

# Create a static library for reuse in tests
add_library(scheduler_lib STATIC ${SCHEDULER_LIB_SOURCES})

# Main executable
add_executable(scheduler src/main.cpp)
target_link_libraries(scheduler scheduler_lib)

# Enable testing
enable_testing()

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
# Prevent GoogleTest from overriding compiler/linker options
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Include GoogleTest module
include(GoogleTest)

# Test executables
add_executable(test_instruction tests/test_instruction.cpp)
target_link_libraries(test_instruction scheduler_lib GTest::gtest_main)

add_executable(test_dagbuilder tests/test_dagbuilder.cpp)
target_link_libraries(test_dagbuilder scheduler_lib GTest::gtest_main)

add_executable(test_scheduler tests/test_scheduler.cpp)
target_link_libraries(test_scheduler scheduler_lib GTest::gtest_main)

# Auto-discover tests and register with CTest
# Set timeout to 30 seconds per test to catch infinite loops
gtest_discover_tests(test_instruction PROPERTIES TIMEOUT 30)
gtest_discover_tests(test_dagbuilder PROPERTIES TIMEOUT 30)
gtest_discover_tests(test_scheduler PROPERTIES TIMEOUT 30)

# Build info
message(STATUS "Building miniCodeScheduler v${PROJECT_VERSION}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
