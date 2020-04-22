cmake_minimum_required(VERSION 3.13)
project(webcam_264)

set(CMAKE_C_STANDARD 99)

set(SOURCE          main.c args.c webcam.c server.c coda960.c proto.c log.c)
set(HEADER common.h        args.h webcam.h server.h coda960.h proto.h log.h)

set(CMAKE_C_FLAGS "-mtune=cortex-a9 -mfpu=neon")
add_definitions(-DLOG_USE_COLOR)

#set(THREADS_PREFER_PTHREAD_FLAG ON)
#find_package(Threads REQUIRED)

add_executable(webcam_x264 ${SOURCE})
#target_link_libraries(webcam_x264  Threads::Threads)


add_subdirectory(proto)


