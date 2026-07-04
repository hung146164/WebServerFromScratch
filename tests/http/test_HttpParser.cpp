#include <iostream>
#include <fstream>
#include <cassert>
#include <string>
#include <cstring>
#include "server/http/HttpRequest.h"
#include "server/http/HttpParserState.h"

// Define to satisfy linker for Router.cpp
thread_local int current_worker_id = -1;

// 1. Test Request Line Parsing
void TestRequestLineParsing() {
    std::cout << "[*] Running TestRequestLineParsing...\n";
    
    HttpRequest req(1024);
    std::string raw_request = "GET /api/view/index.html HTTP/1.1\r\n\r\n";
    std::memcpy(req.cache.data(), raw_request.data(), raw_request.size());
    req.tail_idx = raw_request.size();
    
    const HttpParserState *state = req.Parse();
    assert(state == CompleteState::Instance());
    assert(req.method == HttpMethod::GET);
    assert(req.http_url == "/api/view/index.html");
    assert(req.http_protocol == "HTTP/1.1");
    
    std::cout << "[+] TestRequestLineParsing PASSED!\n";
}

// 2. Test Header Parsing and Space Trimming
void TestHeaderParsing() {
    std::cout << "[*] Running TestHeaderParsing...\n";
    
    HttpRequest req(1024);
    // Note the leading spaces in header values to test trimming
    std::string raw_request = "POST /api/upload HTTP/1.1\r\nHost: localhost\r\nConnection:  keep-alive\r\nContent-Length: 10\r\n\r\n1234567890";
    std::memcpy(req.cache.data(), raw_request.data(), raw_request.size());
    req.tail_idx = raw_request.size();
    
    const HttpParserState *state = req.Parse();
    assert(state == CompleteState::Instance());
    assert(req.method == HttpMethod::POST);
    assert(req.http_url == "/api/upload");
    assert(req.content_len == 10);
    assert(req.body == "1234567890");
    
    // Check header parsing (keys are std::string_view)
    bool found_host = false;
    bool found_connection = false;
    for (const auto &pair : req.header) {
        if (pair.first == "Host") {
            assert(pair.second == "localhost");
            found_host = true;
        }
        if (pair.first == "Connection") {
            // The leading space must be trimmed
            assert(pair.second == "keep-alive");
            found_connection = true;
        }
    }
    assert(found_host);
    assert(found_connection);
    
    std::cout << "[+] TestHeaderParsing PASSED!\n";
}

// 3. Test TCP Stream Compaction (HTTP Pipelining)
void TestTcpStreamCompaction() {
    std::cout << "[*] Running TestTcpStreamCompaction...\n";
    
    HttpRequest req(1024);
    // Request 1 is followed immediately by the start of Request 2 in the same buffer packet
    std::string raw_request = "GET /first HTTP/1.1\r\nHost: localhost\r\n\r\nGET /second HTTP/1.1\r\n";
    std::memcpy(req.cache.data(), raw_request.data(), raw_request.size());
    req.tail_idx = raw_request.size();
    
    // 1. Parse first request
    const HttpParserState *state1 = req.Parse();
    assert(state1 == CompleteState::Instance());
    assert(req.http_url == "/first");
    
    // 2. Perform compaction (like in Worker::HandleRequest)
    int remain = req.tail_idx - req.curr_idx;
    assert(remain > 0); // "GET /second HTTP/1.1\r\n" should be remaining
    std::memmove(&req.cache[0], &req.cache[req.curr_idx], remain);
    req.NextRequest(remain);
    
    // 3. Parse second request
    const HttpParserState *state2 = req.Parse();
    // It shouldn't be complete yet because we are missing "\r\n\r\n" at the end of Request 2
    assert(state2 != CompleteState::Instance());
    
    // 4. Feed the rest of Request 2 and parse
    std::string raw_rest = "Host: localhost\r\n\r\n";
    std::memcpy(&req.cache[req.tail_idx], raw_rest.data(), raw_rest.size());
    req.tail_idx += raw_rest.size();
    
    const HttpParserState *state3 = req.Parse();
    assert(state3 == CompleteState::Instance());
    assert(req.http_url == "/second");
    
    std::cout << "[+] TestTcpStreamCompaction PASSED!\n";
}

