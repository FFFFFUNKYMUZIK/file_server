cmake_minimum_required(VERSION 3.17)

project(file_server C)

set(CMAKE_C_STANDARD 11)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories("/usr/include/")

set(HOME_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SRC_DIR ${HOME_DIR}/src)
set(BUILD_DIR ${HOME_DIR}/build)

#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})
set(CMAKE_VERBOSE_MAKEFILE true)

message("HOME DIR : " ${HOME_DIR})
message("SOURCE DIR : " ${SRC_DIR})
message("BUILD DIR : " ${BUILD_DIR})

file(MAKE_DIRECTORY ${BUILD_DIR})
file(MAKE_DIRECTORY ${BUILD_DIR}/server_area)

#include_directories(${SRC_DIR})
set(CLIENT_DIR ${SRC_DIR}/client)
set(SERVER_DIR ${SRC_DIR}/server)
set(COMMON_DIR ${SRC_DIR}/common)

file(GLOB CLIENT_SRC
        ${CLIENT_DIR}/*.c
        ${COMMON_DIR}/*.c
        )

file(GLOB SERVER_SRC
        ${SERVER_DIR}/*.c
        ${COMMON_DIR}/*.c
        )

add_executable(client
        ${CLIENT_SRC})
target_include_directories(client PUBLIC ${CLIENT_DIR} ${COMMON_DIR})
target_link_libraries(client "/usr/lib/x86_64-linux-gnu/libaio.so")
set_target_properties(client
                        PROPERTIES
                        RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})


add_executable(server
        ${SERVER_SRC})
target_include_directories(server PUBLIC ${SERVER_DIR} ${COMMON_DIR})
target_link_libraries(server "/usr/lib/x86_64-linux-gnu/libaio.so")
target_link_libraries(server pthread)
set_target_properties(server
                    PROPERTIES
                    RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR}/server_area)