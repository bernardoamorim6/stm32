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

add_library(hal INTERFACE)
target_include_directories(hal INTERFACE hal/include)

# Warnings on everywhere, but only fatal in CI: -Werror in a local
# edit-compile loop gets in the way more than it helps.
option(WARNINGS_AS_ERRORS "Treat warnings as errors (CI sets this)" OFF)
set(PROJECT_WARNINGS -Wall -Wextra)
if(WARNINGS_AS_ERRORS)
  list(APPEND PROJECT_WARNINGS -Werror)
endif()

if(CMAKE_CROSSCOMPILING)
  # ARM build: executable target
  add_executable(
    firmware 
    app/main.c 
    drivers/src/startup.c
    drivers/src/systick.c
    drivers/src/gpio.c
    drivers/src/usart.c
    drivers/src/ringbuffer.c
  )
  set_target_properties(firmware PROPERTIES SUFFIX ".elf")

  # The UART ISR has a real time budget: at 115200 a byte arrives every 86.8 us,
  # and echoing needs two interrupts per byte. At -O0 nothing is inlined, so
  # every bitops and ring buffer helper becomes a call with a stack frame and
  # one pass through the handler costs roughly 800 instructions, about 100 us at
  # 12 MHz. That does not fit, and the receiver overruns continuously. -Og
  # inlines them and brings the same path down to about 85 instructions.
  #
  # -fno-tree-loop-distribute-patterns is not needed at -Og but matters if this
  # is ever raised to -O2: GCC then recognises the .bss zeroing loop in
  # Reset_Handler as a memset and emits a call to it, which -nostdlib has not
  # linked.
  target_compile_options(firmware PRIVATE -Og -fno-tree-loop-distribute-patterns ${PROJECT_WARNINGS})

  # C++ only. No exception or RTTI runtime is linked (-nostdlib), and with no
  # threads the guards around function-local statics protect nothing, yet
  # would need __cxa_guard_acquire/release from the C++ runtime.
  target_compile_options(firmware PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti -fno-threadsafe-statics>)

  # -nostdlib drops libgcc along with libc, but libgcc holds the compiler
  # support routines. Cortex-M0+ has no hardware divide instruction, so any
  # runtime division emits a call to __aeabi_uidiv. Linking libgcc back in is
  # the standard bare-metal arrangement: no C library, but keep the routines
  # the compiler itself needs.
  target_link_libraries(firmware PRIVATE bitops gcc)

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
    bitops
    GTest::gtest_main
  )

  add_executable(
    gpio_tests
    tests/gpio_tests.cpp
    drivers/src/gpio.c
  )
  target_link_libraries(
    gpio_tests
    bitops
    GTest::gtest_main
  )

  add_executable(
    ringbuffer_tests
    tests/ringbuffer_tests.cpp
    drivers/src/ringbuffer.c
  )
  target_link_libraries(
    ringbuffer_tests
    bitops
    GTest::gtest_main
  )

  add_executable(
    reg_tests
    tests/reg_tests.cpp
  )
  target_link_libraries(
    reg_tests
    hal
    GTest::gtest_main
  )

  foreach(t bitops_tests gpio_tests ringbuffer_tests reg_tests)
    target_compile_options(${t} PRIVATE ${PROJECT_WARNINGS})
  endforeach()

  include(GoogleTest)
  gtest_discover_tests(bitops_tests)
  gtest_discover_tests(gpio_tests)
  gtest_discover_tests(ringbuffer_tests)
  gtest_discover_tests(reg_tests)

  # Compile-time rejection tests. Each source must fail to build, and with the
  # expected static_assert message, so a typo cannot make one pass by accident.
  foreach(case value_too_wide:value\ does\ not\ fit\ in\ field
               field_past_bit_31:field\ must\ be\ 1-32\ bits)
    string(REPLACE ":" ";" parts "${case}")
    list(GET parts 0 name)
    list(GET parts 1 expected)
    add_executable(compile_fail_${name} EXCLUDE_FROM_ALL tests/compile_fail/${name}.cpp)
    target_link_libraries(compile_fail_${name} hal)
    add_test(NAME CompileFail.${name}
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target compile_fail_${name})
    set_tests_properties(CompileFail.${name} PROPERTIES PASS_REGULAR_EXPRESSION "${expected}")
  endforeach()

endif()