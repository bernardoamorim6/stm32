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
  # ARM build: executable target
  add_executable(
    firmware 
    app/main.c 
    drivers/src/startup.c
    drivers/src/systick.c
    )
  set_target_properties(firmware PROPERTIES SUFFIX ".elf")

  target_link_libraries(firmware PRIVATE bitops)

  target_link_options(firmware PRIVATE
    -nostdlib
    "-T${CMAKE_CURRENT_SOURCE_DIR}/linker/stm32c031c6.ld"
  )
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