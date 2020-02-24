cmake_minimum_required(VERSION 3.10.0)
project(dr-cu-rl
        LANGUAGES CXX
        VERSION 1.2.0
        )
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_executable(dr-cu-rl main.cpp Envs.cpp)
set(LIB_DIR ${CMAKE_CURRENT_LIST_DIR}/lib)
set(CPPZMQ_DIR ${LIB_DIR}/cppzmq)
set(MSGPACK_DIR ${LIB_DIR}/msgpack-c)
set(SPDLOG_DIR ${LIB_DIR}/spdlog)
set(ZMQ_DIR ${LIB_DIR}/libzmq)

find_package(pybind11 REQUIRED)

# Torch
set(Torch_CMAKE_DIR ${LIB_DIR}/libtorch/share/cmake)
set(Torch_DIR ${Torch_CMAKE_DIR}/Torch)
# set(Torch_DIR /home/quto/Repo/libtorch/share/cmake/Torch)
if (NOT TORCH_FOUND)
    find_package(Torch REQUIRED)
    if (TORCH_CXX_FLAGS)
        set(CMAKE_CXX_FLAGS ${TORCH_CXX_FLAGS})
        message(STATUS "TORCH_CXX_FLAGS = ${TORCH_CXX_FLAGS}")
        message(STATUS "TORCH_INCLUDE_DIRS = ${TORCH_INCLUDE_DIRS}")
    endif()
endif (NOT TORCH_FOUND)

# Spdlog
add_subdirectory(${SPDLOG_DIR})
# ppo
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ppo)
# Dr.CU
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dr-cu/src)

target_include_directories(dr-cu-rl
    PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/spdlog/include
    ${CPPZMQ_DIR}
    ${MSGPACK_DIR}/include
    ${SPDLOG_DIR}/include
    ${ZMQ_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/ppo/include
    ${CMAKE_CURRENT_SOURCE_DIR}/dr-cu
    ${CMAKE_CURRENT_SOURCE_DIR}/dr-cu/src
    ${CMAKE_CURRENT_SOURCE_DIR}/dr-cu/rsyn/src
    ${CMAKE_CURRENT_SOURCE_DIR}/dr-cu/rsyn/src/rsyn/export
    ${CMAKE_CURRENT_SOURCE_DIR}/dr-cu/rsyn/include
)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/dr-cu/rsyn/lib/linux)
#set_target_properties(dr-cu-rl PROPERTIES LINK_FLAGS "-static -Wl,--whole-archive -rdynamic -lpthread -Wl,--no-whole-archive")
set_target_properties(dr-cu-rl PROPERTIES LINK_FLAGS "-Wl,--whole-archive -rdynamic -lpthread -Wl,--no-whole-archive")
target_link_libraries(dr-cu-rl PRIVATE cpprl ispd19dr)
