cmake_minimum_required(VERSION 3.10.2)
project(black_box VERSION 0.5.1 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_definitions(_WIN32_WINNT=0x0601 SPDLOG_COMPILED_LIB)
    add_compile_options("/utf-8")
endif()

add_subdirectory(src)