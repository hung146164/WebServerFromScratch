#pragma once
#include "CppConfig.h"

struct HttpRequest
{
    std::vector<char> cache;

    // Request Line
    size_t method_index = 0, method_size = 0;
    size_t url_index = 0, url_size = 0;
    size_t protocol_index = 0, protocol_size = 0;

    // Headers: lưu dưới dạng index để tối ưu
    // key_idx, key_sz, val_idx, val_sz
    std::vector<std::tuple<size_t, size_t, size_t, size_t>> headers;

    // Body
    size_t body_index = 0;
    size_t body_size = 0;   // Size hiện tại đã nhận
    size_t body_length = 0; // Tổng size cần nhận (lấy từ Content-Length header)

    void Reset()
    {
        cache.clear();
        headers.clear();
        method_size = url_size = protocol_size = body_size = body_length = 0;
    }
};