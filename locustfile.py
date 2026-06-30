import random
from locust import HttpUser, task, between

class WebServerVdtUser(HttpUser):
    # Thời gian nghỉ ngẫu nhiên từ 0.5 đến 2 giây giữa các request của mỗi user ảo
    wait_time = between(0.5, 2.0)

    @task(5)
    def view_homepage(self):
        """1. Test GET xem trang chủ index.html"""
        self.client.get("/")

    @task(3)
    def view_pdf(self):
        """2. Test GET xem trực tiếp file PDF (222.pdf)"""
        self.client.get("/api/view/222.pdf")

    @task(3)
    def download_pdf(self):
        """3. Test GET tải file PDF về máy (222.pdf)"""
        self.client.get("/api/download/222.pdf")

    @task(3)
    def view_image(self):
        """4. Test GET xem trực tiếp ảnh (333.jpg)"""
        self.client.get("/api/view/333.jpg")

    @task(3)
    def download_image(self):
        """5. Test GET tải file ảnh về máy (333.jpg)"""
        self.client.get("/api/download/333.jpg")

    @task(4)
    def view_css(self):
        """6. Test GET xem trực tiếp file stylesheet (style.css)"""
        self.client.get("/api/view/style.css")

    @task(3)
    def download_css(self):
        """7. Test GET tải file stylesheet về máy (style.css)"""
        self.client.get("/api/download/style.css")

    @task(2)
    def upload_file(self):
        """8. Test POST upload file (10KB)"""
        # Tạo dữ liệu giả lập 10KB trong RAM để gửi lên
        file_content = b"A" * 10000
        files = {
            "file": ("locust_test.txt", file_content, "text/plain")
        }
        self.client.post("/api/upload", files=files)

    @task(3)
    def parse_students_json(self):
        """9. Test POST gửi JSON học sinh phân tích cú pháp C++"""
        # Tạo dữ liệu ngẫu nhiên mô phỏng hành vi sinh học sinh của Frontend
        students_data = [
            {"name": f"Hoc Sinh {i}", "age": 18 + (i % 5), "gpa": round(2.0 + (random.random() * 2.0), 2)}
            for i in range(20)
        ]
        self.client.post("/api/parse_students", json=students_data)
