cmake_minimum_required(VERSION 3.13)
project(webcam_264_proto)

set(CMAKE_C_STANDARD 99)

#set(SOURCE          main.c args.c webcam.c server.c coda960.c)
#set(HEADER common.h        args.h webcam.h server.h coda960.h)



add_definitions(-DLOG_USE_COLOR)

add_executable(v-client v-client.c
        ../server.c
        ../server.h
        ../proto.c
        ../proto.h
        ../log.c)





