cmake_minimum_required(VERSION 3.10)
project(WebServerVdt)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

set(SOURCES
    src/main.cpp
    src/network/NetworkServer.cpp
    src/network/Worker.cpp
    src/server/http/HttpRequest.cpp
    src/server/http/Router.cpp
    src/common/Json/JsonDocument.cpp
    src/common/Json/JsonParser.cpp
)

add_executable(webserver ${SOURCES})

add_subdirectory(tests/json)
add_subdirectory(tests/http)