cmake_minimum_required(VERSION 3.20)
project(battlecode_bot LANGUAGES CXX)

# The judge compiles to wasm itself; this file is only for IntelliSense.
set(CMAKE_CXX_STANDARD 20)
add_executable(bot
	src/main.cpp
	src/core/memory.cpp
	src/comms/comms.cpp
	src/nav/grid.cpp
	src/nav/bfs.cpp
	src/nav/astar.cpp
	src/nav/flee.cpp
	src/nav/wander.cpp
	src/roles/explorer.cpp
	src/roles/killer.cpp
	src/roles/feeder.cpp
	src/roles/bodyguard.cpp
	src/roles/queen.cpp
	src/roles/stub.cpp
	src/roles/registry.cpp
)
target_include_directories(bot PRIVATE src)
