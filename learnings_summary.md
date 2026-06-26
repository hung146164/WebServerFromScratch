# TỔNG HỢP CÁC BÀI HỌC KINH NGHIỆM & TỐI ƯU HÓA HỆ THỐNG
## DỰ ÁN: WEBSERVERVDT (C++17 MULTI-REACTOR)

Tài liệu này tổng hợp toàn bộ các phát hiện kỹ thuật, bài học kinh nghiệm về lập trình hệ thống, tối ưu hóa hiệu năng, bảo mật và an toàn luồng rút ra trong suốt quá trình phát triển và kiểm thử dự án **WebServerVdt** trên môi trường Linux (WSL2).

---

## 1. Hiện Tượng "Cold Start" Khi Bắn Tải & Cách Khắc Phục
### Hiện tượng
Khi vừa khởi động Web Server và lập tức dùng Locust hoặc các công cụ test bắn tải dồn dập (ví dụ: 1,000 users kết nối đồng thời ngay giây đầu tiên), hệ thống rất dễ bị mất gói tin (drop packets), lỗi kết nối (Connection Refused hoặc Connection Timeout). Tuy nhiên, một khi hệ thống đã ổn định ở mức 1,000 users, nếu ta tăng dần tải lên thêm (+50, +100 users), server lại nhận request cực kỳ ổn định và không hề gặp lỗi.

### Nguyên nhân kỹ thuật
1. **Tràn hàng đợi bắt tay TCP (SYN Backlog Overflow) do giới hạn OS:**
   * Trong Linux, hàng đợi cho các kết nối đang trong quá trình bắt tay 3 bước (half-open connections) được quản lý bởi cấu hình kernel `/proc/sys/net/ipv4/tcp_max_syn_backlog`. Ở WSL2 mặc định, giá trị này chỉ là **256**.
   * Khi ta chạy **2 Worker** (có 2 cổng lắng nghe do `SO_REUSEPORT`), 1,000 yêu cầu kết nối SYN đồng thời sẽ chia đều thành 500 yêu cầu trên mỗi hàng đợi. Vì 500 > 256, hàng đợi SYN trên mỗi socket bị tràn lập tức, dẫn tới ~5% gói tin bị drop ngay giây đầu tiên.
   * Khi ta tăng lên **16 Worker** (16 cổng lắng nghe), 1,000 yêu cầu kết nối SYN được chia đều thành 1,000 / 16 ≈ 62 yêu cầu trên mỗi hàng đợi. Vì 62 < 256, không có hàng đợi nào bị tràn, giúp giảm tỉ lệ mất kết nối từ 5% xuống còn dưới 1%.
2. **Hiện tượng Context Switching (Chuyển ngữ cảnh) khi vượt quá nhân CPU:**
   * Mặc dù 16 luồng giải quyết tốt bài toán tràn hàng đợi kết nối lúc đầu, nhưng nếu môi trường WSL2 chỉ được cấp **2 nhân CPU thực tế** (kiểm tra bằng lệnh `nproc`), việc chạy 16 luồng Worker đồng nghĩa với việc các luồng phải liên tục tranh giành thời gian thực thi của CPU.
   * Hệ điều hành phải thực hiện hoán đổi các luồng liên tục (Context Switch). Điều này làm nguội bộ đệm CPU Cache (L1/L2 Cache) và tiêu hao tài nguyên điều phối của hệ điều hành.
   * Do đó, cấu hình tối ưu nhất cho hiệu năng dài hạn (throughput tối đa, latency thấp nhất) là **số luồng Worker bằng đúng số nhân CPU** (2 Worker tương ứng với 2 Core CPU), đồng thời ta tăng giới hạn hàng đợi hệ thống để không bị rớt gói tin lúc đầu.
3. **Độ trễ tăng xung nhịp CPU (CPU Governor Latency):**
   * Hệ điều hành thường chạy cơ chế quản lý năng lượng CPU (CPU Governor) ở chế độ tiết kiệm điện (`powersave` hoặc `ondemand`). Khi idle, xung nhịp CPU được hạ xuống rất thấp.
   * Khi tải tăng đột biến từ 0 lên 1,000, CPU Governor cần một khoảng thời gian trễ (vài chục đến hàng trăm mili-giây) để phát hiện tải cao và đẩy xung nhịp CPU lên mức tối đa (`turbo boost`). Trong thời gian chuyển giao này, các luồng Worker hoạt động chậm hơn bình thường, dẫn tới nghẽn xử lý.
