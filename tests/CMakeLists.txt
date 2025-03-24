cmake_minimum_required(VERSION 3.28)
# Fetch Google Test
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

# Add the test executable
add_executable(order_book_tests test.cpp ../src/orderbook.cpp ../src/smartorderrouter.cpp ../src/executionplan.cpp ../src/utils.cpp)
target_link_libraries(order_book_tests gtest_main)

# Enable testing
enable_testing()
add_test(NAME order_book_tests COMMAND order_book_tests)