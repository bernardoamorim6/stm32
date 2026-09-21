cmake_minimum_required(VERSION 3.14)
project(my_project)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# GoogleTest requires at least C++17
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(bitops INTERFACE)
target_include_directories(bitops INTERFACE drivers/include)

if(CMAKE_CROSSCOMPILING)
  # ARM build: firmware only
  add_library(firmware STATIC app/main.c)
  target_link_libraries(firmware PRIVATE bitops)
else()
  # Host build: tests only

  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
  )
  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  enable_testing()

  add_executable(
    bitops_tests
    tests/bitops_tests.cpp
  )
  target_link_libraries(
    bitops_tests
    GTest::gtest_main
  )

  include(GoogleTest)
  gtest_discover_tests(bitops_tests)

endif()