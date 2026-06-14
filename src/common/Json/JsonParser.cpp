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
    JsonNode *JsonParser::GetNode()
    {
        if (current_node_idx < node_pool.size())
        {
            CleanNode(&node_pool[current_node_idx]);
            return &node_pool[current_node_idx++];
        }
        return nullptr;
    }

    void JsonParser::CleanNode(JsonNode *node)
    {
        node->child = nullptr;
        node->next = nullptr;
        node->key = {};
        node->value = {};
        node->type = JsonType::NUL;
    }

    JsonDocument JsonParser::Parse(std::string_view body)
    {

        std::vector<JsonNode *> st;

        size_t first = current_node_idx;

        JsonNode *root_node = GetNode();

        JsonDocument jsonDocument(first, root_node, this);

        JsonNode *curr = root_node;

        for (int i = 0; i < body.size();)
        {
            bool retract = false;
            if (body[i] == '{')
            {
                curr->type = JsonType::OBJECT;

                st.push_back(curr);

                curr->child = GetNode();

                curr = curr->child;
            }
            else if (body[i] == '[')
            {
                curr->type = JsonType::ARRAY;

                st.push_back(curr);
                if (curr->child == nullptr)
                {
                    curr->child = GetNode();
                }
                curr = curr->child;
            }
            else if (body[i] == '"')
            {
                curr->type = JsonType::STRING;
                i++;
                int start = i;
                while (i < body.size() && body[i] != '"')
                    i++;

                std::string_view s = body.substr(start, i - start);

                if (!st.empty() && st.back()->type == JsonType::ARRAY)
                {
                    curr->value = s;
                }
                else if (!st.empty() && st.back()->type == JsonType::OBJECT)
                {
                    if (curr->key.size() == 0)
                    {
                        curr->key = s;
                    }
                    else
                    {
                        curr->value = s;
                    }
                }
            }
            else if (body[i] == ']' || body[i] == '}')
            {
                if (!st.empty())
                {
                    curr = st.back();
                    st.pop_back();
                }
            }
            else if (body[i] == ',')
            {
                curr->next = GetNode();
                curr = curr->next;
            }
            else if ((body[i] - '0' >= 0 && body[i] - '0' < 10) || body[i] == '.' || body[i] == '-')
            {
                curr->type = JsonType::NUMBER;
                int start = i;
                while (i < body.size() && ((body[i] - '0' >= 0 && body[i] - '0' < 10) || body[i] == '.' || body[i] == '-'))
                {
                    i++;
                }
                retract = true;

                std::string_view s = body.substr(start, i - start);

                if (!st.empty() && st.back()->type == JsonType::ARRAY)
                {
                    curr->value = s;
                }
                else if (!st.empty() && st.back()->type == JsonType::OBJECT)
                {
                    curr->value = s;
                }
            }
            else if (body[i] == 't' || body[i] == 'f')
            {
                curr->type = JsonType::BOOL;
                std::string_view s;
                if (body[i] == 't')
                {
                    s = body.substr(i, 4);
                    i += 3;
                }
                else if (body[i] == 'f')
                {
                    s = body.substr(i, 5);
                    i += 4;
                }
                curr->value = s;
            }
            else if (body[i] == 'n')
            {
                curr->type = JsonType::NUL;
                curr->value = body.substr(i, 4);
                i += 3;
            }
            if (!retract)
                i++;
        }
        return jsonDocument;
    }
    void JsonParser::release(size_t marker)
    {
        if (current_node_idx < marker)
        {
            throw std::underflow_error("JsonParser::release: Invalid marker or stack underflow.");
        }
        current_node_idx = marker;
    }
}
