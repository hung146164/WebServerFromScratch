#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include "common/Json/JsonParser.h"

int main()
{
    // ==========================================
    // CẤU HÌNH BENCHMARK
    // ==========================================
    std::string filePath = "large_test.json";

    // ĐIỀN CON SỐ TỪ PYTHON SCRIPT VÀO ĐÂY ĐỂ TÍNH TOÁN LATENCY CHÍNH XÁC
    const double TOTAL_OBJECTS_FROM_PYTHON = 10000.0; // Sửa số này sau khi chạy file Python
    const size_t POOL_CAPACITY = 10000000;            // 10 Triệu nodes (An toàn cho 50MB)

    std::cout << "=========================================================\n";
    std::cout << "      VCS BENCHMARK REPORT: ZERO-COPY JSON PARSER        \n";
    std::cout << "=========================================================\n";

    // ==========================================
    // PHA 1: I/O BOUND (KHÔNG TÍNH VÀO TỐC ĐỘ PARSE)
    // ==========================================
    std::cout << "[*] Pha 1: Nạp file vao RAM (" << filePath << ")...\n";
    auto startIO = std::chrono::high_resolution_clock::now();

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "[-] LOI: Khong the mo file du lieu!\n";
        return 1;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string jsonContent = buffer.str();

    auto endIO = std::chrono::high_resolution_clock::now();
    double ioTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(endIO - startIO).count();
    double fileSizeMB = jsonContent.size() / (1024.0 * 1024.0);

    std::cout << "    -> Kich thuoc thuc te: " << std::fixed << std::setprecision(2) << fileSizeMB << " MB\n";
    std::cout << "    -> Thoi gian I/O:      " << ioTimeMs << " ms\n\n";

    // ==========================================
    // PHA 2: CHUẨN BỊ BỘ NHỚ TRONG
    // ==========================================
    std::cout << "[*] Pha 2: Khoi tao Memory Pool (" << POOL_CAPACITY << " nodes)...\n";
    Json::JsonParser parser(POOL_CAPACITY);
    std::string_view bodyView(jsonContent);

    // ==========================================
    // PHA 3: CPU BOUND (CORE BENCHMARK)
    // ==========================================
    std::cout << "[*] Pha 3: Kich hoat Zero-Copy Parsing...\n";
    auto startParse = std::chrono::high_resolution_clock::now();

    Json::JsonDocument doc = parser.Parse(bodyView);

    auto endParse = std::chrono::high_resolution_clock::now();

    // ==========================================
    // PHA 4: TÍNH TOÁN CÁC CHỈ SỐ TOÁN HỌC
    // ==========================================
    // Dùng nanoseconds để đảm bảo độ chính xác tuyệt đối
    double parseTimeNano = std::chrono::duration_cast<std::chrono::nanoseconds>(endParse - startParse).count();
    double parseTimeSec = parseTimeNano / 1e9;
    double parseTimeMs = parseTimeNano / 1e6;

    // Áp dụng công thức Toán học
    double throughput = fileSizeMB / parseTimeSec;
    double latencyPerObj = (parseTimeSec * 1e6) / TOTAL_OBJECTS_FROM_PYTHON;

    std::cout << "\n================ BÁO CÁO HIỆU NĂNG =================\n";
    std::cout << " 1. Core Parsing Time:  " << std::fixed << std::setprecision(4) << parseTimeMs << " ms\n";
    std::cout << " 2. Throughput:         " << std::fixed << std::setprecision(2) << throughput << " MB/s\n";
    std::cout << " 3. Object Latency:     " << std::fixed << std::setprecision(4) << latencyPerObj << " us/object\n";
    std::cout << "======================================================\n";

    // ==========================================
    // PHA 5: DATA INTEGRITY ASSERTION (ĐẢM BẢO KHÔNG LỖI)
    // ==========================================
    std::cout << "\n[*] Pha 4: Kiem tran toan ven du lieu (Data Integrity)...\n";
    try
    {
        std::cout << "    -> Config ID:    " << doc[0]["sys_id"].ToString() << "\n";
        std::cout << "    -> CPU Load:     " << doc[1]["cpu_load"].ToString() << "\n";
        std::cout << "    -> Author:       " << doc[2]["author"].ToString() << "\n";
        std::cout << "    -> Module [0]:   " << doc[2]["modules"][0]["name"].ToString() << "\n";
        std::cout << "[+] PASSED: Toan ven du lieu duoc xac nhan!\n";
    }
    catch (...)
    {
        std::cerr << "[-] FAILED: Truy xuat du lieu that bai (Segfault/Logic Error)!\n";
    }

    return 0;
}