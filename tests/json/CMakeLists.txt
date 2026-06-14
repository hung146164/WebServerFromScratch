cmake_minimum_required(VERSION 3.10)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME OR NOT CMAKE_PROJECT_NAME)
    project(MyModuleUnitTests)
    set(CMAKE_CXX_STANDARD 17)
endif()

# ==========================================
# 1. QUÉT FILE (Dùng đường dẫn tuyệt đối cho an toàn)
# ==========================================
# Quét các file .cpp logic ở thư mục src bên ngoài
file(GLOB ALL_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/../../src/common/Json/*.cpp")

# Quét các file test .cpp trong thư mục json/
file(GLOB ALL_TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# ==========================================
# 2. TẠO FILE CHẠY TEST
# ==========================================
add_executable(run_all_tests ${ALL_TEST_FILES} ${ALL_SRC_FILES})

# ==========================================
# 3. CHỈ ĐƯỜNG DẪN ĐẾN THƯ MỤC INCLUDE BÊN NGOÀI
# ==========================================
target_include_directories(run_all_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")

# ==========================================
# 4. Tăng tốc
# ==========================================
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native")
set(CMAKE_BUILD_TYPE Release)
