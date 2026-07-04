#!/bin/bash

SERVER_IP="168.144.41.233"
SERVER_PORT=8081
URL_PREFIX="http://${SERVER_IP}:${SERVER_PORT}"
OUT_FILE="/tmp/BENCHMARK_RESULTS_FULL.txt"
rm -f /tmp/test3_raw.txt

echo "=== CUSTOM CLOUD BENCHMARK RESULTS ===" > $OUT_FILE
echo "Thời gian bắt đầu: $(date)" >> $OUT_FILE
echo "Môi trường: Client -> Server ($SERVER_IP)" >> $OUT_FILE
echo "=================================" >> $OUT_FILE

# Chuẩn bị file dữ liệu JSON
python3 -c '
import json, random, string
rn = lambda: "".join(random.choices(string.ascii_lowercase, k=7)).capitalize()
gen = lambda n: [{"name": rn(), "age": random.randint(18,25), "gpa": round(random.uniform(2.0,4.0),2)} for _ in range(n)]
with open("/tmp/students_100.json", "w") as f: json.dump(gen(100), f)
with open("/tmp/students_1000.json", "w") as f: json.dump(gen(1000), f)
with open("/tmp/students_5000.json", "w") as f: json.dump(gen(5000), f)
'

# Tạo kịch bản Lua phục vụ wrk test POST JSON
cat > /tmp/post_students.lua << 'LUA'
wrk.method = "POST"
wrk.headers["Content-Type"] = "application/json"
local f = io.open("/tmp/students_100.json", "r")
wrk.body = f:read("*a")
f:close()
LUA

echo -n "[*] Kiểm tra kết nối tới server ${SERVER_IP}... "
if curl -s -o /dev/null -w "%{http_code}" "${URL_PREFIX}/api/view/index.html" | grep -E -q "200"; then
    echo "KẾT NỐI THÀNH CÔNG!"
else
    echo "THẤT BẠI! Vui lòng kiểm tra lại Port hoặc Firewall của Server."
    exit 1
fi
sleep 1

# TEST 1
echo ""
echo "=== TEST 1: ĐANG ĐO THROUGHPUT & LATENCY CƠ BẢN (30 giây) ==="
echo "--- TEST 1: Baseline Throughput & Latency (Index) ---" >> $OUT_FILE
wrk -t4 -c200 -d30s --latency "${URL_PREFIX}/api/view/index.html" 2>&1 | tee -a $OUT_FILE
echo "---------------------------------" >> $OUT_FILE
sleep 3

# TEST 2
echo ""
echo "=== TEST 2: ĐO TẢI ĐỒNG THỜI CAO (CONCURRENCY) ==="
read -p "Nhấn [ENTER] để bắt đầu test 10,000 kết nối đọc trang chủ index.html (1.6KB)..."
echo "--- TEST 2a: 10K CCU đọc trang chủ ---" >> $OUT_FILE
wrk -t4 -c10000 -d20s --latency "${URL_PREFIX}/api/view/index.html" 2>&1 | tee -a $OUT_FILE
echo "---------------------------------" >> $OUT_FILE
sleep 5

read -p "Nhấn [ENTER] để bắt đầu test 10,000 kết nối tải file 222.pdf (650KB)..."
echo "--- TEST 2b: 10K CCU tải 222.pdf ---" >> $OUT_FILE
wrk -t4 -c10000 -d20s --latency "${URL_PREFIX}/api/view/222.pdf" 2>&1 | tee -a $OUT_FILE
echo "---------------------------------" >> $OUT_FILE
sleep 5

# TEST 3
echo ""
echo "=== TEST 3: 100 CLIENT TẢI FILE NẶNG 1.5GB ĐỒNG THỜI ==="
echo "🔔 QUAN TRỌNG: Hãy mở tab monitor RAM bên SERVER."
read -p "Nhấn [ENTER] để bắt đầu tải file nặng..."

echo "--- TEST 3: 100 Client tải file 1.5GB đồng thời ---" >> $OUT_FILE
start_time=$(date +%s)
for i in {1..100}; do
    curl -o /dev/null -s -w "Client $i: Speed=%{speed_download} B/s | Time=%{time_total}s\n" "${URL_PREFIX}/api/download/heavy_1.5gb.bin" >> /tmp/test3_raw.txt &
done
echo "[*] Đang đợi 100 client tải xong... HÃY THEO DÕI RAM SERVER NGAY BÂY GIỜ!"
wait
end_time=$(date +%s)
elapsed=$((end_time - start_time))

echo ">> Tất cả client đã tải xong sau ${elapsed} giây."
cat /tmp/test3_raw.txt >> $OUT_FILE
echo "Tổng thời gian hoàn tất 100 client: ${elapsed}s" >> $OUT_FILE
echo "---------------------------------" >> $OUT_FILE

echo ""
read -p "RAM bên tab VPS lúc nãy vọt lên cao nhất là bao nhiêu? (Ví dụ: 1.1GiB, hoặc vẫn giữ nguyên mức RAM lúc idle): " RAM_USAGE
echo "RAM sử dụng lớn nhất khi 100 người tải file 1.5GB: ${RAM_USAGE}" >> $OUT_FILE
sleep 5

# TEST 4
echo ""
echo "=== TEST 4: ĐO HIỆU NĂNG TÍNH NĂNG PARSE JSON ==="
echo "--- TEST 4a: Parse 100 students (Dung lượng nhỏ) ---" >> $OUT_FILE
for i in {1..3}; do
    curl -s -X POST -H "Content-Type: application/json" --data-binary @/tmp/students_100.json "${URL_PREFIX}/api/parse_students" >> $OUT_FILE
done
echo "--- TEST 4b: Parse 1000 students (Dung lượng vừa) ---" >> $OUT_FILE
for i in {1..3}; do
    curl -s -X POST -H "Content-Type: application/json" --data-binary @/tmp/students_1000.json "${URL_PREFIX}/api/parse_students" >> $OUT_FILE
done
echo "--- TEST 4c: Parse 5000 students (Dung lượng lớn) ---" >> $OUT_FILE
for i in {1..3}; do
    curl -s -X POST -H "Content-Type: application/json" --data-binary @/tmp/students_5000.json "${URL_PREFIX}/api/parse_students" >> $OUT_FILE
done

echo "--- TEST 4d: wrk BẮN TẢI POST JSON LIÊN TỤC (Throughput) ---" >> $OUT_FILE
wrk -t4 -c50 -d20s -s /tmp/post_students.lua "${URL_PREFIX}/api/parse_students" 2>&1 | tee -a $OUT_FILE
echo "---------------------------------" >> $OUT_FILE

echo ""
echo "========================================================="
echo "  BENCHMARK HOÀN TẤT!"
echo "  Kết quả chi tiết đã lưu tại: $OUT_FILE"
echo "========================================================="
