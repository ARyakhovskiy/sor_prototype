cmake_minimum_required(VERSION 3.14)
project(SmartOrderRouting)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include headers
include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

# Add the main executable
add_executable(smart_order_routing 
    src/main.cpp
    src/orderbook.cpp
    src/smartorderrouter.cpp
    src/executionplan.cpp
    src/utils.cpp
    )

add_subdirectory(tests)
enable_testing()
add_test(NAME sor_tests COMMAND sor_tests)
