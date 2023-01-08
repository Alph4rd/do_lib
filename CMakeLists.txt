cmake_minimum_required(VERSION 3.10)

project(do_lib LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(SUBHOOK_STATIC ON)
set(SUBHOOK_TESTS OFF)

add_subdirectory(third_party/)

include_directories(third_party/)

add_library(${PROJECT_NAME} SHARED
	do_lib_linux.cpp
	disassembler.cpp
	ipc.cpp
	darkorbit.cpp
	memory_linux.cpp
	avm.cpp
	singleton.cpp
	flash_hooks.cpp
)
target_compile_options(${PROJECT_NAME} PUBLIC -Wall)

target_link_libraries(${PROJECT_NAME} subhook pthread)
