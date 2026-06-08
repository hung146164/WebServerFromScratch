cmake_minimum_required(VERSION 3.16)
project(WebServerVdtTests LANGUAGES CXX)

include(FetchContent)

# ─── Tải tự động Google Test ──────────────────────────────────────────────────
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ─── Tạo Executable chạy Unit Tests ──────────────────────────────────────────
add_executable(webserver_tests
    test_main.cpp
    http/test_HttpParser.cpp
)

# ─── Target settings cho Test Executable ──────────────────────────────────────
target_include_directories(webserver_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src # Để include các internal headers nếu cần
)

target_link_libraries(webserver_tests
    PRIVATE
    webservervdt
    gtest
    gtest_main
)

target_compile_options(webserver_tests PRIVATE -Wall -Wextra)

# Đăng ký với CTest
add_test(NAME AllUnitTests COMMAND webserver_tests)
