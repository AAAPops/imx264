cmake_minimum_required(VERSION 3.13)
project(v4l2_encode C)

set(CMAKE_C_STANDARD 99)

set(SOURCES _video.c)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(v4l2_encode
        args.c
        args.h
        common.h
        fileops.c
        fileops.h
        gentest.c
        gentest.h
        main.c
        video.c
        video.h)
target_link_libraries(v4l2_encode  Threads::Threads)
