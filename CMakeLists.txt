cmake_minimum_required (VERSION 2.6)

project (yuv_rgb)
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wall -Wextra -pedantic -std=c99")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic -std=c99")

set(CMAKE_BUILD_DIRECTORY ${PROJECT_SOURCE_DIR}/build)
set(CMAKE_CACHEFILE_DIR ${PROJECT_SOURCE_DIR}/build)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/build)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/build)


set(USE_FFMPEG FALSE CACHE BOOL "Enable ffmpeg")
if(USE_FFMPEG)
	add_definitions(-DUSE_FFMPEG=1)
endif(USE_FFMPEG)

set(USE_IPP FALSE CACHE BOOL "Enable IPP")

if(USE_IPP)
	set(IPP_ROOT /opt/intel CACHE PATH "IPP install path")
	
	include_directories(${IPP_ROOT}/ipp/include)
	link_directories(${IPP_ROOT}/ipp/lib/intel64)
	add_definitions(-DUSE_IPP=1)
endif(USE_IPP)

add_executable(test_yuv_rgb test_yuv_rgb.c yuv_rgb.c)

if(USE_FFMPEG)
target_link_libraries(test_yuv_rgb swscale)
endif(USE_FFMPEG)

if(USE_IPP)
	target_link_libraries(test_yuv_rgb ippcc)
endif(USE_IPP)

add_executable(bench bench/bench.c yuv_rgb.c)
target_link_libraries(bench swscale avformat avcodec avutil )
