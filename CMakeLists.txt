cmake_minimum_required(VERSION 3.20)
project(battlecode_bot LANGUAGES CXX)

# The judge compiles to wasm itself; this file is only for IntelliSense.
set(CMAKE_CXX_STANDARD 20)
add_executable(bot src/main.cpp)
target_include_directories(bot PRIVATE src)
