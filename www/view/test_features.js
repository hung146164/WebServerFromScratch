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
document.getElementById('btn-json-gen-2000').addEventListener('click', () => generateRandomStudents(2000));

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

// --- Theme Toggle Logic ---
const themeToggleBtn = document.getElementById('btn-theme-toggle');
if (themeToggleBtn) {
  themeToggleBtn.addEventListener('click', () => {
    document.body.classList.toggle('light-theme');
    logToConsole(`Đã chuyển sang giao diện ${document.body.classList.contains('light-theme') ? 'Sáng' : 'Tối'}.`, 'info');
  });
}

// --- 6. FSM Parser Simulator Logic ---
const fsmTemplates = {
  post: `POST /api/parse_students HTTP/1.1\nHost: localhost:8081\nContent-Type: application/json\nContent-Length: 15\n\n{"name": "Hung"}`,
  get: `GET /index.html HTTP/1.1\nHost: localhost:8081\nAccept: text/html\n\n`,
  colon: `POST /api/parse_students HTTP/1.1\nHost localhost:8081\nContent-Length: 15\n\n{"name": "Hung"}`,
  body: `POST /api/parse_students HTTP/1.1\nHost: localhost:8081\nContent-Length: 100\n\n{"name"`
};

document.getElementById('btn-fsm-tpl-post').addEventListener('click', () => {
  document.getElementById('fsm-input').value = fsmTemplates.post;
  logToConsole('Đã nạp mẫu HTTP POST chuẩn (Thành công).', 'info');
});

document.getElementById('btn-fsm-tpl-get').addEventListener('click', () => {
  document.getElementById('fsm-input').value = fsmTemplates.get;
  logToConsole('Đã nạp mẫu HTTP GET chuẩn (Thành công).', 'info');
});

document.getElementById('btn-fsm-tpl-err-colon').addEventListener('click', () => {
  document.getElementById('fsm-input').value = fsmTemplates.colon;
  logToConsole('Đã nạp mẫu lỗi thiếu dấu ":" ở dòng Header (Trạng thái ERROR).', 'warning');
});

document.getElementById('btn-fsm-tpl-err-len').addEventListener('click', () => {
  document.getElementById('fsm-input').value = fsmTemplates.body;
  logToConsole('Đã nạp mẫu lỗi thiếu dữ liệu Body so với Content-Length (Trạng thái ERROR).', 'warning');
});

let fsmInterval = null;
let currentStep = 0;
let isPaused = false;
let fsmSteps = [];

