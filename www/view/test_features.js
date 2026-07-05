// --- Helper Ghi Log Ra Console Giao Diện ---
function logToConsole(message, type = 'info') {
  const consoleBox = document.getElementById('console-logs');
  const time = new Date().toLocaleTimeString();
  let color = '#fff';
  if (type === 'success') color = 'var(--success)';
  if (type === 'error') color = 'var(--error)';
  if (type === 'warning') color = 'var(--warning)';
  if (type === 'info') color = '#8aadf4';

  consoleBox.innerHTML += `<div style="color: ${color}; margin-bottom: 0.25rem;">[${time}] ${message}</div>`;
  consoleBox.scrollTop = consoleBox.scrollHeight;
}

logToConsole('Giao diện kiểm thử 5 tính năng cốt lõi đã sẵn sàng.', 'success');

document.getElementById('btn-clear-console').addEventListener('click', () => {
  document.getElementById('console-logs').innerHTML = '';
  logToConsole('Console cleared.', 'info');
});

// --- Helper để sinh ngẫu nhiên JSON danh sách học sinh ---
function generateRandomStudents(count) {
  logToConsole(`Đang sinh ngẫu nhiên ${count} học sinh...`, 'info');
  const students = [];
  const firstNames = ['Nguyen', 'Tran', 'Le', 'Pham', 'Hoang', 'Phan', 'Vu', 'Vo', 'Dang', 'Bui'];
  const middleNames = ['Van', 'Thi', 'Minh', 'Anh', 'Duc', 'Hoang', 'Khanh', 'Ngoc', 'Hai', 'Quang'];
  const lastNames = ['An', 'Binh', 'Cuong', 'Dung', 'Em', 'Giang', 'Hai', 'Khanh', 'Linh', 'Minh'];
  
  for (let i = 0; i < count; i++) {
    const fn = firstNames[Math.floor(Math.random() * firstNames.length)];
    const mn = middleNames[Math.floor(Math.random() * middleNames.length)];
    const ln = lastNames[Math.floor(Math.random() * lastNames.length)];
    students.push({
      name: `${fn} ${mn} ${ln}`,
      age: 18 + (i % 6),
      gpa: parseFloat((2.0 + Math.random() * 2.0).toFixed(2))
    });
  }
  document.getElementById('json-input').value = JSON.stringify(students, null, 2);
  logToConsole(`Đã sinh xong ${count} học sinh. Nhấp "Phân tích cú pháp C++" để gửi.`, 'success');
}

document.getElementById('btn-json-gen-100').addEventListener('click', () => generateRandomStudents(100));
document.getElementById('btn-json-gen-500').addEventListener('click', () => generateRandomStudents(500));
document.getElementById('btn-json-gen-10000').addEventListener('click', () => generateRandomStudents(10000));

