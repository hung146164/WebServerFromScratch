/*!
    \file Router.h
    \brief Router definition with fallback handler support
    \author HungForre
    \date 7/6/2026
    \copyright VDT
*/

#ifndef CPPSERVER_HTTP_ROUTER_H
#define CPPSERVER_HTTP_ROUTER_H

#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>

#include "server/http/HttpRequest.h"
#include "server/http/HttpMethod.h"

using HandlerFunc = std::function<void(int fd, const HttpRequest &req)>;

namespace Http
{

    class Router
    {
    public:
        /// Dang ky route cu the
        /// Http::Router::Register(HttpMethod::GET, "/api/students", Handler);
        static void Register(HttpMethod method, const std::string &path, HandlerFunc handler);

        /// Dang ky fallback handler - duoc goi khi khong tim thay route nao
        /// Dung de serve static files cho moi URL khong match
        /// Http::Router::RegisterFallback(HandleStatic);
        static void RegisterFallback(HandlerFunc handler);

        /// Dieu huong request den Handler tuong ung
        static void Dispatch(int fd, const HttpRequest &req);

    private:
        static std::string MethodToString(HttpMethod method);
        static void SendErrorResponse(int fd, int status_code,
                                      const std::string &status_msg,
                                      const std::string &body);

        static std::unordered_map<std::string, HandlerFunc> routes;
        static HandlerFunc fallback_handler;
    };

} // namespace Http

#endif // CPPSERVER_HTTP_ROUTER_H