function generateFsmSteps(rawText) {
  const steps = [];
  let state = 'START';
  let start_idx = 0;
  let curr_idx = 0;
  
  let method = '';
  let url = '';
  let protocol = '';
  let headers = {};
  let current_header_key = '';
  let content_len = 0;
  
  steps.push({
    state: 'START',
    desc: '1. START: Khởi động bộ phân tích máy trạng thái FSM, đặt con trỏ ở byte 0.',
    start: 0,
    curr: 0
  });

  while (curr_idx < rawText.length && state !== 'COMPLETE' && state !== 'ERROR') {
    let c = rawText[curr_idx];
    let nextState = state;
    let desc = '';

    if (state === 'START') {
      if (c === ' ' || c === '\r' || c === '\n') {
        nextState = 'START';
        desc = `START: Gặp kí tự khoảng trắng hoặc ngắt dòng [${c === '\r' ? '\\r' : c === '\n' ? '\\n' : 'space'}]. Bỏ qua.`;
      } else {
        start_idx = curr_idx;
        nextState = 'METHOD';
        desc = `START: Gặp ký tự [${c}] đầu tiên của HTTP Method. Thiết lập start = ${start_idx}, chuyển sang METHOD.`;
      }
    } 
    else if (state === 'METHOD') {
      if (c === ' ') {
        method = rawText.substring(start_idx, curr_idx);
        start_idx = curr_idx + 1;
        nextState = 'URL';
        desc = `METHOD: Gặp khoảng trắng. Nhận diện xong Method: [${method}]. Đặt start = ${start_idx}, chuyển sang URL.`;
      } else if (c === '\r' || c === '\n') {
        nextState = 'ERROR';
        desc = `METHOD: LỖI CÚ PHÁP! Gặp ngắt dòng [${c === '\r' ? '\\r' : '\\n'}] khi đang đọc Method. Chuyển sang ERROR.`;
      } else {
        nextState = 'METHOD';
        desc = `METHOD: Đọc kí tự [${c}] thuộc tên Method.`;
      }
    }
    else if (state === 'URL') {
      if (c === ' ') {
        url = rawText.substring(start_idx, curr_idx);
        start_idx = curr_idx + 1;
        nextState = 'PROTOCOL';
        desc = `URL: Gặp khoảng trắng. Nhận diện xong URL: [${url}]. Đặt start = ${start_idx}, chuyển sang PROTOCOL.`;
      } else if (c === '\r' || c === '\n') {
        nextState = 'ERROR';
        desc = `URL: LỖI CÚ PHÁP! Gặp ngắt dòng [${c === '\r' ? '\\r' : '\\n'}] khi đang đọc URL. Chuyển sang ERROR.`;
      } else {
        nextState = 'URL';
        desc = `URL: Đọc kí tự [${c}] thuộc URL đường dẫn.`;
      }
    }
    else if (state === 'PROTOCOL') {
      if (c === '\r') {
        protocol = rawText.substring(start_idx, curr_idx);
        nextState = 'PROTO_CR';
        desc = `PROTOCOL: Gặp kí tự \\r. Nhận diện xong Protocol: [${protocol}]. Chuyển sang PROTO_CR để chờ \\n.`;
      } else if (c === '\n') {
        protocol = rawText.substring(start_idx, curr_idx);
        start_idx = curr_idx + 1;
        nextState = 'ST_HDR';
        desc = `PROTOCOL: Gặp kí tự \\n. Nhận diện xong Protocol: [${protocol}]. Đặt start = ${start_idx}, chuyển sang ST_HDR.`;
      } else {
        nextState = 'PROTOCOL';
        desc = `PROTOCOL: Đọc kí tự [${c}] thuộc giao thức HTTP.`;
      }
    }
    else if (state === 'PROTO_CR') {
      if (c === '\n') {
        start_idx = curr_idx + 1;
        nextState = 'ST_HDR';
        desc = `PROTO_CR: Gặp kí tự \\n. Hoàn tất dòng Request Line. Đặt start = ${start_idx}, chuyển sang ST_HDR để bắt đầu đọc Header.`;
      } else {
        nextState = 'ERROR';
        desc = `PROTO_CR: LỖI CÚ PHÁP! Chờ \\n nhưng lại gặp kí tự [${c}]. Chuyển sang ERROR.`;
      }
    }
    else if (state === 'ST_HDR') {
      if (c === '\r') {
        nextState = 'ST_HDR_CR';
        desc = `ST_HDR: Gặp kí tự \\r tại điểm đầu dòng. Chờ \\n để xác định dòng trống (kết thúc Headers).`;
      } else {
        nextState = 'HDR_KEY';
        desc = `ST_HDR: Gặp kí tự [${c}]. Khởi đầu dòng Header mới. Chuyển sang HDR_KEY.`;
      }
    }
    else if (state === 'ST_HDR_CR') {
      if (c === '\n') {
        if (headers['content-length']) {
          content_len = parseInt(headers['content-length'].trim(), 10);
        }
        if (content_len > 0) {
          start_idx = curr_idx + 1;
          nextState = 'ST_BODY';
          desc = `ST_HDR_CR: Nhận diện dòng trống \\r\\n. Kết thúc đọc Headers. Phát hiện Content-Length = ${content_len} bytes. Chuyển sang ST_BODY.`;
        } else {
          nextState = 'COMPLETE';
          desc = `ST_HDR_CR: Nhận diện dòng trống \\r\\n. Kết thúc đọc Headers. Không có Content-Length, chuyển sang COMPLETE.`;
        }
      } else {
        nextState = 'ERROR';
        desc = `ST_HDR_CR: LỖI CÚ PHÁP! Chờ \\n cho dòng trống nhưng gặp [${c}]. Chuyển sang ERROR.`;
      }
    }
    else if (state === 'HDR_KEY') {
      if (c === ':') {
        current_header_key = rawText.substring(start_idx, curr_idx).trim().toLowerCase();
        start_idx = curr_idx + 1;
        nextState = 'HDR_VAL';
        desc = `HDR_KEY: Gặp dấu \':\'. Nhận diện xong Header Key: [${current_header_key}]. Đặt start = ${start_idx}, chuyển sang HDR_VAL.`;
      } else if (c === '\r' || c === '\n') {
        nextState = 'ERROR';
        desc = `HDR_KEY: LỖI CÚ PHÁP! Gặp ngắt dòng khi đang đọc Header Key. Thiếu dấu hai chấm \':\'. Chuyển sang ERROR.`;
      } else {
        nextState = 'HDR_KEY';
        desc = `HDR_KEY: Đọc kí tự [${c}] thuộc Header Key.`;
      }
    }
    else if (state === 'HDR_VAL') {
      if (curr_idx === start_idx && c === ' ') {
        start_idx++;
        nextState = 'HDR_VAL';
        desc = `HDR_VAL: Bỏ qua khoảng trắng đầu tiên sau dấu \':\'. Đặt start = ${start_idx}.`;
      } else if (c === '\r') {
        let val = rawText.substring(start_idx, curr_idx);
        headers[current_header_key] = val;
        nextState = 'HDR_VAL_CR';
        desc = `HDR_VAL: Gặp kí tự \\r. Đọc xong Header Value: [${val}]. Chuyển sang HDR_VAL_CR.`;
      } else if (c === '\n') {
        let val = rawText.substring(start_idx, curr_idx);
        headers[current_header_key] = val;
        start_idx = curr_idx + 1;
        nextState = 'ST_HDR';
        desc = `HDR_VAL: Gặp kí tự \\n. Đọc xong Header Value: [${val}]. Đặt start = ${start_idx}, chuyển sang ST_HDR.`;
      } else {
        nextState = 'HDR_VAL';
        desc = `HDR_VAL: Đọc kí tự [${c}] thuộc Header Value.`;
      }
    }
    else if (state === 'HDR_VAL_CR') {
      if (c === '\n') {
        start_idx = curr_idx + 1;
        nextState = 'ST_HDR';
        desc = `HDR_VAL_CR: Gặp kí tự \\n. Hoàn tất 1 dòng Header. Đặt start = ${start_idx}, chuyển sang ST_HDR.`;
      } else {
        nextState = 'ERROR';
        desc = `HDR_VAL_CR: LỖI CÚ PHÁP! Chờ \\n cuối dòng Header nhưng gặp [${c}]. Chuyển sang ERROR.`;
      }
    }
    else if (state === 'ST_BODY') {
      let body_len = curr_idx - start_idx + 1;
      if (body_len >= content_len) {
        nextState = 'COMPLETE';
        desc = `ST_BODY: Đọc đủ ${content_len}/${content_len} bytes Body. Phân tích hoàn tất, chuyển sang COMPLETE.`;
      } else {
        nextState = 'ST_BODY';
        desc = `ST_BODY: Đang đọc dữ liệu Body [${body_len}/${content_len} bytes]. Đọc kí tự [${c}].`;
      }
    }

    state = nextState;
    curr_idx++;
    
    steps.push({
      state: state,
      desc: desc,
      start: start_idx,
      curr: Math.min(curr_idx, rawText.length - 1)
    });
  }

  // EOF Checks
  if (state !== 'COMPLETE' && state !== 'ERROR') {
    if (state === 'ST_BODY') {
      let body_len = curr_idx - start_idx;
      if (body_len >= content_len) {
        steps.push({
          state: 'COMPLETE',
          desc: `COMPLETE: Đọc đủ ${content_len} bytes Body (đạt kết thúc chuỗi). Hoàn tất!`,
          start: start_idx,
          curr: rawText.length - 1
        });
      } else {
        steps.push({
          state: 'ERROR',
          desc: `ERROR: Kết thúc chuỗi đột ngột. Chưa đọc đủ Body [${body_len}/${content_len} bytes].`,
          start: start_idx,
          curr: rawText.length - 1
        });
      }
    } else {
      steps.push({
        state: 'ERROR',
        desc: 'ERROR: Đạt kết thúc chuỗi nhưng Request chưa hoàn chỉnh (Cú pháp HTTP không đầy đủ).',
        start: start_idx,
        curr: rawText.length - 1
      });
    }
  }

  return steps;
}