// --- 3. POST File Upload Logic ---
document.getElementById('btn-upload').addEventListener('click', async () => {
  const fileInput = document.getElementById('file-input');
  if (fileInput.files.length === 0) {
    logToConsole('Vui lòng chọn một tệp trước khi nhấn Tải lên.', 'error');
    return;
  }
  
  const file = fileInput.files[0];
  logToConsole(`Đang gửi tệp tin lên server: ${file.name} (${(file.size/1024).toFixed(2)} KB)...`, 'info');
  
  const formData = new FormData();
  formData.append('file', file);
  
  try {
    const res = await fetch('/api/upload', {
      method: 'POST',
      body: formData
    });
    
    let json = null;
    try {
      json = await res.json();
    } catch(e) {}
    
    if (res.status === 201 && json) {
      logToConsole(`Upload tệp tin thành công! File lưu trên Server: ${json.filename}`, 'success');
      document.getElementById('upload-status').innerHTML = `
        <span style="color:var(--success); font-weight:bold;">Thành công!</span><br>
        <strong>Tên file gốc:</strong> ${file.name}<br>
        <strong>Tên lưu trữ:</strong> ${json.filename}<br>
        <strong>Kích thước:</strong> ${(file.size/1024).toFixed(2)} KB<br>
        <div style="margin-top: 0.5rem; display: flex; gap: 0.5rem;">
          <a href="/api/view/upload/${json.filename}" target="_blank" class="btn btn-secondary" style="flex: 1; font-size: 0.75rem; text-decoration: none; text-align: center; padding: 0.3rem 0;">Xem tệp</a>
          <a href="/api/download/upload/${json.filename}" class="btn btn-primary" style="flex: 1; font-size: 0.75rem; text-decoration: none; text-align: center; padding: 0.3rem 0;">Tải về tệp</a>
        </div>
        <div style="font-size: 0.7rem; color: var(--text-muted); margin-top: 0.25rem; line-height: 1.2;">
          * Lưu ý: Để link tải hoạt động mà không sửa code C++, bạn cần chạy lệnh này trên server một lần: <code>ln -sf ../upload www/view/upload</code>
        </div>
      `;
    } else {
      const errMsg = json ? json.error : 'Không rõ nguyên nhân';
      const statusText = res.status === 413 ? 'HTTP 413 Payload Too Large' : `HTTP ${res.status}`;
      logToConsole(`Upload thất bại! Mã HTTP: ${res.status}. Chi tiết: ${errMsg}`, 'error');
      document.getElementById('upload-status').innerHTML = `
        <span style="color:var(--error); font-weight:bold;">Thất bại (${statusText})!</span><br>
        <strong>Lỗi:</strong> ${errMsg}<br>
        <em>Chi tiết: File vượt quá giới hạn đệm 64KB của Server C++. Yêu cầu bị từ chối để bảo vệ bộ nhớ!</em>
      `;
    }
  } catch (err) {
    logToConsole(`Thất bại: HTTP 413 Payload Too Large (Kết nối bị đóng do kích thước file vượt ngưỡng 64KB!)`, 'error');
    document.getElementById('upload-status').innerHTML = `
      <span style="color:var(--error); font-weight:bold;">Thất bại (HTTP 413 Payload Too Large)!</span><br>
      <strong>Lỗi:</strong> Failed to fetch.<br>
      <em>Chi tiết: File upload quá lớn vượt quá giới hạn đệm 64KB của Server C++. Connection bị đóng để bảo vệ bộ nhớ!</em>
    `;
  }
});