4. **Lỗi trang bộ nhớ khi khởi tạo (Page Faults & Memory Cold Starts):**
   * Khi tiến trình mới khởi chạy, các bộ đệm (`std::vector` phẳng, cấu trúc dữ liệu LRU, mảng chứa `HttpRequest`) tuy đã được cấp phát nhưng chưa được hệ điều hành ánh xạ vật lý vào RAM (chưa ghi đè dữ liệu thực sự).
   * Lần đầu tiên ghi dữ liệu vào các vùng nhớ này sẽ kích hoạt hàng loạt sự kiện **Page Fault** cấp hệ điều hành để ánh xạ trang bộ nhớ vật lý. Sự kiện này bắt buộc CPU phải chuyển đổi chế độ để kernel xử lý, làm tăng trễ cho các request đầu tiên.
5. **Mở rộng hàng đệm của Card mạng ảo (WSL2 vNIC Buffer Auto-scaling):**
   * WSL2 chạy qua một máy ảo Hyper-V. Card mạng ảo (vNIC) và switch ảo giữa Windows và WSL2 tự động điều chỉnh kích thước bộ đệm (buffer) dựa trên lưu lượng thực tế. Một lượng tải cực lớn ập đến bất ngờ sẽ vượt quá bộ đệm chưa kịp giãn nở của card ảo, gây rớt gói tin ở tầng hypervisor.

### Giải pháp khắc phục
* **Tăng Listen Backlog:** Thiết lập tham số backlog trong hàm `listen()` tối thiểu là `1024` hoặc cao hơn.
* **Cấu hình Kernel Linux:** Điều chỉnh các thông số hệ thống của hệ điều hành trong `/etc/sysctl.conf`:
  ```bash
  sysctl -w net.core.somaxconn=10240
  sysctl -w net.ipv4.tcp_max_syn_backlog=10240
  ```
* **Cơ chế Warm-up trước khi Test:** Thay vì bắn ngay lập tức 1,000 users, hãy cấu hình Locust tăng tải từ từ (Spawn Rate: 50 users/giây) hoặc chạy một luồng tải nhẹ (warm-up) khoảng 5-10 giây trước khi tăng tối đa công suất. Điều này giúp CPU đạt xung nhịp đỉnh, OS hoàn thành ánh xạ trang RAM, và các card mạng ảo ổn định hàng đợi.

---

## 2. Sửa Lỗi An Toàn Bộ Nhớ Nghiêm Trọng (`Stack-use-after-scope`)
### Vấn đề
Phát hiện bởi AddressSanitizer (ASan) khi chạy tính năng Hot Reload (`SIGHUP`):
```cpp
std::string_view ebpf_str = doc["enable_ebpf"].ToString();
```
* **Nguyên nhân:** Hàm `ToString()` trả về một đối tượng `std::string` tạm thời (temporary object) trên stack. Việc gán giá trị này vào `std::string_view` chỉ lưu địa chỉ con trỏ trỏ đến vùng đệm của đối tượng tạm thời đó.
* Ngay sau dấu chấm phẩy `;`, đối tượng tạm thời bị hủy bỏ (out of scope), khiến con trỏ của `std::string_view` trở thành **con trỏ lơ lửng (dangling pointer)**. Khi dòng lệnh tiếp theo sử dụng `ebpf_str`, chương trình sẽ đọc vùng nhớ đã bị giải phóng, gây lỗi `stack-use-after-scope` và crash server ngầm.

### Bài học rút ra
* **Quy tắc vàng với `std::string_view`:** Chỉ sử dụng `std::string_view` khi bạn chắc chắn đối tượng sở hữu chuỗi gốc (`std::string` hoặc mảng `char[]`) có vòng đời (lifetime) dài hơn đối tượng view.
* **Khắc phục:** Đổi kiểu dữ liệu sang `std::string` để sao chép dữ liệu và quản lý thời gian sống an toàn trong phạm vi khối lệnh.