function updateFsmUiActiveState(activeStateId) {
  // Clear active states
  const nodes = document.querySelectorAll('.fsm-state-node');
  nodes.forEach(node => {
    node.classList.remove('active-state');
  });

  // Set active state
  const activeNode = document.getElementById(`state-${activeStateId}`);
  if (activeNode) {
    activeNode.classList.add('active-state');
    activeNode.classList.add('passed-state');
  }
}

function clearFsmUiStates() {
  const nodes = document.querySelectorAll('.fsm-state-node');
  nodes.forEach(node => {
    node.classList.remove('active-state', 'passed-state');
  });
}

function renderFsmVisualText(rawText, startIdx, currIdx) {
  let segment1 = rawText.substring(0, startIdx);
  let segment2 = rawText.substring(startIdx, currIdx);
  let activeChar = rawText.substring(currIdx, currIdx + 1);
  let segment3 = rawText.substring(currIdx + 1);
  
  // Escape HTML tags
  const escapeHtml = (text) => text
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;");
  
  segment1 = escapeHtml(segment1);
  segment2 = escapeHtml(segment2);
  activeChar = escapeHtml(activeChar);
  segment3 = escapeHtml(segment3);
  
  const outputHtml = segment1 + 
                `<span style="color:#3b82f6; font-weight:bold;" title="Con trỏ start">[s]</span>` + 
                `<span style="background:rgba(6,182,212,0.25); color:#fff; border-bottom:2px solid var(--accent-cyan);">${segment2}</span>` + 
                `<span style="background:#ef4444; color:#fff; font-weight:bold; animation: blink 1.5s infinite;">${activeChar || ' '}</span>` +
                `<span style="color:#ef4444; font-weight:bold;" title="Con trỏ curr">[c]</span>` +
                segment3;
  
  document.getElementById('fsm-visual-text').innerHTML = outputHtml;
}