// 4. Test Heavy HTTP Request Parsing (Load & Edge Case Validation)
void TestHeavyRequestParsing() {
    std::cout << "[*] Running TestHeavyRequestParsing...\n";
    
    std::string path_prefix = "";
    std::string request_file = "heavy_http_request.raw";
    std::string metadata_file = "heavy_http_metadata.txt";
    
    // 1. Kiem tra thu muc hien tai (khi chay truc tiep tu build/tests/http/)
    std::ifstream meta(metadata_file);
    if (!meta.is_open()) {
        // 2. Kiem tra tu thu muc root cua du an
        path_prefix = "tests/http/";
        request_file = path_prefix + "heavy_http_request.raw";
        metadata_file = path_prefix + "heavy_http_metadata.txt";
        meta.open(metadata_file);
    }
    if (!meta.is_open()) {
        // 3. Kiem tra neu chay tu root cua du an nhung doc trong build/
        path_prefix = "build/tests/http/";
        request_file = path_prefix + "heavy_http_request.raw";
        metadata_file = path_prefix + "heavy_http_metadata.txt";
        meta.open(metadata_file);
    }
    if (!meta.is_open()) {
        // 4. Fallback ve thu muc source neu build/ thieu file
        path_prefix = "../tests/http/";
        request_file = path_prefix + "heavy_http_request.raw";
        metadata_file = path_prefix + "heavy_http_metadata.txt";
        meta.open(metadata_file);
    }
    
    assert(meta.is_open());
    std::string line;
    std::string expected_method, expected_url, expected_protocol;
    long long expected_length = 0, expected_checksum = 0;
    
    while (std::getline(meta, line)) {
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);
        if (key == "method") expected_method = val;
        else if (key == "url") expected_url = val;
        else if (key == "protocol") expected_protocol = val;
        else if (key == "body_length") expected_length = std::stoll(val);
        else if (key == "checksum") expected_checksum = std::stoll(val);
    }
    
    // Read raw request file
    std::ifstream file(request_file, std::ios::binary | std::ios::ate);
    assert(file.is_open());
    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Instantiate HttpRequest with big capacity
    HttpRequest req(file_size + 4096);
    assert(file.read(req.cache.data(), file_size));
    req.tail_idx = file_size;
    
    const HttpParserState *state = req.Parse();
    assert(state == CompleteState::Instance());
    
    assert(req.method == HttpMethod::POST);
    assert(req.http_url == expected_url);
    assert(req.http_protocol == expected_protocol);
    assert(req.content_len == expected_length);
    assert(req.body.size() == expected_length);
    
    // Verify some specific modern/extreme headers
    bool found_user_agent = false;
    bool found_custom_header = false;
    bool found_empty_header = false;
    bool found_spaces_header = false;
    
    for (const auto &pair : req.header) {
        if (pair.first == "User-Agent") {
            assert(pair.second.find("Mozilla/5.0") != std::string_view::npos);
            found_user_agent = true;
        }
        if (pair.first == "X-Custom-Header-Very-Long") {
            assert(pair.second.size() == 1000);
            found_custom_header = true;
        }
        if (pair.first == "X-Empty-Header") {
            assert(pair.second.empty());
            found_empty_header = true;
        }
        if (pair.first == "X-Spaces-Header") {
            // Note: Parser only trims leading spaces, preserving trailing spaces
            assert(pair.second == "trimmed-value-with-spaces     ");
            found_spaces_header = true;
        }
    }
    
    assert(found_user_agent);
    assert(found_custom_header);
    assert(found_empty_header);
    assert(found_spaces_header);
    
    // Calculate simple sum checksum of parsed body
    long long calculated_checksum = 0;
    for (char c : req.body) {
        calculated_checksum = (calculated_checksum + (unsigned char)c) % 1000000007;
    }
    
    assert(calculated_checksum == expected_checksum);
    
    std::cout << "[+] TestHeavyRequestParsing PASSED! Checksum verified: " << calculated_checksum << "\n";
}

int main() {
    std::cout << "=========================================================\n";
    std::cout << "      RUNNING HTTP PARSER UNIT TESTS                     \n";
    std::cout << "=========================================================\n";
    
    TestRequestLineParsing();
    TestHeaderParsing();
    TestTcpStreamCompaction();
    TestHeavyRequestParsing();
    
    std::cout << "=========================================================\n";
    std::cout << "      ALL HTTP PARSER UNIT TESTS PASSED!                 \n";
    std::cout << "=========================================================\n";
    return 0;
}
