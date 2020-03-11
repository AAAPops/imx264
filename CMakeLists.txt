cmake_minimum_required(VERSION 3.13)
project(webcam_264 C)

set(CMAKE_C_STANDARD 99)

set(SOURCE webcam.c)
set(HEADER webcam.h)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(webcam_x264 ${SOURCE})
target_link_libraries(webcam_x264  Threads::Threads)


add_executable(v4l2-example  v4l2-example.c)
target_link_libraries(v4l2-example  Threads::Threads)
