/*!
    \file HttpRequest.cpp
    \brief Implementation of HttpRequest and State pattern classes
    \author HungForre
    \date 6/6/2026
    \copyright VDT
*/

#include "server/http/HttpRequest.h"
#include "server/http/HttpParserState.h"
#include <cctype>
#include <algorithm>
#include <string>

const HttpParserState *HttpRequest::Parse()
{
    while (curr_idx < tail_idx &&
           state != CompleteState::Instance() &&
           state != ErrorState::Instance())
    {
        char c = cache[curr_idx];

        state = state->HandleChar(c, *this);

        curr_idx++;
    }
    return state;
}

void HttpRequest::Reset()
{
    state = StartState::Instance();
    method = HttpMethod::UNKNOWN;
    http_url = std::string_view();
    http_protocol = std::string_view();
    header.clear();
    body = std::string_view();
    content_len = 0;
    tail_idx = 0;
    start_idx = 0;
    curr_idx = 0;
}

void HttpRequest::NextRequest(int new_tail)
{
    state = StartState::Instance();
    method = HttpMethod::UNKNOWN;
    http_url = std::string_view();
    http_protocol = std::string_view();
    header.clear();
    body = std::string_view();
    content_len = 0;

    curr_idx = 0;
    start_idx = 0;

    // Gán tail_idx bằng số byte dư thừa sau khi dịch chuyển
    tail_idx = new_tail;
}

// --- StartState ---
const HttpParserState *StartState::Instance()
{
    static StartState instance;
    return &instance;
}
const HttpParserState *StartState::HandleChar(char c, HttpRequest &req) const
{
    if (c == ' ' || c == '\r' || c == '\n')
    {
        return this;
    }
    req.start_idx = req.curr_idx;
    return MethodState::Instance();
}

// --- MethodState ---
const HttpParserState *MethodState::Instance()
{
    static MethodState instance;
    return &instance;
}
const HttpParserState *MethodState::HandleChar(char c, HttpRequest &req) const
{
    if (c == ' ')
    {
        int len = req.curr_idx - req.start_idx;
        std::string_view m(&req.cache[req.start_idx], len);

        if (m == "GET")
            req.method = HttpMethod::GET;
        else if (m == "POST")
            req.method = HttpMethod::POST;
        else if (m == "PUT")
            req.method = HttpMethod::PUT;
        else if (m == "DELETE")
            req.method = HttpMethod::DELETE;
        else if (m == "HEAD")
            req.method = HttpMethod::HEAD;
        else if (m == "OPTIONS")
            req.method = HttpMethod::OPTIONS;
        else
            req.method = HttpMethod::UNKNOWN;

        req.start_idx = req.curr_idx + 1;
        return UrlState::Instance();
    }
    else if (c == '\r' || c == '\n')
    {
        return ErrorState::Instance();
    }
    return this;
}

// --- UrlState ---
const HttpParserState *UrlState::Instance()
{
    static UrlState instance;
    return &instance;
}
const HttpParserState *UrlState::HandleChar(char c, HttpRequest &req) const
{
    if (c == ' ')
    {
        int len = req.curr_idx - req.start_idx;
        req.http_url = std::string_view(&req.cache[req.start_idx], len);
        req.start_idx = req.curr_idx + 1;

        return ProtocolState::Instance();
    }
    else if (c == '\r' || c == '\n')
    {
        return ErrorState::Instance();
    }
    return this;
}

// --- ProtocolState ---
const HttpParserState *ProtocolState::Instance()
{
    static ProtocolState instance;
    return &instance;
}
const HttpParserState *ProtocolState::HandleChar(char c, HttpRequest &req) const
{
    if (c == '\r')
    {
        int len = req.curr_idx - req.start_idx;
        req.http_protocol = std::string_view(&req.cache[req.start_idx], len);
        return ProtocolCRState::Instance();
    }
    else if (c == '\n')
    {
        int len = req.curr_idx - req.start_idx;
        req.http_protocol = std::string_view(&req.cache[req.start_idx], len);
        req.start_idx = req.curr_idx + 1;
        return StartHeaderState::Instance();
    }
    return this;
}

// --- ProtocolCRState ---
const HttpParserState *ProtocolCRState::Instance()
{
    static ProtocolCRState instance;
    return &instance;
}
const HttpParserState *ProtocolCRState::HandleChar(char c, HttpRequest &req) const
{
    if (c == '\n')
    {
        req.start_idx = req.curr_idx + 1;
        return StartHeaderState::Instance();
    }
    return ErrorState::Instance();
}

