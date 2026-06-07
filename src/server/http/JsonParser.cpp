/*!
    \file JsonParser.cpp
    \brief JSON Parser with SSE2 SIMD acceleration
    \author HungForre
    \date 7/6/2026
    \copyright VDT
*/

#include "server/http/JsonParser.h"
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <sstream>

// SSE2 có mặt trên mọi CPU x86_64
#if defined(__x86_64__) || defined(_M_X64)
#include <emmintrin.h> // SSE2
#define WEBSERVER_SSE2 1
#endif

namespace Http
{

    // ════════════════════════════════════════════════════════════════════════════
    //  SIMD Helpers
    // ════════════════════════════════════════════════════════════════════════════

    /// Dùng SSE2 để bỏ qua whitespace nhanh hơn duyệt từng byte
    static int SkipWsSIMD(std::string_view s, int i)
    {
#ifdef WEBSERVER_SSE2
        const char *p = s.data() + i;
        int n = s.size() - i;

        // Xử lý 16 byte mỗi lần bằng SSE2
        while (n >= 16)
        {
            __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i *>(p));

            // Kiểm tra 4 loại whitespace cùng lúc
            __m128i sp = _mm_cmpeq_epi8(chunk, _mm_set1_epi8(' '));
            __m128i tb = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\t'));
            __m128i nl = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\n'));
            __m128i cr = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\r'));

            __m128i ws = _mm_or_si128(_mm_or_si128(sp, tb), _mm_or_si128(nl, cr));
            int mask = _mm_movemask_epi8(ws);

            if (mask != 0xFFFF)
            {
                // Tìm byte đầu tiên KHÔNG phải whitespace
                int skip = __builtin_ctz(~mask);
                return i + (int)(p - (s.data() + i)) + (int)skip;
            }
            p += 16;
            n -= 16;
            i += 16;
        }
#endif
        // Fallback scalar cho phần còn lại
        while (i < s.size() &&
               (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
            ++i;
        return i;
    }

    /// Dùng SSE2 tìm ký tự kết thúc của JSON string ('" hoặc '\')
    static int FindStringEnd(std::string_view s, int i)
    {
#ifdef WEBSERVER_SSE2
        const char *p = s.data() + i;
        int n = s.size() - i;

        while (n >= 16)
        {
            __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i *>(p));
            __m128i quotes = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('"'));
            __m128i esc = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\\'));
            __m128i hits = _mm_or_si128(quotes, esc);
            int mask = _mm_movemask_epi8(hits);

            if (mask != 0)
            {
                int pos = __builtin_ctz(mask);
                return i + (int)(p - (s.data() + i)) + (int)pos;
            }
            p += 16;
            n -= 16;
            i += 16;
        }
#endif
        while (i < s.size() && s[i] != '"' && s[i] != '\\')
            ++i;
        return i;
    }

    // ════════════════════════════════════════════════════════════════════════════
    //  Internal parse functions
    // ════════════════════════════════════════════════════════════════════════════

    void JsonParser::SkipWhitespace(std::string_view json, int &idx)
    {
        idx = SkipWsSIMD(json, idx);
    }

    JsonNode *JsonParser::ParseString(std::string_view json, int &idx)
    {
        idx++; // Bỏ qua '"' mở đầu
        int start = idx;

        while (idx < json.size())
        {
            idx = FindStringEnd(json, idx);
            if (idx >= json.size())
                break;

            if (json[idx] == '\\')
            {
                // Bỏ qua ký tự escape
                idx += 2;
                continue;
            }
            // Gặp '"' kết thúc
            break;
        }

        JsonNode *node = new JsonNode{};
        node->type = JsonType::STRING;
        node->val_str = std::string_view(json.data() + start, idx - start);
        idx++; // Bỏ qua '"' đóng
        return node;
    }

    JsonNode *JsonParser::ParsePrimitive(std::string_view json, int &idx)
    {
        int start = idx;
        while (idx < json.size())
        {
            char c = json[idx];
            if (c == ',' || c == '}' || c == ']' ||
                c == ' ' || c == '\t' || c == '\n' || c == '\r')
                break;
            ++idx;
        }
        std::string_view val(json.data() + start, idx - start);

        JsonNode *node = new JsonNode{};
        node->val_str = val;

        if (val == "true" || val == "false")
            node->type = JsonType::BOOLEAN;
        else if (val == "null")
            node->type = JsonType::NUL;
        else
            node->type = JsonType::NUMBER;

        return node;
    }

    JsonNode *JsonParser::ParseArray(std::string_view json, int &idx)
    {
        idx++; // Bỏ qua '['
        JsonNode *arr = new JsonNode{};
        arr->type = JsonType::ARRAY;
        JsonNode *tail = nullptr;

        while (idx < json.size())
        {
            SkipWhitespace(json, idx);
            if (idx >= json.size())
                break;
            if (json[idx] == ']')
            {
                idx++;
                break;
            }
            if (json[idx] == ',')
            {
                idx++;
                continue;
            }

            JsonNode *elem = ParseValue(json, idx);
            if (!elem)
                break;

            if (!arr->child)
                arr->child = elem;
            else
                tail->next = elem;
            tail = elem;
        }
        return arr;
    }

    JsonNode *JsonParser::ParseObject(std::string_view json, int &idx)
    {
        idx++; // Bỏ qua '{'
        JsonNode *obj = new JsonNode{};
        obj->type = JsonType::OBJECT;
        JsonNode *tail = nullptr;

        while (idx < json.size())
        {
            SkipWhitespace(json, idx);
            if (idx >= json.size())
                break;
            if (json[idx] == '}')
            {
                idx++;
                break;
            }
            if (json[idx] == ',')
            {
                idx++;
                continue;
            }
            if (json[idx] != '"')
            {
                idx++;
                continue;
            } // Bỏ qua ký tự lạ

            // Parse key
            JsonNode *key_node = ParseString(json, idx);
            std::string_view key = key_node->val_str;
            delete key_node;

            // Bỏ qua ':'
            SkipWhitespace(json, idx);
            if (idx < json.size() && json[idx] == ':')
                idx++;

            // Parse value
            JsonNode *val = ParseValue(json, idx);
            if (val)
            {
                val->key = key;
                if (!obj->child)
                    obj->child = val;
                else
                    tail->next = val;
                tail = val;
            }
        }
        return obj;
    }

    JsonNode *JsonParser::ParseValue(std::string_view json, int &idx)
    {
        SkipWhitespace(json, idx);
        if (idx >= json.size())
            return nullptr;

        switch (json[idx])
        {
        case '{':
            return ParseObject(json, idx);
        case '[':
            return ParseArray(json, idx);
        case '"':
            return ParseString(json, idx);
        default:
            return ParsePrimitive(json, idx);
        }
    }

    // ════════════════════════════════════════════════════════════════════════════
    //  Public API — JsonParser
    // ════════════════════════════════════════════════════════════════════════════

    JsonNode *JsonParser::Parse(std::string_view json)
    {
        int idx = 0;
        return ParseValue(json, idx);
    }

    void JsonParser::Free(JsonNode *node)
    {
        if (!node)
            return;
        Free(node->child);
        Free(node->next);
        delete node;
    }

    JsonNode *JsonParser::Get(JsonNode *node, std::string_view key)
    {
        if (!node || node->type != JsonType::OBJECT)
            return nullptr;
        JsonNode *curr = node->child;
        while (curr)
        {
            if (curr->key == key)
                return curr;
            curr = curr->next;
        }
        return nullptr;
    }

    bool JsonParser::Has(JsonNode *node, std::string_view key)
    {
        return Get(node, key) != nullptr;
    }

    std::string_view JsonParser::GetString(JsonNode *node, std::string_view key)
    {
        JsonNode *n = Get(node, key);
        if (!n || n->type != JsonType::STRING)
            return {};
        return n->val_str;
    }

    int JsonParser::GetInt(JsonNode *node, std::string_view key)
    {
        JsonNode *n = Get(node, key);
        if (!n || n->type != JsonType::NUMBER)
            return 0;
        return std::atoi(std::string(n->val_str).c_str());
    }

    double JsonParser::GetDouble(JsonNode *node, std::string_view key)
    {
        JsonNode *n = Get(node, key);
        if (!n || n->type != JsonType::NUMBER)
            return 0.0;
        return std::atof(std::string(n->val_str).c_str());
    }

    bool JsonParser::GetBool(JsonNode *node, std::string_view key)
    {
        JsonNode *n = Get(node, key);
        if (!n || n->type != JsonType::BOOLEAN)
            return false;
        return n->val_str == "true";
    }

    // ════════════════════════════════════════════════════════════════════════════
    //  Public API — JsonBuilder
    // ════════════════════════════════════════════════════════════════════════════

    JsonNode *JsonBuilder::NewObject()
    {
        auto *n = new JsonNode{};
        n->type = JsonType::OBJECT;
        return n;
    }

    JsonNode *JsonBuilder::NewArray()
    {
        auto *n = new JsonNode{};
        n->type = JsonType::ARRAY;
        return n;
    }

    JsonNode *JsonBuilder::NewString(std::string_view key, std::string_view value)
    {
        auto *n = new JsonNode{};
        n->type = JsonType::STRING;
        n->key = key;
        n->val_str = value;
        return n;
    }

    JsonNode *JsonBuilder::NewInt(std::string_view key, int value)
    {
        // val_str cần lifetime ổn định -> dùng string nội bộ
        // (Lưu ý: cách đơn giản này, val_str trỏ vào temporary!)
        // Giải pháp: lưu chuỗi vào một buffer riêng trong node
        // Để đơn giản, người dùng nên dùng Serialize() để chuyển ra chuỗi
        auto *n = new JsonNode{};
        n->type = JsonType::NUMBER;
        n->key = key;
        // Dùng heap-allocated string để val_str tồn tại lâu dài
        char *buf = new char[32];
        snprintf(buf, 32, "%d", value);
        n->val_str = std::string_view(buf, strlen(buf));
        return n;
    }

    JsonNode *JsonBuilder::NewDouble(std::string_view key, double value)
    {
        auto *n = new JsonNode{};
        n->type = JsonType::NUMBER;
        n->key = key;
        char *buf = new char[64];
        snprintf(buf, 64, "%g", value);
        n->val_str = std::string_view(buf, strlen(buf));
        return n;
    }

    JsonNode *JsonBuilder::NewBool(std::string_view key, bool value)
    {
        auto *n = new JsonNode{};
        n->type = JsonType::BOOLEAN;
        n->key = key;
        n->val_str = value ? std::string_view("true") : std::string_view("false");
        return n;
    }

    void JsonBuilder::Append(JsonNode *parent, JsonNode *child)
    {
        if (!parent || !child)
            return;
        if (!parent->child)
        {
            parent->child = child;
            return;
        }
        JsonNode *curr = parent->child;
        while (curr->next)
            curr = curr->next;
        curr->next = child;
    }

    static void SerializeNode(const JsonNode *node, std::ostringstream &oss);

    static void SerializeValue(const JsonNode *node, std::ostringstream &oss)
    {
        switch (node->type)
        {
        case JsonType::STRING:
            oss << '"';
            // Escape ký tự đặc biệt
            for (char c : node->val_str)
            {
                if (c == '"')
                    oss << "\\\"";
                else if (c == '\\')
                    oss << "\\\\";
                else if (c == '\n')
                    oss << "\\n";
                else if (c == '\r')
                    oss << "\\r";
                else if (c == '\t')
                    oss << "\\t";
                else
                    oss << c;
            }
            oss << '"';
            break;
        case JsonType::NUMBER:
        case JsonType::BOOLEAN:
            oss << node->val_str;
            break;
        case JsonType::NUL:
            oss << "null";
            break;
        case JsonType::OBJECT:
            oss << '{';
            {
                JsonNode *curr = node->child;
                bool first = true;
                while (curr)
                {
                    if (!first)
                        oss << ',';
                    first = false;
                    oss << '"' << curr->key << "\":";
                    SerializeValue(curr, oss);
                    curr = curr->next;
                }
            }
            oss << '}';
            break;
        case JsonType::ARRAY:
            oss << '[';
            {
                JsonNode *curr = node->child;
                bool first = true;
                while (curr)
                {
                    if (!first)
                        oss << ',';
                    first = false;
                    SerializeValue(curr, oss);
                    curr = curr->next;
                }
            }
            oss << ']';
            break;
        }
    }

    std::string JsonBuilder::Serialize(const JsonNode *node)
    {
        if (!node)
            return "null";
        std::ostringstream oss;
        SerializeValue(node, oss);
        return oss.str();
    }

} // namespace Http