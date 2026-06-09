/*!
    \file Json.h
    \brief Custom JSON parser/serializer using C++17
    \author Antigravity
    \date 9/6/2026
    \copyright VDT
*/
#ifndef CPPSERVER_COMMON_JSON_H
#define CPPSERVER_COMMON_JSON_H

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <memory>
#include <string_view>
#include <iostream>
#include <sstream>

class Json
{
public:
    enum class Type
    {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };

    // Constructors
    Json();
    Json(std::nullptr_t);
    Json(bool value);
    Json(int value);
    Json(double value);
    Json(const char *value);
    Json(const std::string &value);
    Json(std::string_view value);

    // Copy constructor & assignment (Deep Copy)
    Json(const Json &other);
    Json &operator=(const Json &other);

    // Move constructor & assignment
    Json(Json &&other) noexcept;
    Json &operator=(Json &&other) noexcept;

    ~Json();

    // Factory methods
    static Json Array();
    static Json Object();

    // Parsing and Serializing
    static Json Parse(std::string_view json_str);
    std::string Dump(int indent = -1) const;

    // Type checking
    Type GetType() const { return type_; }
    bool IsNull() const { return type_ == Type::Null; }
    bool IsBool() const { return type_ == Type::Bool; }
    bool IsNumber() const { return type_ == Type::Number; }
    bool IsString() const { return type_ == Type::String; }
    bool IsArray() const { return type_ == Type::Array; }
    bool IsObject() const { return type_ == Type::Object; }

    // Getter values
    bool AsBool() const;
    double AsDouble() const;
    int AsInt() const;
    const std::string &AsString() const;
    const std::vector<Json> &AsArray() const;
    std::vector<Json> &AsArray();
    const std::map<std::string, Json> &AsObject() const;
    std::map<std::string, Json> &AsObject();

    // Operators for array and object access
    Json &operator[](size_t index);
    const Json &operator[](size_t index) const;
    Json &operator[](const std::string &key);
    const Json &operator[](const std::string &key) const;

    // Utility helper for object keys
    bool HasKey(const std::string &key) const;

    // Array helper methods
    void PushBack(const Json &value);
    void PushBack(Json &&value);
    size_t Size() const;

private:
    Type type_ = Type::Null;

    // Variant containing scalar types and smart pointers to containers to support recursive definitions
    using ValueType = std::variant<
        std::nullptr_t,
        bool,
        double,
        std::string,
        std::unique_ptr<std::vector<Json>>,
        std::unique_ptr<std::map<std::string, Json>>>;

    ValueType value_;

    void DumpHelper(std::ostringstream &oss, int indent, int current_indent) const;
    static std::string EscapeString(const std::string &str);
};

#endif // CPPSERVER_COMMON_JSON_H
