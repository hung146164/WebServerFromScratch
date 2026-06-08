/*!
    \file JsonParser.h
    \brief JSON Parser and Serializer using linked list
    \author HungForre
    \date 8/6/2026
    \copyright VDT
*/
#ifndef CPPSERVER_HTTP_JSONPARSER_H
#define CPPSERVER_HTTP_JSONPARSER_H

#include <string_view>

namespace Http
{
    enum class JsonType
    {
        OBJECT,
        ARRAY,
        STRING,
        NUMBER,
        BOOLEAN,
        NONE,
    };
    struct JsonNode
    {
        JsonType type = JsonType::NONE;
        std::string_view key;
        std::string_view value;
        JsonNode *next = nullptr;
        JsonNode *child = nullptr;
    };

    class JsonParser
    {
    };

    class JsonBuilder
    {
    };

}

#endif