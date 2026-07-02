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
* `include/`: Các tệp tin tiêu đề cấu trúc HTTP.
* `www/`: Thư mục chứa tài nguyên tĩnh phục vụ web:
  * `www/view/`: Chứa các trang web tĩnh (`index.html`, `style.css`...) phục vụ hiển thị.
  * `www/download/`: Chứa các file tài liệu và ảnh để tải về.
  * `www/upload/`: Thư mục lưu trữ các file client tải lên.
* `locustfile.py`: Kịch bản kiểm thử hiệu năng bằng Locust.

## 🛠️ Hướng dẫn Biên dịch & Cấu hình Hệ thống

### 1. Yêu cầu hệ thống
* Hệ điều hành Linux (Ubuntu/Debian) hoặc WSL2.
* Trình biên dịch hỗ trợ C++17 (GCC 8.0 trở lên).
* CMake (3.10 trở lên) và Make.

### 2. Biên dịch dự án (Release Mode với tối ưu hóa -O3)
Thực hiện chạy CMake và Make bên trong thư mục `build`:
```bash
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### 3. Sinh các tệp tin dữ liệu nặng để phục vụ kiểm thử
Vì các file lớn được khai báo trong `.gitignore` để tránh đẩy lên Git, bạn cần tự sinh nhanh các tệp tin này bằng các lệnh sau trước khi chạy test:
```bash
# Trở về thư mục gốc của dự án
cd ..

# Đảm bảo thư mục lưu trữ upload tồn tại
mkdir -p www/upload

# Sinh file test 50MB (chỉ mất 0.1s)
truncate -s 50M www/download/heavy_file.bin

# Sinh file test 1.5GB (chỉ mất 0.1s)
truncate -s 1500M www/download/heavy_1.5gb.bin
```

### 4. Tùy chỉnh cấu hình trong tệp `config.json`
Tệp cấu hình [config.json](config.json) nằm ở thư mục gốc của dự án. Bạn có thể tự do chỉnh sửa các thông số hiệu năng trước khi khởi chạy Server:
* **port**: Cổng mạng Server lắng nghe (mặc định: `8081`).
* **num_workers**: Số lượng luồng xử lý (Reactor threads).
* **client_per_worker**: Số lượng kết nối tối đa mỗi luồng quản lý (LRU Cache Size).
* **read_timeout_sec**: Thời gian chờ ngắt kết nối không hoạt động (idle timeout).
* **rate_limit_per_sec**: Giới hạn số lượng request tối đa/giây từ một địa chỉ IP.

### 5. Cấu hình giới hạn hệ thống để chịu tải tối đa (Ulimit)
Để Server không bị lỗi từ chối kết nối khi chịu tải hàng ngàn users đồng thời, bạn cần mở khóa giới hạn cổng mạng và số lượng tệp mở của Linux:
```bash
# Nâng giới hạn file descriptor cho tiến trình ở terminal hiện tại
ulimit -n 65535

# Tối ưu hóa hàng đợi kết nối của nhân hệ điều hành Linux (Yêu cầu quyền sudo)
sudo sysctl -w net.core.somaxconn=65535
sudo sysctl -w net.ipv4.tcp_max_syn_backlog=65535
```

### 6. Khởi chạy Server
> ⚠️ **QUAN TRỌNG:** Phải khởi chạy file thực thi từ **thư mục gốc của dự án** (không chạy bên trong thư mục `build`) để Server có thể định vị chính xác đường dẫn tương đối của tệp cấu hình `config.json` và thư mục tài nguyên tĩnh `www/`.

Từ thư mục gốc dự án, chạy lệnh:
```bash
# Khởi chạy hiển thị log bình thường
./build/webserver

# Hoặc khởi chạy tối ưu hóa (hủy in log console để tránh nghẽn I/O khi test tải)
./build/webserver > /dev/null 2>&1
```

---

## 🧪 Cơ chế Xác Thực & Hệ thống Kiểm thử Tự động (Testing & Verification)

Dự án triển khai một quy trình kiểm thử nghiêm ngặt nhằm xác minh tính chính xác tuyệt đối của mã nguồn xử lý I/O và Parser (không chỉ dừng lại ở việc chạy thử không lỗi mà phải xác thực ngữ nghĩa dữ liệu).

### 1. Kiểm thử Toàn vẹn Ngữ nghĩa JSON (JSON Parser Validation)
Để xác nhận bộ phân tích cú pháp JSON tự viết bằng C++ (sử dụng Arena Allocator) có khả năng parse chính xác 100% cấu trúc dữ liệu tải nặng mà không bị mất dữ liệu hay lỗi logic:

* **Bước 1: Sinh dữ liệu biên (Edge Cases) bằng Python:**
  Chương trình [sinhtest.py](tests/json/sinhtest.py) tự động tạo file `large_test.json` (50MB) chứa hàng chục ngàn đối tượng ngẫu nhiên chứa các trường hợp bẫy hệ thống:
  * Tràn số nguyên: `INT64_MAX` và `INT64_MIN`.
  * Ký tự đặc biệt (Escape sequence): Các ký tự `\n`, `\t`, `\"`, `\\`.
  * Emojis và ký tự Unicode phức tạp.
  * Cấu trúc lồng sâu 5 cấp (Deep array/object nesting).
