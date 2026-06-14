/*!
    \file JsonDocument.h
    \brief JsonDocument definition
    \author HungForre
    \date 12/6/2026
    \copyright VDT
*/
#ifndef CPPSERVER_COMMON_JSONDOCUMENT_H
#define CPPSERVER_COMMON_JSONDOCUMENT_H

#include <string>
#include <cstddef>
#include "JsonNode.h"
#include "JsonType.h"

namespace Json
{

    class JsonParser;

    class JsonDocument
    {
    private:
        size_t start_idx = 0;
        JsonNode *node = nullptr;
        JsonParser *parser = nullptr;

    public:
        JsonDocument(size_t start_idx_, JsonNode *node_, JsonParser *parser_);
        ~JsonDocument();

        JsonDocument(JsonDocument &&other) noexcept;
        JsonDocument &operator=(JsonDocument &&other) noexcept;

        JsonDocument(const JsonDocument &other);
        JsonDocument &operator=(const JsonDocument &other);

        JsonDocument operator[](const char *key);
        JsonDocument operator[](const int index);
        JsonNode *GetNode();
        std::string ToString() const;
        operator std::string() const;
    };
}

#endif // CPPSERVER_COMMON_JSONDOCUMENT_H