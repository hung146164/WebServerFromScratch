#include <gtest/gtest.h>
#include "server/http/HttpRequest.h"
#include "server/http/HttpParserState.h"

// Test parse một request GET cơ bản (không có body)
TEST(HttpParserTest, ParseGetRequestWithoutBody)
{
    HttpRequest req;
    
    // Dữ liệu HTTP Request thô giả lập nhận từ socket
    std::string raw_request = 
        "GET /api/students HTTP/1.1\r\n"
        "Host: localhost:8081\r\n"
        "User-Agent: UnitTester\r\n"
        "Accept: */*\r\n"
        "\r\n";
        
    // Copy vào buffer cache của request
    ASSERT_LE(raw_request.size(), req.cache.size());
    std::copy(raw_request.begin(), raw_request.end(), req.cache.begin());
    req.tail_idx = (int)raw_request.size();
    
    // Gọi hàm phân tích cú pháp
    req.Parse();
    
    // Kiểm tra Method, URL và Protocol
    EXPECT_EQ(req.method, HttpMethod::GET);
    EXPECT_EQ(req.http_url, "/api/students");
    EXPECT_EQ(req.http_protocol, "HTTP/1.1");
    
    // Kiểm tra các Headers được lưu vào map
    EXPECT_EQ(req.header["Host"], "localhost:8081");
    EXPECT_EQ(req.header["User-Agent"], "UnitTester");
    EXPECT_EQ(req.header["Accept"], "*/*");
}

// Test parse request POST có kèm body JSON và Content-Length
TEST(HttpParserTest, ParsePostRequestWithBody)
{
    HttpRequest req;
    
    std::string body_content = "{\"name\":\"Nam\",\"class\":\"Math\",\"score\":10.0}";
    std::string raw_request = 
        "POST /api/students HTTP/1.1\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(body_content.size()) + "\r\n"
        "\r\n" + body_content;
        
    ASSERT_LE(raw_request.size(), req.cache.size());
    std::copy(raw_request.begin(), raw_request.end(), req.cache.begin());
    req.tail_idx = (int)raw_request.size();
    
    req.Parse();
    
    // Kiểm tra thông tin parse
    EXPECT_EQ(req.method, HttpMethod::POST);
    EXPECT_EQ(req.http_url, "/api/students");
    EXPECT_EQ(req.content_type, "application/json");
    EXPECT_EQ(req.content_len, (int)body_content.size());
    EXPECT_EQ(req.body, body_content);
}
