/*!
    \file JsonDocument.h
    \brief JsonDocument defination
    \author HungForre
    \date 12/6/2026
    \copyright VDT
*/
#ifndef CPPSERVER_COMMON_JSONDOCUMENT_H
#define CPPSERVER_COMMON_JSONDOCUMENT_H

#include "JsonNode.h"
#include "JsonParser.h"
#include "JsonType.h"

namespace Json
{
    class JsonDocument
    {
    private:
        size_t start_idx = 0;
        JsonNode *node = nullptr;
        JsonParser *parser = nullptr;

    public:
        JsonDocument(size_t start_idx_, JsonNode *node_, JsonParser *parser_)
            : start_idx(start_idx_), node(node_), parser(parser_)
        {
        }
        ~JsonDocument()
        {
            parser->release(start_idx);
        }
        JsonDocument operator[](const char *key)
        {
            if (node != nullptr && node->type == JsonType::OBJECT)
            {
                JsonNode *curr = node->child;
                while (curr != nullptr)
                {
                    if (curr->key == key)
                    {
                        return JsonDocument(0, curr, nullptr);
                    }
                    curr = curr->next;
                }
            }

            return JsonDocument(0, nullptr, nullptr);
        }
        JsonDocument operator[](const int index)
        {
            if (node != nullptr && node->type == JsonType::ARRAY && index >= 0)
            {
                JsonNode *curr = node->child;
                int current_idx = 0;
                while (curr != nullptr)
                {
                    if (current_idx == index)
                    {
                        return JsonDocument(0, curr, nullptr);
                    }

                    current_idx++;
                    curr = curr->next;
                }
            }

            return JsonDocument(0, nullptr, nullptr);
        }
    };
}

#endif