---

## 3. Tối Ưu Giải Phóng Bộ Nhớ RAM Tầng Kernel (`SO_LINGER`)
### Vấn đề
Mặc định trong Linux, khi ứng dụng gọi `close(fd)` trên một socket có dữ liệu chưa gửi hết (hoặc do client đột ngột mất mạng, mất sóng Wi-Fi), nhân hệ điều hành (Kernel) sẽ chuyển socket đó thành dạng *Orphan Socket* và tiến hành thử gửi lại dữ liệu (TCP Retransmission) lên tới **15 lần (kéo dài ~13.4 phút)** trước khi dọn dẹp.
* Điều này khiến toàn bộ Send Buffer của socket đó bị giam giữ trong RAM hệ điều hành, dẫn đến hiện tượng RAM của server tăng dần liên tục (Memory Leak ở tầng OS) khi có nhiều client chập chờn tải file lớn.

### Giải pháp tối ưu
Cấu hình tuỳ chọn socket `SO_LINGER` khi đóng kết nối:
```cpp
struct linger sl;
sl.l_onoff = 1;  // Kích hoạt cơ chế linger
sl.l_linger = 0; // Thời gian chờ bằng 0 giây
setsockopt(fd, SOL_SOCKET, SO_LINGER, &sl, sizeof(sl));
close(fd);
```
* **Kết quả:** Kernel lập tức giải phóng toàn bộ bộ nhớ đệm nhận/gửi của socket đó ngay tại thời điểm `close()`. Đồng thời, một gói tin `RST` (Reset) được gửi đi để xóa sạch dấu vết kết nối trên đường truyền, ngăn chặn rò rỉ RAM hệ thống.

---

## 4. Đảm Bảo An Toàn Đa Luồng (Thread-Safety) Cho Ghi Log & Thời Gian
### Vấn đề 1: Lỗi Race Condition thời gian hệ thống
* Hàm `std::localtime` trả về một con trỏ tới cấu trúc tĩnh `struct tm` dùng chung toàn cục. Khi nhiều luồng Worker gọi `std::localtime` đồng thời, dữ liệu thời gian trong bộ đệm tĩnh này sẽ bị ghi đè chéo lẫn nhau, dẫn đến giờ log hiển thị sai lệch hoặc không chính xác.
* **Khắc phục:** Sử dụng hàm chuẩn thread-safe **`localtime_r`** (trong Linux) truyền vào một biến `struct tm` cục bộ nằm trên stack của từng luồng.

### Vấn đề 2: Lỗi ghi đè đệm Console (Console Output Interleaving)
* Việc ghi log ra console bằng cách gọi liên tiếp:
  ```cpp
  std::cout << "[INFO] " << time_str << " " << msg << std::endl;
  ```
  Sẽ khiến hệ điều hành chuyển ngữ cảnh (Context Switch) giữa chừng khi luồng mới in được một nửa dòng log. Kết quả là log của các Worker khác nhau bị trộn lẫn ký tự vào nhau.
* **Khắc phục:** Lắp ghép toàn bộ log thành một đối tượng `std::string` hoàn chỉnh trước, sau đó chỉ gọi `std::cout << full_log_str;` một lần duy nhất. Tiêu chuẩn C++11 đảm bảo việc ghi một chuỗi đơn lẻ ra `std::cout` là nguyên tử (atomic) ở tầng luồng.

---

## 5. Tránh Xung Đột Ghi File Đồng Thời (Concurrent Upload Collisions)
### Vấn đề
Khi nhiều client đồng thời tải lên các tệp tin có tên giống nhau (ví dụ: `image.png`), nếu server lưu trực tiếp vào thư mục tĩnh `/uploads/image.png`, các luồng Worker sẽ ghi đè đè chéo lên file của nhau, làm hỏng dữ liệu và gây lỗi race condition ghi đĩa.

### Giải pháp tối ưu
Tự động thêm tiền tố định danh duy nhất vào trước tên tệp gốc:
`[thời_gian_thực_micro_giây]_[chỉ_số_socket_fd]_[tên_file_gốc]`
* **Kết quả:** Đảm bảo 100% tính duy nhất của tên tệp trên đĩa, loại bỏ hoàn toàn khả năng xung đột ghi file giữa các luồng xử lý song song.

