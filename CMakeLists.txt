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

# HTTP Server Logic Unit Tests
find_package(Threads REQUIRED)
set(HTTP_TEST_SOURCES
    tests/http/test_HttpParser.cpp
    src/server/http/HttpRequest.cpp
    src/server/http/Router.cpp
    src/common/Json/JsonDocument.cpp
    src/common/Json/JsonParser.cpp
)
add_executable(run_http_tests ${HTTP_TEST_SOURCES})
target_link_libraries(run_http_tests PRIVATE Threads::Threads)