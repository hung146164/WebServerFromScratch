/*!
    \file HttpParserState.h
    \brief HTTP parser state classes using State Pattern
    \author HungForre
    \date 6/6/2026
    \copyright VDT
*/

#ifndef CPPSERVER_HTTP_HTTPPARSESTATE_H
#define CPPSERVER_HTTP_HTTPPARSESTATE_H

/*
 * 1. StartState
 * 2. MethodState
 * 3. UrlState
 * 4. ProtocolState
 * 5. ProtocolCRState
 * 6. StartHeaderState
 * 7. StartHeaderCRState
 * 8. HeaderKeyState
 * 9. HeaderValueState
 * 10. HeaderValueCRState
 * 11. StartBodyState
 * 12. CompleteState
 * 13. ErrorState
 */

/*
FSM
*/
struct HttpRequest;

class HttpParserState
{
public:
    virtual ~HttpParserState() = default;

    virtual const HttpParserState *HandleChar(char c, HttpRequest &req) const = 0;
};

// 1. START_PARSE: Tìm ký tự đầu tiên của Method
class StartState : public HttpParserState
{
public:
    static const HttpParserState *Instance();
    const HttpParserState *HandleChar(char c, HttpRequest &req) const override;
};

// 2. METHOD: Đọc Method, gặp ' ' -> UrlState
class MethodState : public HttpParserState
{
public:
    static const HttpParserState *Instance();
    const HttpParserState *HandleChar(char c, HttpRequest &req) const override;
};

// 3. URL: Đọc URL, gặp ' ' -> ProtocolState
class UrlState : public HttpParserState
{
public:
    static const HttpParserState *Instance();
    const HttpParserState *HandleChar(char c, HttpRequest &req) const override;
};

// 4. PROTOCOL: Đọc giao thức (HTTP/1.1), gặp '\r' -> ProtocolCRState
class ProtocolState : public HttpParserState
{
public:
    static const HttpParserState *Instance();
    const HttpParserState *HandleChar(char c, HttpRequest &req) const override;
};

// 5. PROTOCOL_CR: Chờ '\n' -> StartHeaderState
class ProtocolCRState : public HttpParserState
{
public:
    static const HttpParserState *Instance();
    const HttpParserState *HandleChar(char c, HttpRequest &req) const override;
};

// 6. START_HEADER: Gặp '\r' -> StartHeaderCRState (kết thúc header), gặp chữ -> HeaderKeyState
class StartHeaderState : public HttpParserState
{
public:
    static const HttpParserState *Instance();
    const HttpParserState *HandleChar(char c, HttpRequest &req) const override;
};

// 7. START_HEADER_CR: Chờ '\n' -> StartBodyState (Xác nhận kết thúc toàn bộ header \r\n\r\n)
class StartHeaderCRState : public HttpParserState
{
public:
    static const HttpParserState *Instance();
    const HttpParserState *HandleChar(char c, HttpRequest &req) const override;
};

// 8. HEADER_KEY: Đọc Key của Header, gặp ':' -> HeaderValueState
class HeaderKeyState : public HttpParserState
{
public:
    static const HttpParserState *Instance();
    const HttpParserState *HandleChar(char c, HttpRequest &req) const override;
};

// 9. HEADER_VALUE: Đọc Value của Header, gặp '\r' -> HeaderValueCRState
class HeaderValueState : public HttpParserState
{
public:
    static const HttpParserState *Instance();
    const HttpParserState *HandleChar(char c, HttpRequest &req) const override;
};

// 10. HEADER_VALUE_CR: Chờ '\n' -> Quay lại StartHeaderState (hoàn tất 1 dòng header \r\n)
class HeaderValueCRState : public HttpParserState
{
public:
    static const HttpParserState *Instance();
    const HttpParserState *HandleChar(char c, HttpRequest &req) const override;
};

// 11. START_BODY: Đọc body dựa vào Content-Length -> CompleteState
class StartBodyState : public HttpParserState
{
public:
    static const HttpParserState *Instance();
    const HttpParserState *HandleChar(char c, HttpRequest &req) const override;
};

// 12. COMPLETE: Đã parse hoàn tất thành công
class CompleteState : public HttpParserState
{
public:
    static const HttpParserState *Instance();
    const HttpParserState *HandleChar(char c, HttpRequest &req) const override;
};

// 13. ERROR: Lỗi cú pháp HTTP
class ErrorState : public HttpParserState
{
public:
    static const HttpParserState *Instance();
    const HttpParserState *HandleChar(char c, HttpRequest &req) const override;
};

#endif // CPPSERVER_HTTP_HTTPPARSESTATE_H