---

## 6. Thiết Kế Cơ Chế Giới Hạn Kết Nối Địa Chỉ IP Không Khóa (Lock-Free IP Limiter)
### Vấn đề
Để chống tấn công DoS, ta cần giới hạn số kết nối đồng thời từ một địa chỉ IP. Nếu sử dụng một bảng băm toàn cục (Global Hash Map) và bảo vệ bằng khóa khóa chung (`std::mutex`), khi hàng ngàn kết nối ập tới, các Worker sẽ phải xếp hàng chờ đợi nhau để lấy khóa (Lock Contention), làm suy giảm nghiêm trọng hiệu năng đa luồng.

### Giải pháp tối ưu
* **Kiến trúc "Share-Nothing" (Không chia sẻ tài nguyên):** Mỗi luồng Worker sở hữu một bảng băm cục bộ `std::unordered_map<std::string, int> ip_connections` riêng biệt.
* Khi luồng Worker `accept` một kết nối mới, nó tự đếm và giới hạn dựa trên bảng băm của riêng mình. Không hề có tài nguyên chung nào được chia sẻ giữa các luồng, do đó **không cần dùng bất kỳ khóa Mutex nào**. Hiệu năng hệ thống chạy song song 100% ở tốc độ tối đa của CPU.

---

## 7. Phân Biệt "Chỉ Số Mảng" và "Socket FD" Trong Cấu Trúc Cache LRU
### Vấn đề
Trong cấu trúc hàng đợi LRU tùy biến (`LRUCustom.h`), mảng chứa liên kết đôi sử dụng chỉ số mảng (index từ `0` đến `capacity-1`) để định vị các nút nội bộ.
* Hàm lấy kết nối cũ nhất `OldestKey()` ban đầu bị lập trình lỗi khi trả về chính chỉ số index đuôi (`tail`) của mảng thay vì trả về `nodes[tail].key` (là giá trị Socket FD thực tế).
* Khi tiến trình dọn dẹp Keep-Alive dọn socket rác, nó lấy nhầm index `0` làm socket FD để đóng. Việc gọi `close(0)` đã vô tình đóng mất File Descriptor `0` (Standard Input - stdin của tiến trình).
* Khi stdin bị đóng, hàm đọc dữ liệu hệ thống liên tục trả về lỗi, khiến Worker rơi vào vòng lặp vô hạn tiêu tốn 100% CPU trên nhân đó và làm sập hoàn toàn khả năng chấp nhận kết nối mới.

### Bài học rút ra
* Phải phân tách rõ ràng giữa cấu trúc lưu trữ nội bộ (array index) và giá trị thực thể quản lý (socket fd).
* Luôn bổ sung kiểm tra điều kiện an toàn (`fd > 0`) trước khi thực hiện các thao tác đóng hệ thống.

---

## 8. Cơ Chế Gửi File Bất Đồng Bộ EPOLLOUT & Chống Tấn Công Slow-Read
### Vấn đề
* **Nghẽn Event Loop:** Nếu gửi file lớn bằng chế độ chặn (blocking) hoặc gửi đồng bộ trong Event Loop, luồng Worker sẽ bị treo cho đến khi file được truyền xong, làm tê liệt việc xử lý các socket khác trên luồng đó.
* **Tấn công Slow-Read:** Kẻ tấn công thiết lập kích thước bộ đệm nhận rất nhỏ (ví dụ: 1 byte) để buộc server phải giữ kết nối và gửi file cực kỳ chậm, làm cạn kiệt tài nguyên file descriptor của hệ thống.

### Giải pháp tối ưu
1. **Gửi file bất đồng bộ qua `EPOLLOUT`:**
   * Khi cần gửi file tĩnh, server ghi nhận lại trạng thái (`file_fd`, `offset`, `remaining`) vào đối tượng kết nối và đăng ký sự kiện `EPOLLOUT`, đồng thời tạm gỡ cờ `EPOLLIN` (Read Suspension) để tránh nhận đè request mới.
   * Khi bộ đệm ghi của socket trống, `epoll_wait` sẽ kích hoạt và Worker sẽ gọi `ContinueSendFile` để gửi một phần dữ liệu bằng `sendfile` cho đến khi gặp lỗi chặn `EAGAIN`.
