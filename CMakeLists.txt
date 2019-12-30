cmake_minimum_required(VERSION 3.10.0)
project(dr-cu-rl
        LANGUAGES CXX
        VERSION 1.2.0
        )
set(CMAKE_CXX_STANDARD 17)

add_executable(dr-cu-rl main.cpp Envs.cpp)
set(LIB_DIR ${CMAKE_CURRENT_LIST_DIR}/lib)
set(CPPZMQ_DIR ${LIB_DIR}/cppzmq)
set(MSGPACK_DIR ${LIB_DIR}/msgpack-c)
set(SPDLOG_DIR ${LIB_DIR}/spdlog)
set(ZMQ_DIR ${LIB_DIR}/libzmq)

# Torch
set(Torch_DIR /home/qut/Repo/libtorch_precxx11/libtorch/share/cmake/Torch)
# set(Torch_DIR /home/quto/Repo/libtorch/share/cmake/Torch)
if (NOT TORCH_FOUND)
    find_package(Torch REQUIRED)
    if (TORCH_CXX_FLAGS)
        set(CMAKE_CXX_FLAGS ${TORCH_CXX_FLAGS})
    endif()
endif (NOT TORCH_FOUND)

# Spdlog
add_subdirectory(${SPDLOG_DIR})
# ppo
add_subdirectory(./ppo)
# Dr.CU
add_subdirectory(./dr-cu/src)

target_include_directories(dr-cu-rl
    PRIVATE
    .
    ../include
    ../lib/spdlog/include
    ${CPPZMQ_DIR}
    ${MSGPACK_DIR}/include
    ${SPDLOG_DIR}/include
    ${ZMQ_DIR}/include
    ./ppo/include
    ./dr-cu
    ./dr-cu/src
    ./dr-cu/rsyn/src
    ./dr-cu/rsyn/src/rsyn/export
    ./dr-cu/rsyn/include
)
#set_target_properties(dr-cu-rl PROPERTIES LINK_FLAGS "-static -Wl,--whole-archive -rdynamic -lpthread -Wl,--no-whole-archive")
set_target_properties(dr-cu-rl PROPERTIES LINK_FLAGS "-Wl,--whole-archive -rdynamic -lpthread -Wl,--no-whole-archive")
target_link_libraries(dr-cu-rl PRIVATE cpprl ispd19dr)
