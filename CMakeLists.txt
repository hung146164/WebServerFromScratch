cmake_minimum_required(VERSION 3.16)
project(WebServerVdt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Threads REQUIRED)

# ─── Thư viện tĩnh lõi của Framework ────────────────────────────────────────
add_library(webservervdt STATIC
    src/network/NetworkServer.cpp
    src/network/Worker.cpp
    src/server/http/HttpRequest.cpp
    src/server/http/Router.cpp
)

target_include_directories(webservervdt
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_options(webservervdt PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
    -Wsign-conversion
    -msse2 # SSE2 cho SIMD JSON parser (có trên mọi CPU x86_64)
)

target_link_libraries(webservervdt PUBLIC Threads::Threads)

# ─── Ứng dụng demo ────────────────────────────────────────────────────────────
add_executable(webserver src/main.cpp)

target_link_libraries(webserver PRIVATE webservervdt)

target_compile_options(webserver PRIVATE -Wall -Wextra)

# ─── Cài đặt (cho người dùng framework) ──────────────────────────────────────
install(TARGETS webservervdt
    ARCHIVE DESTINATION lib
)
install(DIRECTORY include/
    DESTINATION include
)
install(TARGETS webserver
    RUNTIME DESTINATION bin
)

# ─── Unit Tests ──────────────────────────────────────────────────────────────
option(BUILD_TESTING "Build unit tests" ON)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
