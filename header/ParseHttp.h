#pragma once

#include <string>
#include <map>
#include <sstream>
#include <algorithm>
#include <cstring>

struct HttpRequest
{
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
};

inline std::string trim(const std::string &value)
{
    const size_t start = value.find_first_not_of(" \t\r\n");
    const size_t end = value.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? std::string() : value.substr(start, end - start + 1);
}

inline std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
                   { return std::tolower(c); });
    return value;
}

inline bool parseHttpRequest(const std::string &buffer, HttpRequest &request, size_t &requestLength)
{
    const size_t headerEnd = buffer.find("\r\n\r\n");
    if (headerEnd == std::string::npos)
    {
        return false;
    }

    const size_t requestLineEnd = buffer.find("\r\n");
    if (requestLineEnd == std::string::npos)
    {
        return false;
    }

    std::istringstream requestLine(buffer.substr(0, requestLineEnd));
    if (!(requestLine >> request.method >> request.path >> request.version))
    {
        return false;
    }

    size_t position = requestLineEnd + 2;
    while (position < headerEnd)
    {
        const size_t lineEnd = buffer.find("\r\n", position);
        if (lineEnd == std::string::npos)
        {
            break;
        }

        const std::string headerLine = buffer.substr(position, lineEnd - position);
        const size_t colon = headerLine.find(':');
        if (colon != std::string::npos)
        {
            const std::string name = toLower(trim(headerLine.substr(0, colon)));
            const std::string value = trim(headerLine.substr(colon + 1));
            request.headers[name] = value;
        }
        position = lineEnd + 2;
    }

    size_t contentLength = 0;
    const auto it = request.headers.find("content-length");
    if (it != request.headers.end())
    {
        try
        {
            contentLength = std::stoul(it->second);
        }
        catch (...)
        {
        }
    }

    const size_t totalLength = headerEnd + 4 + contentLength;
    if (buffer.size() < totalLength)
    {
        return false;
    }

    request.body = buffer.substr(headerEnd + 4, contentLength);
    requestLength = totalLength;
    return true;
}

inline bool isHttpRequestComplete(const std::string &buffer)
{
    size_t requestLength;
    HttpRequest request;
    return parseHttpRequest(buffer, request, requestLength);
}
