/*!
    \file JsonDocument.cpp
    \brief JsonDocument implementation
    \author HungForre
    \date 12/6/2026
    \copyright VDT
*/
#include "common/Json/JsonDocument.h"
#include "common/Json/JsonParser.h"

namespace Json
{

    JsonDocument::JsonDocument(size_t start_idx_, JsonNode *node_, JsonParser *parser_)
        : start_idx(start_idx_), node(node_), parser(parser_)
    {
    }

    JsonDocument::~JsonDocument()
    {
        if (parser != nullptr)
        {
            parser->release(start_idx);
        }
    }

    JsonDocument::JsonDocument(JsonDocument &&other) noexcept
        : start_idx(other.start_idx), node(other.node), parser(other.parser)
    {
        other.parser = nullptr;
    }

    JsonDocument &JsonDocument::operator=(JsonDocument &&other) noexcept
    {
        if (this != &other)
        {
            if (parser != nullptr)
                parser->release(start_idx);
            start_idx = other.start_idx;
            node = other.node;
            parser = other.parser;
            other.parser = nullptr;
        }
        return *this;
    }

    JsonDocument::JsonDocument(const JsonDocument &other)
        : start_idx(other.start_idx), node(other.node), parser(nullptr)
    {
    }

    JsonDocument &JsonDocument::operator=(const JsonDocument &other)
    {
        if (this != &other)
        {
            if (parser != nullptr)
                parser->release(start_idx);
            start_idx = other.start_idx;
            node = other.node;
            parser = nullptr;
        }
        return *this;
    }

    JsonDocument JsonDocument::operator[](const char *key)
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

    JsonDocument JsonDocument::operator[](const int index)
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

    JsonNode *JsonDocument::GetNode()
    {
        return node;
    }

    std::string JsonDocument::ToString() const
    {
        if (node != nullptr && node->value.data() != nullptr)
        {
            return std::string(node->value);
        }
        return "";
    }

    JsonDocument::operator std::string() const
    {
        return ToString();
    }
}