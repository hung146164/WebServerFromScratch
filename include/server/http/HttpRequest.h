/*!
    \file HttpRequest.h
    \brief HttpRequest Definition
    \author HungForre
    \date 6/6/2026
    \copyright VDT
*/
#ifndef CPPSERVER_HTTP_HTTPREQUEST_H
#define CPPSERVER_HTTP_HTTPREQUEST_H

#include <string_view>
#include <string>
#include <unordered_map>
#include <vector>

#include "HttpMethod.h"
#include "HttpParserState.h"

struct HttpRequest
{
    HttpMethod method;
    const HttpParserState *state;

    std::string_view http_url;
    std::string_view http_protocol;
    std::unordered_map<std::string_view, std::string_view> header;

    int content_len = 0;
    std::string_view content_type;
    std::string_view body;

    int head_idx = 0;
    int tail_idx = 0;  // Con trỏ ghi
    int start_idx = 0; // con trỏ đọc
    int curr_idx = 0;  // con trỏ duyệt

    std::string_view current_header_key;

    std::vector<char> cache;
    std::string client_ip;

    HttpRequest(int bufferSize = 65536);

    // Chạy vòng lặp phân tích cú pháp
    const HttpParserState *Parse();

    // State for asynchronous file sending (EPOLLOUT)
    bool is_sending_file = false;
    int file_fd = -1;
    off_t file_offset = 0;
    off_t file_remaining = 0;

    // Minimum data rate speed monitoring
    time_t last_speed_check_time = 0;
    off_t bytes_sent_in_period = 0;

    void Reset();
    void NextRequest(int new_tail);
};
#endif