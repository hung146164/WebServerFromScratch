/*!
    \file JsonParser.h
    \brief JsonParser defination
    \author HungForre
    \date 12/6/2026
    \copyright VDT
*/
#ifndef CPPSERVER_COMMON_JSONPARSER_H
#define CPPSERVER_COMMON_JSONPARSER_H

#include <string_view>
#include <vector>

#include "JsonNode.h"
#include "JsonDocument.h"

namespace Json
{
    class JsonParser
    {

    private:
        std::vector<JsonNode> node_pool;
        size_t current_node_idx = 0;
        JsonNode GetNode()
        {
            if (current_node_idx < node_pool.size())
            {
                JsonNode curr = node_pool[current_node_idx];
                current_node_idx++;
                return curr;
            }
            return nullptr;
        }

    public:
        JsonParser(int number_node_pool_);
        JsonDocument Parse(std::string_view body);
        void release(size_t marker);
    };
};

#endif