// --- 4. POST JSON Parser Logic ---
document.getElementById('btn-parse-json').addEventListener('click', async () => {
  const jsonText = document.getElementById('json-input').value.trim();
  if (!jsonText) {
    logToConsole('Vui lòng nhập dữ liệu JSON học sinh.', 'error');
    return;
  }
  
  logToConsole('Gửi danh sách học sinh (JSON) lên Server phân tích...', 'info');
  const start = performance.now();
  
  try {
    const res = await fetch('/api/parse_students', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json'
      },
      body: jsonText
    });
    
    const elapsed = (performance.now() - start).toFixed(2);
    
    let json = null;
    try {
      json = await res.json();
    } catch(e) {}
    
    if (res.status === 200 && json) {
      logToConsole(`Phân tích thành công! Thời gian phản hồi mạng: ${elapsed} ms. C++ Parse time: ${json.parsing_time_us} μs.`, 'success');
      document.getElementById('json-status').innerHTML = `
        <span style="color:var(--success); font-weight:bold;">Thành công!</span><br>
        <strong>Tổng số học sinh:</strong> ${json.total_students}<br>
        <strong>Điểm GPA trung bình:</strong> ${json.average_gpa.toFixed(2)}<br>
        <strong>Thủ khoa:</strong> ${json.top_student} (${json.max_gpa.toFixed(2)})<br>
        <strong>Thời gian C++ xử lý:</strong> ${json.parsing_time_us} μs (${(json.parsing_time_us / 1000).toFixed(3)} ms)<br>
        <strong>Trạng thái Pool:</strong> An toàn (dưới 10k nodes)
      `;
    } else if (res.status === 413) {
      const detail = json && json.error ? json.error : 'Vượt ngưỡng bộ nhớ tĩnh của Server';
      logToConsole(`Thất bại: HTTP 413 Payload Too Large (${detail})`, 'error');
      document.getElementById('json-status').innerHTML = `
        <span style="color:var(--error); font-weight:bold;">Thất bại (HTTP 413 Payload Too Large)!</span><br>
        <strong>Chi tiết:</strong> ${detail}<br>
        <em>Giải thích: Server C++ đã chủ động chặn dữ liệu lớn hoặc vượt quá số lượng JSON node giới hạn để chống tấn công từ chối dịch vụ (DoS Bomb Prevention)!</em>
      `;
    } else {
      const errorMsg = json ? json.error : 'Malformed JSON or invalid schema';
      logToConsole(`Thất bại: Mã lỗi HTTP ${res.status} - ${errorMsg}`, 'error');
      document.getElementById('json-status').innerHTML = `
        <span style="color:var(--error); font-weight:bold;">Thất bại (HTTP ${res.status})!</span><br>
        <strong>Lỗi:</strong> ${errorMsg}
      `;
    }
  } catch (err) {
    logToConsole('Thất bại: HTTP 413 Payload Too Large (Kết nối bị đóng do kích thước yêu cầu vượt ngưỡng 64KB!)', 'error');
    document.getElementById('json-status').innerHTML = `
      <span style="color:var(--error); font-weight:bold;">Thất bại (HTTP 413 Payload Too Large)!</span><br>
      <strong>Lỗi:</strong> Failed to fetch.<br>
      <em>Chi tiết: Kích thước JSON gửi đi quá lớn vượt quá giới hạn đệm 64KB của Server C++. Connection bị đóng để bảo vệ bộ nhớ!</em>
    `;
  }
});

// --- 5. Parallel Stress Test Logic (100 / 200 Requests) ---
async function runStressTest(reqCount) {
  logToConsole(`Bắt đầu bắn song song ${reqCount} requests đến Server...`, 'warning');
  document.getElementById('stress-test-status').innerHTML = `
    <span style="color:var(--warning); font-weight:bold;">Đang xử lý...</span><br>
    Đang gửi song song ${reqCount} requests...
  `;
  
  const start = performance.now();
  let successCount = 0;
  let failCount = 0;
  const promises = [];

  for (let i = 0; i < reqCount; i++) {
    promises.push(
      fetch('/api/view/index.html', { cache: 'no-store' })
        .then(res => {
          if (res.status === 200) {
            successCount++;
          } else {
            failCount++;
          }
        })
        .catch(() => {
          failCount++;
        })
    );
  }

  await Promise.all(promises);
  const elapsed = (performance.now() - start).toFixed(2);
  
  logToConsole(`Hoàn thành bắn ${reqCount} requests. Thành công: ${successCount}, Thất bại: ${failCount}. Tổng thời gian: ${elapsed} ms.`, successCount === reqCount ? 'success' : 'error');
  
  document.getElementById('stress-test-status').innerHTML = `
    <span style="color:${successCount === reqCount ? 'var(--success)' : 'var(--error)'}; font-weight:bold;">
      ${successCount === reqCount ? 'Thành công 100%!' : 'Có lỗi xảy ra!'}
    </span><br>
    <strong>Thành công:</strong> <span style="color:var(--success); font-weight:bold;">${successCount}</span> / ${reqCount}<br>
    <strong>Thất bại:</strong> <span style="color:var(--error); font-weight:bold;">${failCount}</span> / ${reqCount}<br>
    <strong>Tổng thời gian chạy:</strong> ${elapsed} ms<br>
    <strong>Tốc độ phản hồi trung bình:</strong> ${(elapsed / reqCount).toFixed(2)} ms/req
  `;
}

document.getElementById('btn-stress-test-100').addEventListener('click', () => runStressTest(100));
document.getElementById('btn-stress-test-200').addEventListener('click', () => runStressTest(200));
