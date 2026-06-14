cmake_minimum_required(VERSION 3.10)
project(MyBigProject)

# Cấu hình chung cho toàn dự án sử dụng C++ 17
set(CMAKE_CXX_STANDARD 17)

# ... (Nơi đây có thể chứa cấu hình để build ra file chạy chính của app) ...

# KÍCH HOẠT THƯ MỤC TEST
# Lệnh này bảo CMake: "Hãy nhảy vào thư mục 'test' và chạy file CMakeLists.txt ở trong đó giúp ta"
add_subdirectory(test)