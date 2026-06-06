/*!
    \file HttpMethod.h
    \brief HttpMethod
    \author HungForre
    \date 6/6/2026
    \copyright VDT
*/

#ifndef CPPSERVER_HTTP_HTTPMETHOD_H
#define CPPSERVER_HTTP_HTTPMETHOD_H

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

#endif