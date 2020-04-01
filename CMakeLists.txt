cmake_minimum_required(VERSION 3.13)
project(webcam_264)

set(CMAKE_C_STANDARD 99)

set(SOURCE main.c args.c webcam.c)
set(HEADER common.h webcam.h args.h server.h)

set(CMAKE_C_FLAGS "-mtune=cortex-a9 -mfpu=neon")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(webcam_x264 ${SOURCE})
target_link_libraries(webcam_x264  Threads::Threads)


#add_executable(v4l2-example  v4l2-example.c)
#target_link_libraries(v4l2-example  Threads::Threads)

#add_executable(imx264-encode  imx264-encode.c)
#target_link_libraries(imx264-encode  Threads::Threads)


#add_subdirectory(converter)


