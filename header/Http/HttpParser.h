#pragma once
#include "CppConfig.h"

enum class ParseState
{
    READING_HEADER,
    READING_PAYLOAD,
    COMPLETE,
    ERROR
};

enum class HttpMethod
{
    GET,
    POST,
    PUT,
    DELETE,
    HEAD,
    OPTIONS,
    UNKNOWN
};

struct HttpRequest
{
    HttpMethod method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    ParseState state;

    HttpRequest() : method(HttpMethod::UNKNOWN), state(ParseState::READING_HEADER) {}
};

class HttpParser
{
private:
    HttpRequest request;
    size_t header_end_pos{0};

    HttpMethod ParseMethod(const std::string &method_str);
    bool ParseRequestLine(const std::string &line);
    bool ParseHeaderLine(const std::string &line);

public:
    HttpParser();
    ParseState Parse(const char *buffer, size_t len);
    const HttpRequest &GetRequest() const { return request; }
    void Reset();
};