2. **Lá chắn tốc độ tối thiểu (Minimum Speed Protection):**
   * Đo lường lượng dữ liệu gửi được sau mỗi chu kỳ 5 giây.
   * Nếu tốc độ truyền tải trung bình thấp hơn **5 KB/s**, server sẽ chủ động đóng kết nối ngay lập tức để giải phóng tài nguyên hệ thống, đập tan kiểu tấn công Slow-Read.

---

## 9. Kích Hoạt Đa Nhân Thực Tế Trên WSL2 (WSL Core Configuration)
### Hiện tượng
Server cấu hình chạy 7 Workers nhưng khi log khởi động in ra, các Worker chỉ chạy luân phiên trên CPU Core 0 và Core 1:
```text
[Worker 0] Event Loop started successfully on CPU Core 0
[Worker 1] Event Loop started successfully on CPU Core 1
[Worker 2] Event Loop started successfully on CPU Core 0
...
```
### Nguyên nhân
WSL2 theo mặc định cấu hình trên máy tính Windows chỉ được cấp quyền truy cập vào 2 nhân CPU ảo để tiết kiệm tài nguyên cho hệ điều hành host.

### Cách xử lý
Tạo hoặc chỉnh sửa tệp tin cấu hình WSL toàn cục trên Windows tại đường dẫn:
`C:\Users\<Tên_User>\.wslconfig`
Thêm cấu hình tăng số lượng nhân CPU cho phép sử dụng:
```ini
[wsl2]
processors=8
```
Sau đó, mở PowerShell trên Windows và chạy lệnh tắt hoàn toàn máy ảo WSL để nạp lại:
```powershell
wsl --shutdown
```
Khi khởi động lại WSL2, tiến trình Web Server sẽ phân bổ đều đặn và chạy song song trên 7 nhân CPU độc lập từ Core 0 đến Core 6, nâng cao hiệu năng xử lý song song lên gấp nhiều lần.

---

## 10. Cách Giám Sát Tài Nguyên, CPU & RAM Thực Tế Trong Quá Trình Test
Để báo cáo dự án đạt điểm tối đa trước Hội đồng đánh giá, việc đưa ra các số liệu đo đạc thực tế là bắt buộc. Dưới đây là các câu lệnh chuẩn trên Linux để theo dõi hệ thống:

### Giám sát CPU/RAM tổng quan (Bằng `htop` hoặc `top`)
* Chạy lệnh: `htop`
* Nhấn phím `F6` để sắp xếp theo cột chiếm dụng CPU hoặc bộ nhớ. Giúp quan sát trực quan sự phân bổ tải trên từng Core CPU.

### Kiểm tra rò rỉ bộ nhớ nâng cao (Bằng AddressSanitizer - ASan)
Biên dịch dự án kèm cờ kiểm toán bộ nhớ trong CMake:
```cmake
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
```
* **Cách hoạt động:** Khi chạy server dưới chế độ ASan, nếu xảy ra rò rỉ bộ nhớ (Memory Leak), ghi tràn mảng (Buffer Overflow), hoặc giải phóng bộ nhớ hai lần (Double Free), ASan sẽ ngay lập tức dừng tiến trình và in ra chỉ dẫn dòng code bị lỗi chi tiết 100%.

### Đo đạc băng thông mạng thực tế (Bằng `nload` hoặc `iftop`)
* Cài đặt: `sudo apt install nload`
* Chạy lệnh: `nload` để xem biểu đồ lưu lượng mạng nhận và gửi thời gian thực, đo đạc trực quan băng thông khi thực hiện tải file tĩnh lớn.

---

## 11. Thực Nghiệm So Sánh Hiệu Năng Đa Nhân (6 Core vs 2 Core) Khi Đã Tối Ưu Hàng Đợi Mạng
Sau khi giải quyết triệt để lỗi nghẽn hàng đợi bắt tay bằng cách cấu hình hệ thống nới rộng `tcp_max_syn_backlog`, ta tiến hành đo đạc hiệu năng máy chủ khi cấp phát số lượng nhân CPU khác nhau cho WSL2 để làm rõ mức độ ảnh hưởng của số nhân CPU đối với kiến trúc Multi-Reactor:

