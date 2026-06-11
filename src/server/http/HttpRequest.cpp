/*!
    \file HttpRequest.cpp
    \brief FSM HTTP parser
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
    http_url = {};
    http_protocol = {};
    header.clear();
    body = {};
    content_len = 0;
    tail_idx = 0;
    start_idx = 0;
    curr_idx = 0;
}

void HttpRequest::NextRequest(int new_tail)
{
    state = StartState::Instance();
    method = HttpMethod::UNKNOWN;
    http_url = {};
    http_protocol = {};
    header.clear();
    body = {};
    content_len = 0;
    curr_idx = 0;
    start_idx = 0;
    tail_idx = new_tail;
}

static inline int span(int from, int to) { return (int)(to - from); }

// --- StartState ---
const HttpParserState *StartState::Instance()
{
    static StartState i;
    return &i;
}

const HttpParserState *StartState::HandleChar(char c, HttpRequest &req) const
{
    if (c == ' ' || c == '\r' || c == '\n')
        return this;
    req.start_idx = req.curr_idx;
    return MethodState::Instance();
}

// --- MethodState ---
const HttpParserState *MethodState::Instance()
{
    static MethodState i;
    return &i;
}

const HttpParserState *MethodState::HandleChar(char c, HttpRequest &req) const
{
    if (c == ' ')
    {
        std::string_view m(&req.cache[req.start_idx], span(req.start_idx, req.curr_idx));
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
    if (c == '\r' || c == '\n')
        return ErrorState::Instance();
    return this;
}

// --- UrlState ---
const HttpParserState *UrlState::Instance()
{
    static UrlState i;
    return &i;
}

const HttpParserState *UrlState::HandleChar(char c, HttpRequest &req) const
{
    if (c == ' ')
    {
        req.http_url = std::string_view(&req.cache[req.start_idx], span(req.start_idx, req.curr_idx));
        req.start_idx = req.curr_idx + 1;
        return ProtocolState::Instance();
    }
    if (c == '\r' || c == '\n')
        return ErrorState::Instance();
    return this;
}

// --- ProtocolState ---
const HttpParserState *ProtocolState::Instance()
{
    static ProtocolState i;
    return &i;
}

const HttpParserState *ProtocolState::HandleChar(char c, HttpRequest &req) const
{
    if (c == '\r')
    {
        req.http_protocol = std::string_view(&req.cache[req.start_idx], span(req.start_idx, req.curr_idx));
        return ProtocolCRState::Instance();
    }
    if (c == '\n')
    {
        req.http_protocol = std::string_view(&req.cache[req.start_idx], span(req.start_idx, req.curr_idx));
        req.start_idx = req.curr_idx + 1;
        return StartHeaderState::Instance();
    }
    return this;
}

// --- ProtocolCRState ---
const HttpParserState *ProtocolCRState::Instance()
{
    static ProtocolCRState i;
    return &i;
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
    static StartHeaderState i;
    return &i;
}

const HttpParserState *StartHeaderState::HandleChar(char c, HttpRequest &req) const
{
    if (c == '\r')
        return StartHeaderCRState::Instance();
    return HeaderKeyState::Instance();
}

// --- StartHeaderCRState ---
const HttpParserState *StartHeaderCRState::Instance()
{
    static StartHeaderCRState i;
    return &i;
}

const HttpParserState *StartHeaderCRState::HandleChar(char c, HttpRequest &req) const
{
    if (c != '\n')
        return ErrorState::Instance();

        for (const auto &pair : req.header)
    {
        std::string_view k = pair.first;
        auto iequal = [](std::string_view a, std::string_view b)
        {
            if (a.size() != b.size())
                return false;
            for (size_t i = 0; i < a.size(); ++i)
                if (std::tolower((unsigned char)a[i]) != (unsigned char)b[i])
                    return false;
            return true;
        };
        if (iequal(k, "content-length"))
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
        else if (iequal(k, "content-type"))
        {
            req.content_type = pair.second;
        }
    }

    if (req.content_len > 0)
    {
        req.start_idx = req.curr_idx + 1;
        return StartBodyState::Instance();
    }
    return CompleteState::Instance();
}

// --- HeaderKeyState ---
const HttpParserState *HeaderKeyState::Instance()
{
    static HeaderKeyState i;
    return &i;
}

const HttpParserState *HeaderKeyState::HandleChar(char c, HttpRequest &req) const
{
    if (c == ':')
    {
        req.current_header_key = std::string_view(&req.cache[req.start_idx], span(req.start_idx, req.curr_idx));
        req.start_idx = req.curr_idx + 1;
        return HeaderValueState::Instance();
    }
    if (c == '\r' || c == '\n')
        return ErrorState::Instance();
    return this;
}

// --- HeaderValueState ---
const HttpParserState *HeaderValueState::Instance()
{
    static HeaderValueState i;
    return &i;
}

const HttpParserState *HeaderValueState::HandleChar(char c, HttpRequest &req) const
{
    if (req.curr_idx == req.start_idx && c == ' ')
    {
        req.start_idx++;
        return this;
    }

    if (c == '\r')
    {
        req.header[req.current_header_key] = std::string_view(&req.cache[req.start_idx], span(req.start_idx, req.curr_idx));
        return HeaderValueCRState::Instance();
    }
    if (c == '\n')
    {
        req.header[req.current_header_key] = std::string_view(&req.cache[req.start_idx], span(req.start_idx, req.curr_idx));
        req.start_idx = req.curr_idx + 1;
        return StartHeaderState::Instance();
    }
    return this;
}

// --- HeaderValueCRState ---
const HttpParserState *HeaderValueCRState::Instance()
{
    static HeaderValueCRState i;
    return &i;
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
    static StartBodyState i;
    return &i;
}

const HttpParserState *StartBodyState::HandleChar(char c, HttpRequest &req) const
{
    (void)c;
    int body_len = req.curr_idx - req.start_idx + 1;

    if (body_len >= req.content_len)
    {
        req.body = std::string_view(&req.cache[req.start_idx], req.content_len);
        return CompleteState::Instance();
    }
    return this;
}

// --- CompleteState ---
const HttpParserState *CompleteState::Instance()
{
    static CompleteState i;
    return &i;
}
const HttpParserState *CompleteState::HandleChar(char, HttpRequest &) const { return this; }

// --- ErrorState ---
const HttpParserState *ErrorState::Instance()
{
    static ErrorState i;
    return &i;
}
const HttpParserState *ErrorState::HandleChar(char, HttpRequest &) const { return this; }
