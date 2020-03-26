cmake_minimum_required(VERSION 3.13)
project(webcam_264 C)

set(CMAKE_C_STANDARD 99)

add_executable(yuy2-to-nv12  main.c common.h)
