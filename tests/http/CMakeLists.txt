cmake_minimum_required(VERSION 3.10)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME OR NOT CMAKE_PROJECT_NAME)
    project(HTTPUnitTests)
    set(CMAKE_CXX_STANDARD 17)
endif()

find_package(Threads REQUIRED)

set(HTTP_TEST_SOURCES
    test_HttpParser.cpp
    ../../src/server/http/HttpRequest.cpp
    ../../src/server/http/Router.cpp
    ../../src/common/Json/JsonDocument.cpp
    ../../src/common/Json/JsonParser.cpp
)

add_executable(testhttp ${HTTP_TEST_SOURCES})

target_include_directories(testhttp PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")

target_link_libraries(testhttp PRIVATE Threads::Threads)

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native")
set(CMAKE_BUILD_TYPE Release)

# Tu dong chay script python sinh file request 10MB sau khi compile
add_custom_command(
    TARGET testhttp POST_BUILD
    COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/generate_http_tests.py" 10
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Auto-generating 10MB HTTP request payload inside tests/http directory..."
)
