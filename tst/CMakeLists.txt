cmake_minimum_required(VERSION 3.13)
project(tst)

set(CMAKE_C_STANDARD 99)


add_definitions(-DLOG_USE_COLOR)

add_executable(pipe-test pipe-test.c  ../log.c)





