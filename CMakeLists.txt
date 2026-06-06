cmake_minimum_required(VERSION 3.16)
project(WebServerVdt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Threads REQUIRED)

add_executable(webserver
    src/main.cpp
    src/NetworkServer.cpp
    src/Worker.cpp
    src/HttpParser.cpp
)

target_include_directories(webserver
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_options(webserver PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
    -Wsign-conversion
)

target_link_libraries(webserver PRIVATE Threads::Threads)

install(TARGETS webserver
    RUNTIME DESTINATION bin
)