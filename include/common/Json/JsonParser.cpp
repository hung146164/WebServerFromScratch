/*!
    \file JsonParser.h
    \brief JsonParser implementation
    \author HungForre
    \date 12/6/2026
    \copyright VDT
*/
#include <string_view>
#include <vector>
#include <stdexcept>

#include "common/Json/JsonParser.h"


namespace Json
{
    JsonParser::JsonParser(int number_node_pool_)
    {
        node_pool.resize(number_node_pool_);
    }
    JsonDocument JsonParser::Parse(std::string_view body)
    {
        std::vector<JsonNode> st;
        JsonDocument jsonDocument(current_node_idx,&node_pool[current_node_idx],this);
        for (int i = 0; i < body.size(); i++)
        {
            if(body[i]=='{')
            {
                i++;
                
            }
            if(body[i]=='"')
            {
                i++;
                int start=i;
                while(i<body.size() && body[i]!='"') i++;
                
            }
        }
    }
    void JsonParser::release(size_t marker)
    {
        if (current_node_idx < marker || marker < 0)
        {
            throw std::underflow_error("JsonParser::release: Invalid marker or stack underflow.");
        }
        current_node_idx = marker;
    }
};
