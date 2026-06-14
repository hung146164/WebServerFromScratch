import json
import os
import random
import time

def generate_vcs_benchmark_data(filename="large_test.json"):
    print(f"[*] Khởi tạo môi trường sinh dữ liệu: {filename}")
    start_time = time.time()

    # 1. BA OBJECT ĐỊNH DANH (Bắt buộc cho C++ Assertions)
    schema_config = {
        "sys_id": "VDT-CORE-GATEWAY-2026",
        "enabled": True, "debug_mode": False, "backup": None,
        "settings": {"timeout": 30, "retry": 3, "protocol": "HTTP/2"}
    }
    schema_metrics = {
        "timestamp": 1771234567, "cpu_load": 0.82, "memory_free_bytes": -1024,
        "latency_history": [12.5, 14.2, 9.1, 105.6, 11.0, 13.4], "status": 200
    }
    schema_info = {
        "title": "KMA Algorithms Club (KAC)",
        "author": "Nguyen Ba Phuc Hung",
        "tags": ["C++", "CMake", "Memory Pool", "Zero-Copy"],
        "modules": [{"name": "JsonParser", "optimized": True}]
    }

    # 2. TẬP DỮ LIỆU BIÊN (Bẫy Parser)
    edge_cases = [
        {"type": "INT64_MAX", "val": 9223372036854775807},
        {"type": "INT64_MIN", "val": -9223372036854775808},
        {"type": "FLOAT_MIN", "val": 2.2250738585072014e-308},
        {"type": "ESCAPE_HELL", "val": "C:\\\\Sys32\\\\config\\n\\t\"Payload\"\\b\\f"},
        {"type": "UNICODE", "val": "Viettel Cyber Security 🛡️ ⚔️"},
        {"type": "DEEP_NEST", "val": [[[[[{"core": "reached"}]]]]]}
    ]

    target_bytes = 50 * 1024 * 1024 # 50 MB
    total_objects = 3

    with open(filename, 'w', encoding='utf-8') as f:
        f.write("[\n")
        # Ghi 3 Header Objects
        f.write(json.dumps(schema_config) + ",\n")
        f.write(json.dumps(schema_metrics) + ",\n")
        f.write(json.dumps(schema_info))

        # Nhồi payload cho đến khi đạt target size
        while True:
            f.write(",\n")
            
            # Tạo chuỗi rác dài từ 1KB - 5KB để ép Memory Pool hoạt động mạnh
            buffer_size = random.randint(1024, 5120)
            junk_buffer = "A" * buffer_size
            
            payload = {
                "obj_id": total_objects,
                "edge": random.choice(edge_cases),
                "buffer": junk_buffer
            }
            
            f.write(json.dumps(payload, ensure_ascii=False))
            total_objects += 1
            
            # Flush và check size định kỳ để giảm I/O bottleneck của Python
            if total_objects % 2000 == 0:
                f.flush()
                current_mb = os.path.getsize(filename) / (1024 * 1024)
                print(f" -> Đang tiến hành: {current_mb:.2f} MB / 50.00 MB", end='\r')
                if current_mb >= 50.0:
                    break

        f.write("\n]")

    elapsed = time.time() - start_time
    actual_size_mb = os.path.getsize(filename) / (1024 * 1024)
    
    print(f"\n[+] Hoàn tất sinh dữ liệu trong {elapsed:.2f}s!")
    print(f"    - File name: {filename}")
    print(f"    - Kích thước: {actual_size_mb:.2f} MB")
    print(f"    - TỔNG SỐ OBJECTS: {total_objects}")
    print(">>> HÃY COPY TỔNG SỐ OBJECTS NÀY VÀO FILE C++ ĐỂ TÍNH LATENCY! <<<")

generate_vcs_benchmark_data()