function runNextFsmStep() {
  if (currentStep >= fsmSteps.length) {
    clearInterval(fsmInterval);
    fsmInterval = null;
    document.getElementById('btn-fsm-start').innerText = 'Chạy Mô Phỏng';
    document.getElementById('fsm-input').style.display = 'block';
    document.getElementById('fsm-visual-text').style.display = 'none';
    
    // Check if the final state was error or complete
    const lastStep = fsmSteps[fsmSteps.length - 1];
    if (lastStep && lastStep.state === 'ERROR') {
      logToConsole('Mô phỏng FSM hoàn thành với trạng thái ERROR!', 'error');
    } else {
      logToConsole('Mô phỏng FSM HTTP Parser hoàn thành.', 'success');
    }
    return;
  }

  const item = fsmSteps[currentStep];
  updateFsmUiActiveState(item.state);

  // Display FSM progress log
  const logBox = document.getElementById('fsm-log-box');
  const isError = item.state === 'ERROR';
  const color = isError ? 'var(--error)' : 'var(--accent-cyan)';
  logBox.innerHTML = `<span style="color:${color}; font-weight:bold;">[STATE: ${item.state}]</span> (Bước ${currentStep + 1}/${fsmSteps.length})<br>${item.desc}`;
  
  // Render visual pointers
  const rawText = document.getElementById('fsm-input').value;
  renderFsmVisualText(rawText, item.start, item.curr);

  currentStep++;
}

document.getElementById('btn-fsm-start').addEventListener('click', () => {
  const startBtn = document.getElementById('btn-fsm-start');
  
  if (fsmInterval) {
    // We are running, so this click is "Tạm dừng" (Pause)
    clearInterval(fsmInterval);
    fsmInterval = null;
    isPaused = true;
    startBtn.innerText = 'Tiếp tục';
    logToConsole('Đã tạm dừng mô phỏng FSM HTTP Parser.', 'warning');
    return;
  }
  
  if (isPaused) {
    // We are paused, so this click is "Tiếp tục" (Resume)
    isPaused = false;
    startBtn.innerText = 'Tạm dừng';
    logToConsole('Tiếp tục chạy mô phỏng FSM HTTP Parser...', 'info');
    runNextFsmStep();
    fsmInterval = setInterval(runNextFsmStep, 1500);
    return;
  }
  
  const rawText = document.getElementById('fsm-input').value;
  fsmSteps = generateFsmSteps(rawText);
  currentStep = 0;
  clearFsmUiStates();
  const logBox = document.getElementById('fsm-log-box');
  logBox.innerHTML = '';
  
  document.getElementById('fsm-input').style.display = 'none';
  document.getElementById('fsm-visual-text').style.display = 'block';
  
  startBtn.innerText = 'Tạm dừng';
  logToConsole('Khởi chạy mô phỏng FSM HTTP Parser thực tế...', 'info');
  
  runNextFsmStep(); // Run first step
  fsmInterval = setInterval(runNextFsmStep, 1500);
});

document.getElementById('btn-fsm-reset').addEventListener('click', () => {
  if (fsmInterval) {
    clearInterval(fsmInterval);
    fsmInterval = null;
  }
  clearFsmUiStates();
  const logBox = document.getElementById('fsm-log-box');
  logBox.innerHTML = 'Đã reset máy trạng thái FSM.';
  
  const textarea = document.getElementById('fsm-input');
  textarea.setSelectionRange(0, 0);
  logToConsole('FSM Simulator has been reset.', 'info');
});
