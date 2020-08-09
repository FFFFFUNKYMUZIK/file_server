cmake_minimum_required(VERSION 3.17)
project(file_server C)

add_executable(file_server server.c wthr.h config.h wthr.c msg.h sock.c filemeta.h filemeta.c fileinfo.h sock.h msg_queue.c msg_queue.h ipc_msg.h)

include_directories("/usr/include/")
target_link_libraries(file_server "/usr/lib/x86_64-linux-gnu/libaio.so")

set(CMAKE_C_STANDARD 11)