### Bảng đo đạc so sánh hiệu năng (Bắn tải 1,000 users đồng thời)

| Chỉ số đo đạc (Locust) | Cấu hình chạy 6 nhân CPU (6 Core) | Cấu hình chạy 2 nhân CPU (2 Core) | Đánh giá so sánh |
| :--- | :--- | :--- | :--- |
| **Tỷ lệ lỗi (Failures)** | 0% (Không lỗi) | 0% (Không lỗi) | Ngang nhau (Do đã tăng backlog) |
| **Độ trễ trung bình (Average Latency)** | **12.53 ms** | **9.09 ms** | **2 Core nhanh hơn 27.5%** |
| **Độ trễ Median (50%ile)** | 6 ms | 5 ms | 2 Core nhanh hơn |
| **Độ trễ 95%ile** | 55 ms | 37 ms | **2 Core nhanh hơn 32.7%** |
| **Độ trễ 99%ile** | 110 ms | 80 ms | **2 Core nhanh hơn 27.2%** |
| **Độ trễ lớn nhất (Max Latency)** | 224 ms | 179 ms | 2 Core nhanh hơn |
| **Tổng Request xử lý thành công** | 35,486 requests | 40,620 requests | 2 Core xử lý nhiều hơn ~14.4% |
| **Tốc độ xử lý (Current RPS)** | ~499.5 RPS | ~499.3 RPS | Ngang nhau (Bị giới hạn bởi Client Locust) |

### Phân tích nguyên nhân tại sao 2 Core chạy nhanh hơn 6 Core:
1. **Cạnh tranh tài nguyên với Client Locust (Locust CPU Starvation):**
   * Do cả Locust (đóng vai trò Client) và Web Server đều chạy trên cùng một máy tính vật lý.
   * Khi server được cấu hình chạy trên 6 nhân, nó chiếm dụng phần lớn tài nguyên CPU của hệ thống host. Locust (chạy bằng Python, vốn bị ảnh hưởng bởi GIL và có độ trễ lập lịch cao) bị chia sẻ ít tài nguyên CPU hơn, dẫn tới việc chính Locust bị trễ trong khâu ghi nhận mốc thời gian nhận gói tin (Timestamping) và gửi đi.
   * Khi server rút gọn về 2 nhân, hệ thống Windows còn lại 4 nhân hoàn toàn rảnh rỗi cho Locust. Locust gửi request và đo đạc phản hồi tức thì mà không gặp hiện tượng nghẽn luồng tại Client.
2. **Hiệu năng gom cụm Epoll (Epoll Batching Efficiency):**
   * Trong kiến trúc Multi-Reactor, các Worker lấy sự kiện mạng ra theo từng lô (Batch).
   * Khi chạy 2 Worker trên 2 Core, lượng connection đổ về mỗi Worker tập trung nhiều hơn (500 connections/Worker). Điều này giúp tăng hiệu quả gộp sự kiện (batching) của hàm `epoll_wait`, giảm số lượng System Call gọi vào Kernel.
   * Khi chạy 6 Worker, kết nối bị phân tán mỏng hơn (chỉ ~166 connections/Worker). `epoll_wait` phải kích hoạt thường xuyên hơn với kích thước lô nhỏ, làm tăng tỷ lệ System Call/Request.
3. **Độ ấm bộ đệm Cache CPU (Cache Locality):**
   * Chạy 2 luồng giúp tập trung toàn bộ dữ liệu HTTP, đệm socket và bảng định tuyến vào Cache L1/L2 của đúng 2 nhân CPU. Tốc độ đọc ghi bộ nhớ gần như tức thời.
   * Chạy 6 luồng làm phân tán vùng nhớ Cache trên 6 core khác nhau, dẫn tới hiện tượng Cache Thrashing hoặc độ trễ trao đổi dữ liệu giữa các nhân (Inter-core communication overhead) tăng lên khi Linux Kernel phải điều phối tài nguyên mạng.
