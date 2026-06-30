# WebServerVdt - Máy chủ Web hiệu năng cao bằng C++ từ con số 0

Dự án xây dựng một Web Server hiệu năng cao viết bằng ngôn ngữ C++ (chuẩn C++17) từ con số 0, không sử dụng bất kỳ thư viện hoặc mạng ngoài nào (chỉ sử dụng thư viện tiêu chuẩn C++). Máy chủ hoạt động dựa trên mô hình xử lý bất đồng bộ không chặn (Non-blocking I/O) kết hợp vòng lặp sự kiện `epoll` của Linux và kiến trúc Multi-Reactor Multi-Thread.

## 🚀 Tính năng cốt lõi

1. **Xem trực tiếp file tĩnh (View File - inline)**: Hỗ trợ phân tích MIME type và truyền file nhị phân trực tiếp (`.html`, `.css`, `.js`, `.png`, `.jpg`, `.pdf`,...) để trình duyệt hiển thị trực tiếp.
2. **Tải file về máy (Download File - attachment)**: Kích hoạt hộp thoại tải xuống ở Client cho tất cả các loại tệp tin tĩnh.
3. **Tải file lên máy chủ (Upload File - POST Multipart)**: Hỗ trợ bóc tách định dạng dữ liệu form `multipart/form-data` hoặc tải nhị phân thô, validate định dạng an toàn (whitelist) và lưu trữ tệp lên đĩa cứng. Giới hạn dung lượng an toàn để chống DoS.
4. **Phân tích cú pháp JSON (JSON Parser - POST)**: Tích hợp bộ phân tích cú pháp JSON viết thủ công (Arena Node Pool) tối ưu bộ nhớ động để tính toán dữ liệu học sinh với tốc độ micro-giây.

## 🏗️ Thiết kế kỹ thuật & Kiến trúc

* **Mô hình lập trình**: Sử dụng mô hình **Multi-Reactor Thread Pool** (mỗi luồng chạy một vòng lặp `epoll` riêng sử dụng cờ `SO_REUSEPORT` ở tầng kernel để phân phối kết nối công bằng).
* **Truyền tải Zero-Copy**: Tích hợp lệnh gọi hệ thống `sendfile` để chuyển dữ liệu trực tiếp từ Page Cache của file sang Socket Send Buffer ở cấp độ kernel, giảm thiểu chi phí copy bộ nhớ và chuyển đổi ngữ cảnh (context switch).
* **Quản lý kết nối (LRU Cache)**: Mỗi worker sở hữu một cấu trúc LRU cache để quản lý vòng đời kết nối. Tự động quét và giải phóng (kick) các kết nối rác hoặc không hoạt động quá thời gian chờ (Idle Timeout) để tiết kiệm tài nguyên.
* **Bảo mật**: Cơ chế chuẩn hóa đường dẫn Canonical Path giải quyết lỗ hổng leo rào thư mục (Path Traversal), giới hạn số lượng kết nối tối đa từ một địa chỉ IP (`max_client_per_ip`) và tích hợp bộ đệm Arena tĩnh chống lỗi tràn bộ nhớ (Memory Exhaustion DoS).

## 📁 Cấu trúc thư mục

* `src/`: Mã nguồn triển khai lớp Server, Worker, Router và Parser.
* `include/`: Các tệp tin tiêu đề khai báo lớp và hàm tiện ích HTTP.
* `www/`: Thư mục chứa tài nguyên tĩnh phục vụ web:
  * `www/view/`: Chứa các trang web tĩnh (`index.html`, `style.css`...) phục vụ hiển thị.
  * `www/download/`: Chứa các file tài liệu và ảnh để tải về.
  * `www/upload/`: Thư mục lưu trữ các file client tải lên.
* `locustfile.py`: Kịch bản kiểm thử hiệu năng bằng Locust.

## 🛠️ Hướng dẫn Biên dịch & Chạy Server

### 1. Yêu cầu hệ thống
* Hệ điều hành Linux (Ubuntu/Debian) hoặc WSL2.
* Trình biên dịch hỗ trợ C++17 (GCC 8.0 trở lên).
* CMake (3.10 trở lên) và Make.

### 2. Biên dịch dự án (Release Mode với tối ưu hóa -O3)
```bash
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

### 3. Cấu hình hệ thống & Khởi chạy
Trước khi chạy, nâng giới hạn số lượng File Descriptor của tiến trình để phục vụ tải cao:
```bash
ulimit -n 65535
```
Khởi chạy Server (mặc định chạy cổng `8081`):
```bash
# Khởi chạy kèm hiển thị log
./webserver

# Hoặc khởi chạy tối ưu hóa hiệu năng (hủy in log console để tránh nghẽn I/O)
./webserver > /dev/null
```

## 📈 Hướng dẫn Kiểm thử hiệu năng (Locust)

Kịch bản kiểm thử Locust được viết sẵn để đo lường hiệu năng đồng thời của 9 API chính (từ tải file, xem file, upload cho đến parse JSON).

### Chạy Locust:
1. Cài đặt Locust:
   ```bash
   pip install locust
   ```
2. Chạy Locust chỉ định file kịch bản:
   ```bash
   locust -f locustfile.py
   ```
3. Truy cập `http://localhost:8089` trên trình duyệt để bắt đầu swarming.