// --- StartHeaderState ---
const HttpParserState *StartHeaderState::Instance()
{
    static StartHeaderState instance;
    return &instance;
}
const HttpParserState *StartHeaderState::HandleChar(char c, HttpRequest &req) const
{
    if (c == '\r')
    {
        return StartHeaderCRState::Instance();
    }
    else
    {
        return HeaderKeyState::Instance();
    }
}

// --- StartHeaderCRState ---
const HttpParserState *StartHeaderCRState::Instance()
{
    static StartHeaderCRState instance;
    return &instance;
}
const HttpParserState *StartHeaderCRState::HandleChar(char c, HttpRequest &req) const
{
    if (c == '\n')
    {
        for (const auto &pair : req.header)
        {
            std::string key(pair.first);

            std::transform(key.begin(), key.end(), key.begin(), ::tolower);

            if (key == "content-length")
            {
                try
                {
                    req.content_len = std::stoul(std::string(pair.second));
                }
                catch (...)
                {
                    return ErrorState::Instance();
                }
            }
            else if (key == "content-type")
            {
                req.content_type = pair.second;
            }
        }

        if (req.content_len > 0)
        {
            req.start_idx = req.curr_idx + 1;
            return StartBodyState::Instance();
        }
        else
        {
            return CompleteState::Instance();
        }
    }
    return ErrorState::Instance();
}

// --- HeaderKeyState ---
const HttpParserState *HeaderKeyState::Instance()
{
    static HeaderKeyState instance;
    return &instance;
}
const HttpParserState *HeaderKeyState::HandleChar(char c, HttpRequest &req) const
{
    if (c == ':')
    {
        int len = req.curr_idx - req.start_idx;
        req.current_header_key = std::string_view(&req.cache[req.start_idx], len);
        req.start_idx = req.curr_idx + 1;
        return HeaderValueState::Instance();
    }
    else if (c == '\r' || c == '\n')
    {
        return ErrorState::Instance();
    }
    return this;
}

// --- HeaderValueState ---
const HttpParserState *HeaderValueState::Instance()
{
    static HeaderValueState instance;
    return &instance;
}
const HttpParserState *HeaderValueState::HandleChar(char c, HttpRequest &req) const
{
    if (c == '\r')
    {
        int len = req.curr_idx - req.start_idx;
        req.header[req.current_header_key] = std::string_view(&req.cache[req.start_idx], len);
        return HeaderValueCRState::Instance();
    }
    else if (c == '\n')
    {
        int len = req.curr_idx - req.start_idx;
        req.header[req.current_header_key] = std::string_view(&req.cache[req.start_idx], len);
        req.start_idx = req.curr_idx + 1;
        return StartHeaderState::Instance();
    }
    return this;
}

// --- HeaderValueCRState ---
const HttpParserState *HeaderValueCRState::Instance()
{
    static HeaderValueCRState instance;
    return &instance;
}
const HttpParserState *HeaderValueCRState::HandleChar(char c, HttpRequest &req) const
{
    if (c == '\n')
    {
        req.start_idx = req.curr_idx + 1;
        return StartHeaderState::Instance();
    }
    return ErrorState::Instance();
}

// --- StartBodyState ---
const HttpParserState *StartBodyState::Instance()
{
    static StartBodyState instance;
    return &instance;
}
const HttpParserState *StartBodyState::HandleChar(char c, HttpRequest &req) const
{
    // Tính độ dài body đã đọc được (bao gồm cả ký tự c hiện tại)
    int body_len = req.curr_idx - req.start_idx + 1;

    if (body_len >= (int)req.content_len)
    {
        req.body = std::string_view(&req.cache[req.start_idx], req.content_len);
        return CompleteState::Instance();
    }
    return this;
}

// --- CompleteState ---

const HttpParserState *CompleteState::Instance()
{
    static CompleteState instance;
    return &instance;
}

const HttpParserState *CompleteState::HandleChar(char c, HttpRequest &req) const
{
    return this;
}

// --- ErrorState ---
const HttpParserState *ErrorState::Instance()
{
    static ErrorState instance;
    return &instance;
}
const HttpParserState *ErrorState::HandleChar(char c, HttpRequest &req) const
{
    return this;
}