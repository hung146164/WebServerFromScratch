/*!
    \file JsonNode.h
    \brief JsonNode defination
    \author HungForre
    \date 12/6/2026
    \copyright VDT
*/
#ifndef CPPSERVER_COMMON_JSONNODE_H
#define CPPSERVER_COMMON_JSONNODE_H

#include <string_view>

#include "JsonType.h"

namespace Json
{
    struct JsonNode
    {
        std::string_view key;
        std::string_view value;
        JsonType type = JsonType::NUL;
        JsonNode *child = nullptr;
        JsonNode *next = nullptr;
    };
}

#endif