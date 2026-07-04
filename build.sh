#!/bin/bash
# Script build dự án WebServer nhanh từ thư mục gốc

echo "[*] Đang cấu hình CMake (Release mode)..."
cmake -B build -DCMAKE_BUILD_TYPE=Release

if [ $? -eq 0 ]; then
    echo "[*] Đang biên dịch dự án với $(nproc) luồng..."
    cmake --build build -j$(nproc)
    if [ $? -eq 0 ]; then
        echo ""
        echo "========================================="
        echo " [+] BIÊN DỊCH THÀNH CÔNG!"
        echo " [*] File chạy nằm tại: ./build/webserver"
        echo "-----------------------------------------"
        echo " 👉 Để chạy CÓ xuất log ra màn hình:"
        echo "    ./build/webserver"
        echo ""
        echo " 👉 Để chạy KHÔNG xuất log (tối ưu hiệu năng):"
        echo "    ./build/webserver > /dev/null 2>&1"
        echo "========================================="
    else
        echo "[-] LỖI BIÊN DỊCH!"
        exit 1
    fi
else
    echo "[-] LỖI CẤU HÌNH CMAKE!"
    exit 1
fi
