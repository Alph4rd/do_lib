cmake_minimum_required(VERSION 3.10)

project (DarkTanos)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(SUBHOOK_STATIC ON)
set(SUBHOOK_TESTS OFF)


add_subdirectory(client/)
add_subdirectory(do_lib/)
add_subdirectory(third_party/)
