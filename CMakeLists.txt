cmake_minimum_required(VERSION 3.24)
project(JpegStreamer)

set(CMAKE_CXX_STANDARD 17)

set(OUR_LIVE555 OFF)

if (OUR_LIVE555)
    add_compile_definitions(NO_OPENSSL OUR_LIVE555)

    include_directories(
            /home/will/Core/ExternalLibraries/Live555/liveMedia/include
            /home/will/Core/ExternalLibraries/Live555/groupsock/include
            /home/will/Core/ExternalLibraries/Live555/BasicUsageEnvironment/include
            /home/will/Core/ExternalLibraries/Live555/UsageEnvironment/include
    )

    add_subdirectory(/home/will/Core/ExternalLibraries/Live555 Live555)
else()
    include_directories(
            /usr/include/liveMedia /usr/include/groupsock /usr/include/BasicUsageEnvironment /usr/include/UsageEnvironment
    )
endif ()

add_executable(JpegStreamer
        JpegFrameParser.cpp
        JpegFrameParser.hh
        JPEGFramedSource.cpp
        JPEGFramedSource.hh
        main.cpp)

if (OUR_LIVE555)
    target_link_libraries(JpegStreamer live555)
else()
    target_link_libraries(JpegStreamer liveMedia groupsock BasicUsageEnvironment UsageEnvironment)
endif ()

