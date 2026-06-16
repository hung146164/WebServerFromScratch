# WebServerVdt: High-Performance C++ Web Server

WebServerVdt is a lightweight, high-performance HTTP Web Server written in C++17 from scratch. It uses a **Multi-Reactor event-driven architecture** combining `SO_REUSEPORT` with `epoll` loops across a pool of worker threads, achieving exceptional concurrency.

---

## 🚀 Key Features

* **Multi-Reactor Concurrency**: Avoids the thread-per-connection overhead by spawning a fixed pool of Worker threads, each running its own non-blocking Linux `epoll` event loop. It uses `SO_REUSEPORT` at the kernel level for load balancing.
* **FSM HTTP Parser**: An efficient Finite State Machine (FSM) parser using the State Pattern to read HTTP request headers, cookies, and bodies incrementally.
* **Zero-Copy Static File Serving**: Serves HTML, CSS, JavaScript, and media files directly from disk using the Linux kernel-space `sendfile` API for maximum speed and zero CPU memory copies.
* **Range Requests (Partial Content)**: Supports pause/resume and multi-part downloads (such as IDM acceleration) by handling the HTTP `Range` and `206 Partial Content` protocol.
* **Multipart File Upload (POST)**: Includes a built-in `multipart/form-data` and raw binary parser to let clients upload files directly onto the server.
* **Wildcard & Prefix Routing**: Supports prefix path matching (e.g. `/api/download_image/*`) to easily bundle static assets or file downloads.
* **Stable Node-Pool JSON Parser**: Features a custom high-performance, zero-copy JSON parser using a pre-allocated memory pool.
* **Graceful Shutdown**: Capture signals like `SIGINT` (Ctrl+C) and `SIGTERM` to safely close worker loops, release sockets, and join all threads with zero resource leaks.

---

## 📁 Project Structure

```text
WebServerVdt/
├── CMakeLists.txt         # Root CMake build configuration
├── Dockerfile             # Multi-stage production Docker build
├── include/               # Header files
│   ├── common/            # Custom LRU cache, Socket guards, memory pools
│   ├── network/           # Server configuration, Epoll workers, main listener
│   └── server/http/       # HTTP parser, Response helpers, Routers
├── src/                   # Implementation source files
│   ├── main.cpp           # Main entry point (Route registration and startup)
│   ├── network/           # Server and Epoll Worker logic
│   └── server/http/       # HTTP FSM parser and Routing logic
├── tests/                 # Unit tests (JSON parser benchmarks)
└── www/                   # Web Root (Static files, downloaded assets, uploads)
    └── uploads/           # Target folder for uploaded files
```

---

## 🛠️ How to Build and Run

### Prerequisites (Linux/WSL)
* GCC or Clang supporting C++17
* CMake 3.10+
* Make

### Local Build
```bash
# 1. Clone the project and navigate to the directory
cd WebServerVdt

# 2. Create and enter build directory
mkdir -p build && cd build

# 3. Configure and compile
cmake ..
make

# 4. Start the server (runs on port 8081 by default)
./webserver
```

### Build & Run with Docker
```bash
# Build the container
docker build -t cpp-webserver .

# Run the container
docker run -p 8081:8080 cpp-webserver
```

---

## 📖 Developer Guide & API Usage

### 1. Initializing and Starting the Server
Define configurations like ports and worker threads via `ServerConfig` and register signals for a clean shutdown:

```cpp
#include "network/NetworkServer.h"
#include "network/ServerConfig.h"
#include <csignal>

NetworkServer* global_server = nullptr;

void signal_handler(int signal) {
    if (global_server) {
        global_server->Stop();
    }
}

int main() {
    ServerConfig cfg;
    cfg.port = 8081;
    cfg.num_workers = 2; // Fixed number of worker threads

    NetworkServer server(cfg);
    global_server = &server;

    // Register signal handlers for graceful shutdown and hot config reload
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGHUP, signal_handler); // Reload config.json on the fly

    server.Start(); // Blocks until shutdown signal received
    return 0;
}
```

### 2. Route Registration (Wildcards & POST)
Register GET, POST, and fallback handlers inside `main.cpp` before starting the server.

```cpp
#include "server/http/Router.h"
#include "server/http/HttpUtils.h"

// 1. Wildcard matching for file downloads
// Matches GET /api/download_image/photo.jpg or GET /api/download_image/doc.pdf
Http::Router::Register(HttpMethod::GET, "/api/download_image/*", [](int fd, const HttpRequest& req) {
    std::string_view req_range = "";
    auto it = req.header.find("Range");
    if (it != req.header.end()) {
        req_range = it->second;
    }
    // Serve from the "www" directory
    Http::ServeFile(fd, req, "www", req_range);
});

// 2. Handle POST file uploads
// Automatically parses multipart/form-data and saves files in 'www/uploads/'
Http::Router::Register(HttpMethod::POST, "/api/upload", Http::HandleFileUpload);

// 3. Handle Application-level Encrypted POST Data (RC4 Cipher)
Http::Router::Register(HttpMethod::POST, "/api/secure_data", [](int fd, const HttpRequest& req) {
    std::string body_data(req.body);
    
    // Decrypt the client request payload using RC4
    Http::RC4("VDTSecretKey", body_data);
    std::cout << "[Secure API] Decrypted body: " << body_data << "\n";

    // Prepare response JSON
    std::string resp = "{\"status\":\"ok\",\"secret_received\":\"" + body_data + "\"}";
    
    // Encrypt the response payload before sending
    Http::RC4("VDTSecretKey", resp);

    // Send encrypted response with custom header X-Encrypt: true
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: application/octet-stream\r\n"
        << "Content-Length: " << resp.size() << "\r\n"
        << "X-Encrypt: true\r\n"
        << "Connection: keep-alive\r\n\r\n"
        << resp;
    std::string response = oss.str();
    send(fd, response.data(), response.size(), MSG_NOSIGNAL);
});

// 4. Fallback Route: serves static HTML, CSS, JS automatically
Http::Router::RegisterFallback([](int fd, const HttpRequest& req) {
    Http::ServeFile(fd, req, "www");
});
```

### 3. Sending Responses (JSON, HTML, Text, Redirects)
Utility methods are provided inside `HttpResponse.h` to easily reply to clients:

```cpp
#include "server/http/HttpResponse.h"

// Send JSON response
Http::JSON(fd, 200, "{\"status\":\"success\"}");

// Send HTML webpage
Http::HTML(fd, 200, "<h1>Welcome to WebServerVdt</h1>");

// Send plain text
Http::Text(fd, 200, "Hello World");

// Redirect
Http::Redirect(fd, "/index.html");

// Error JSON Helper (404, 400, 500)
Http::Error(fd, 404, "Page Not Found");
```

---

## 🔬 Systems Programming Architecture Details

### Concurrency Model (Multi-Reactor)
Unlike the traditional **Thread-per-Connection** model (which creates a new OS thread for every request, wasting memory and causing CPU thrashing due to context switching), `WebServerVdt` uses a **Multi-Reactor** model.
1. The Linux Kernel handles connection load-balancing across workers via `SO_REUSEPORT`.
2. Each Worker thread runs a single event loop around `epoll_wait` in non-blocking mode.
3. When a connection is ready, it is accepted via `accept4` (with `SOCK_NONBLOCK` and `SOCK_CLOEXEC` flags).
4. HTTP parser cache buffers are managed inside an **LRU Cache (`LRUCustom`)** mapping file descriptors to requests, avoiding dynamic allocations for active sockets.