* **Cơ chế tính toán Checksum:**
  Trong quá trình sinh file, script Python tính toán cộng dồn giá trị định danh của toàn bộ các đối tượng được ghi ra:
  $$\text{Expected Checksum} = \sum_{i=3}^{N-1} \text{obj-id}_i$$
* **Bước 2: Đối chiếu chéo bằng C++ (`testjson`):**
  Khi chạy `./build/tests/json/testjson`, chương trình C++ nạp file JSON, parse thành cây DOM, duyệt qua toàn bộ cây để tính toán lại Checksum và đếm số phần tử.
  * **Xác thực thành công:** Chương trình so sánh `Calculated Checksum` thu được từ Parser C++ với `Expected Checksum` từ Python. Nếu trùng khớp, in ra thông báo: `[+] PASSED: Toan ven du lieu duoc xac nhan!`.
  * Điều này đảm bảo Parser đã đọc đúng từng thuộc tính, từng ký tự và không bỏ sót bất kỳ node nào trong file 50MB.

### 2. Kiểm thử Bộ phân tích cú pháp HTTP (HTTP Parser Unit Tests)
Tệp tin [test_HttpParser.cpp](tests/http/test_HttpParser.cpp) thực hiện kiểm thử hộp trắng (White-box testing) đối với lớp `HttpRequest` nhằm xác nhận:
* **Tách dòng Request Line**: Phân tích đúng phương thức (GET/POST), đường dẫn URL và phiên bản HTTP (HTTP/1.1).
* **Cắt khoảng trắng Header**: Xác nhận các ký tự khoảng trắng thừa trước và sau khóa/giá trị của Header được loại bỏ chính xác (ví dụ: `Host:  localhost ` -> key: `Host`, val: `localhost`).
* **TCP Stream Compaction**: Kiểm thử xem khi bộ đệm dính gói TCP (chứa dữ liệu của request tiếp theo), hàm `std::memmove` có di chuyển vùng nhớ dính gói về đầu đệm và reset chính xác máy trạng thái FSM để chuẩn bị nhận request kế tiếp hay không.
* **Cách chạy:** Thực thi trực tiếp file biên dịch `./build/tests/http/testhttp`.

---

## 📈 Hướng dẫn Chạy Kiểm Thử Hiệu Năng (Benchmark)

### Cách 1: Kiểm thử bằng Apache Benchmark (Lệnh ab) - Khuyên dùng để đo Max Speed
Công cụ `ab` rất gọn nhẹ và đo đạc cực kỳ chuẩn xác tốc độ xử lý mạng thô.

1. Cài đặt `ab` trên Linux/WSL2:
   ```bash
   sudo apt update && sudo apt install -y apache2-utils
   ```
2. Chạy test tải file PDF 650KB (Giả lập 100 người tải đồng thời, tổng 1000 lượt tải):
   ```bash
   ab -k -c 100 -n 1000 http://127.0.0.1:8081/api/download/222.pdf
   ```
   *(Lưu ý: Bắt buộc phải có cờ `-k` để kích hoạt cơ chế Keep-Alive tương thích với nhân Web Server).*
3. Chạy test tải file 1.5GB khổng lồ (Giả lập 100 người tải đồng thời cùng lúc):
   ```bash
   ab -k -c 100 -n 100 http://127.0.0.1:8081/api/download/heavy_1.5gb.bin
   ```

### Cách 2: Kiểm thử bằng Locust (Giao diện Web UI)
Công cụ giả lập hành vi người dùng thật (đọc tin tức, tải file, gửi JSON học sinh ngẫu nhiên).

1. Cài đặt Locust (Yêu cầu máy đã cài Python):
   ```bash
   pip install locust
   ```
2. Khởi chạy Locust:
   ```bash
   locust -f locustfile.py
   ```
3. Truy cập `http://localhost:8089` trên trình duyệt, điền thông số Users (ví dụ: 1000) và Host là `http://localhost:8081` để bắt đầu swarming.

### Cách 3: Kiểm thử Trực Tiếp trên Giao Diện Web (Web UI Testing)
Sau khi khởi chạy Web Server, bạn có thể thực hiện kiểm nghiệm trực quan tất cả các tính năng thông qua giao diện Web thân thiện:

1. **Truy cập Giao diện**: Mở trình duyệt web của bạn và truy cập địa chỉ:
   ```text
   http://127.0.0.1:8081/
   ```
2. **Kiểm thử các Tính năng**:
   * **Tải tệp tin (Download File)**: Click vào các liên kết tải xuống PDF hoặc JPG để kiểm tra cơ chế truyền dẫn `sendfile` Zero-copy của máy chủ.
   * **Tải lên tệp tin (Upload File)**: Kéo thả một tệp bất kỳ (hình ảnh, tài liệu) vào vùng tải lên và nhấn nút Upload. Server sẽ bóc tách dữ liệu Multipart, lưu file vào thư mục `www/upload/` và phản hồi trạng thái thành công.
   * **Phân tích cú pháp JSON (JSON Parser)**: Nhập dữ liệu JSON học sinh vào ô văn bản, nhấn nút Submit. Máy chủ sẽ chuyển dữ liệu tới API `/api/parse_students`, sử dụng bộ phân tích cú pháp JSON tối ưu hóa bằng Memory Pool để bóc tách thông tin và hiển thị kết quả trực tiếp lên giao diện.
