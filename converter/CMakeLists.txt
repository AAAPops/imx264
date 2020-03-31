cmake_minimum_required(VERSION 3.13)
project(webcam_264 C)

set(CMAKE_C_STANDARD 99)

#set(CMAKE_C_FLAGS "-mthumb -O0 -mtune=cortex-a9 -mfpu=neon -mvectorize-with-neon-quad")
set(CMAKE_C_FLAGS "-mtune=cortex-a9 -mfpu=neon -mvectorize-with-neon-quad")


add_executable(yuy2-to-nv12  main.c common.h)


#add_executable(neon-tst  neon-tst.c)
