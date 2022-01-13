cmake_minimum_required(VERSION 3.22)
project(miniDebugger)

set(CMAKE_CXX_STANDARD 14)

include_directories(dependencies/libelfin dependencies/linenoise include)
add_executable(miniDebugger src/miniDebugger.cpp dependencies/linenoise/linenoise.c)

add_executable(hello examples/hello.cpp)
set_target_properties(hello
        PROPERTIES COMPILE_FLAGS "-g -O0")

add_executable(variable examples/variable.cpp)
set_target_properties(variable
        PROPERTIES COMPILE_FLAGS "-gdwarf-2 -O0")

add_executable(unwinding examples/stack_unwinding.cpp)
set_target_properties(unwinding
        PROPERTIES COMPILE_FLAGS "-g -O0")

add_custom_target(libelfin
        COMMAND make
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/dependencies/libelfin)

target_link_libraries(miniDebugger
        ${PROJECT_SOURCE_DIR}/dependencies/libelfin/dwarf/libdwarf++.so
        ${PROJECT_SOURCE_DIR}/dependencies/libelfin/elf/libelf++.so)
add_dependencies(miniDebugger libelfin)

