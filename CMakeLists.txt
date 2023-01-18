cmake_minimum_required(VERSION 3.17)

# 注意 FFmpegPro是工程名称，开发这需要按照实际进行替换
project(ffwasm)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# set(CMAKE_BUILD_TYPE "Debug")

# if (CMAKE_BUILD_TYPE STREQUAL "Debug")
#     add_definitions(-DDEBUG)
#     add_definitions(-DMOGIC_DEBUG)
#     set(CMAKE_C_FLAGS_DEBUG   "-O0")
#     set(CMAKE_CXX_FLAGS_DEBUG "-O0")
# else ()
#     set(CMAKE_C_FLAGS_RELEASE   "-O2")
#     set(CMAKE_CXX_FLAGS_RELEASE "-O2")
# endif ()

# FFmpeg的安装目录，可以通过命令"brew info ffmpeg"获取
set(FFMPEG_DIR /usr/local/Cellar/ffmpeg/4.4.1_5)
# 头文件搜索路径
include_directories(${FFMPEG_DIR}/include/)
# 动态链接库或静态链接库的搜索路径
link_directories(${FFMPEG_DIR}/lib/)

add_executable(ffwasm src/main.cpp src/ffdecode.cpp)

#链接库
target_link_libraries(ffwasm 
        avcodec avfilter avformat avutil swresample swscale
        )