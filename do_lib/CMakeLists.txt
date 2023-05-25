project(do_lib LANGUAGES CXX)

include_directories(../third_party/)

add_library(${PROJECT_NAME} SHARED
    do_lib_linux.cpp
    disassembler.cpp
    ipc.cpp
    darkorbit.cpp
    memory_linux.cpp
    avm.cpp
    singleton.cpp
    flash_stuff.cpp
)
target_compile_options(${PROJECT_NAME} PUBLIC -Wall)

target_link_libraries(${PROJECT_NAME} subhook pthread)

