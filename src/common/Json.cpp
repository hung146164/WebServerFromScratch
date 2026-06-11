/*!
    \file Json.cpp
    \brief Custom JSON parser/serializer using C++17
    \author Antigravity
    \date 9/6/2026
    \copyright VDT
*/
#include "common/Json.h"
#include <stdexcept>
#include <cctype>
#include <algorithm>

// ─── Constructors & Destructor ──────────────────────────────────────────────

Json::Json() : type_(Type::Null), value_(nullptr) {}

Json::Json(std::nullptr_t) : type_(Type::Null), value_(nullptr) {}

Json::Json(bool value) : type_(Type::Bool), value_(value) {}

Json::Json(int value) : type_(Type::Number), value_(static_cast<double>(value)) {}

Json::Json(double value) : type_(Type::Number), value_(value) {}

Json::Json(const char *value) : type_(Type::String), value_(std::string(value)) {}

Json::Json(const std::string &value) : type_(Type::String), value_(value) {}

Json::Json(std::string_view value) : type_(Type::String), value_(std::string(value)) {}

Json::Json(const Json &other) : type_(other.type_)
{
    switch (other.type_)
    {
    case Type::Null:
        value_ = nullptr;
        break;
    case Type::Bool:
        value_ = std::get<bool>(other.value_);
        break;
    case Type::Number:
        value_ = std::get<double>(other.value_);
        break;
    case Type::String:
        value_ = std::get<std::string>(other.value_);
        break;
    case Type::Array:
        value_ = std::make_unique<std::vector<Json>>(*std::get<std::unique_ptr<std::vector<Json>>>(other.value_));
        break;
    case Type::Object:
        value_ = std::make_unique<std::map<std::string, Json>>(*std::get<std::unique_ptr<std::map<std::string, Json>>>(other.value_));
        break;
    }
}

Json &Json::operator=(const Json &other)
{
    if (this != &other)
    {
        Json temp(other);
        std::swap(type_, temp.type_);
        std::swap(value_, temp.value_);
    }
    return *this;
}

Json::Json(Json &&other) noexcept = default;
Json &Json::operator=(Json &&other) noexcept = default;

Json::~Json() = default;

// ─── Factory Methods ────────────────────────────────────────────────────────

Json Json::Array()
{
    Json j;
    j.type_ = Type::Array;
    j.value_ = std::make_unique<std::vector<Json>>();
    return j;
}

Json Json::Object()
{
    Json j;
    j.type_ = Type::Object;
    j.value_ = std::make_unique<std::map<std::string, Json>>();
    return j;
}

// ─── Getters ────────────────────────────────────────────────────────────────

bool Json::AsBool() const
{
    if (type_ != Type::Bool)
        throw std::runtime_error("Not a boolean");
    return std::get<bool>(value_);
}

double Json::AsDouble() const
{
    if (type_ != Type::Number)
        throw std::runtime_error("Not a number");
    return std::get<double>(value_);
}

int Json::AsInt() const
{
    if (type_ != Type::Number)
        throw std::runtime_error("Not a number");
    return static_cast<int>(std::get<double>(value_));
}

const std::string &Json::AsString() const
{
    if (type_ != Type::String)
        throw std::runtime_error("Not a string");
    return std::get<std::string>(value_);
}

const std::vector<Json> &Json::AsArray() const
{
    if (type_ != Type::Array)
        throw std::runtime_error("Not an array");
    return *std::get<std::unique_ptr<std::vector<Json>>>(value_);
}

std::vector<Json> &Json::AsArray()
{
    if (type_ != Type::Array)
        throw std::runtime_error("Not an array");
    return *std::get<std::unique_ptr<std::vector<Json>>>(value_);
}

const std::map<std::string, Json> &Json::AsObject() const
{
    if (type_ != Type::Object)
        throw std::runtime_error("Not an object");
    return *std::get<std::unique_ptr<std::map<std::string, Json>>>(value_);
}

std::map<std::string, Json> &Json::AsObject()
{
    if (type_ != Type::Object)
        throw std::runtime_error("Not an object");
    return *std::get<std::unique_ptr<std::map<std::string, Json>>>(value_);
}

// ─── Operators ──────────────────────────────────────────────────────────────

Json &Json::operator[](size_t index)
{
    if (type_ != Type::Array)
        throw std::runtime_error("Not an array");
    auto &arr = *std::get<std::unique_ptr<std::vector<Json>>>(value_);
    if (index >= arr.size())
        throw std::out_of_range("Index out of bounds");
    return arr[index];
}

const Json &Json::operator[](size_t index) const
{
    if (type_ != Type::Array)
        throw std::runtime_error("Not an array");
    const auto &arr = *std::get<std::unique_ptr<std::vector<Json>>>(value_);
    if (index >= arr.size())
        throw std::out_of_range("Index out of bounds");
    return arr[index];
}

Json &Json::operator[](const std::string &key)
{
    if (type_ == Type::Null)
    {
        type_ = Type::Object;
        value_ = std::make_unique<std::map<std::string, Json>>();
    }
    if (type_ != Type::Object)
        throw std::runtime_error("Not an object");
    return (*std::get<std::unique_ptr<std::map<std::string, Json>>>(value_))[key];
}

const Json &Json::operator[](const std::string &key) const
{
    if (type_ != Type::Object)
        throw std::runtime_error("Not an object");
    const auto &obj = *std::get<std::unique_ptr<std::map<std::string, Json>>>(value_);
    auto it = obj.find(key);
    if (it == obj.end())
        throw std::out_of_range("Key not found: " + key);
    return it->second;
}

bool Json::HasKey(const std::string &key) const
{
    if (type_ != Type::Object)
        return false;
    const auto &obj = *std::get<std::unique_ptr<std::map<std::string, Json>>>(value_);
    return obj.find(key) != obj.end();
}

void Json::PushBack(const Json &value)
{
    if (type_ == Type::Null)
    {
        type_ = Type::Array;
        value_ = std::make_unique<std::vector<Json>>();
    }
    if (type_ != Type::Array)
        throw std::runtime_error("Not an array");
    std::get<std::unique_ptr<std::vector<Json>>>(value_)->push_back(value);
}

void Json::PushBack(Json &&value)
{
    if (type_ == Type::Null)
    {
        type_ = Type::Array;
        value_ = std::make_unique<std::vector<Json>>();
    }
    if (type_ != Type::Array)
        throw std::runtime_error("Not an array");
    std::get<std::unique_ptr<std::vector<Json>>>(value_)->push_back(std::move(value));
}

size_t Json::Size() const
{
    if (type_ == Type::Array)
        return std::get<std::unique_ptr<std::vector<Json>>>(value_)->size();
    if (type_ == Type::Object)
        return std::get<std::unique_ptr<std::map<std::string, Json>>>(value_)->size();
    return 0;
}

// ─── Serialization ──────────────────────────────────────────────────────────

std::string Json::Dump(int indent) const
{
    std::ostringstream oss;
    DumpHelper(oss, indent, 0);
    return oss.str();
}

void Json::DumpHelper(std::ostringstream &oss, int indent, int current_indent) const
{
    switch (type_)
    {
    case Type::Null:
        oss << "null";
        break;
    case Type::Bool:
        oss << (std::get<bool>(value_) ? "true" : "false");
        break;
    case Type::Number:
        oss << std::get<double>(value_);
        break;
    case Type::String:
        oss << "\"" << EscapeString(std::get<std::string>(value_)) << "\"";
        break;
    case Type::Array:
    {
        const auto &arr = *std::get<std::unique_ptr<std::vector<Json>>>(value_);
        if (arr.empty())
        {
            oss << "[]";
            break;
        }
        oss << "[";
        if (indent >= 0)
            oss << "\n";
        for (size_t i = 0; i < arr.size(); ++i)
        {
            if (indent >= 0)
                oss << std::string(static_cast<size_t>(current_indent + indent), ' ');
            arr[i].DumpHelper(oss, indent, current_indent + indent);
            if (i + 1 < arr.size())
                oss << ",";
            if (indent >= 0)
                oss << "\n";
        }
        if (indent >= 0)
            oss << std::string(static_cast<size_t>(current_indent), ' ');
        oss << "]";
        break;
    }
    case Type::Object:
    {
        const auto &obj = *std::get<std::unique_ptr<std::map<std::string, Json>>>(value_);
        if (obj.empty())
        {
            oss << "{}";
            break;
        }
        oss << "{";
        if (indent >= 0)
            oss << "\n";
        size_t count = 0;
        for (const auto &[k, v] : obj)
        {
            if (indent >= 0)
                oss << std::string(static_cast<size_t>(current_indent + indent), ' ');
            oss << "\"" << EscapeString(k) << "\":";
            if (indent >= 0)
                oss << " ";
            v.DumpHelper(oss, indent, current_indent + indent);
            if (++count < obj.size())
                oss << ",";
            if (indent >= 0)
                oss << "\n";
        }
        if (indent >= 0)
            oss << std::string(static_cast<size_t>(current_indent), ' ');
        oss << "}";
        break;
    }
    }
}

std::string Json::EscapeString(const std::string &str)
{
    std::string escaped;
    escaped.reserve(str.size());
    for (char c : str)
    {
        switch (c)
        {
        case '\"':
            escaped += "\\\"";
            break;
        case '\\':
            escaped += "\\\\";
            break;
        case '\b':
            escaped += "\\b";
            break;
        case '\f':
            escaped += "\\f";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 32)
            {
                char buf[7];
                std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                escaped += buf;
            }
            else
            {
                escaped += c;
            }
            break;
        }
    }
    return escaped;
}

// ─── JSON Parser Implementation ─────────────────────────────────────────────

class JsonParser
{
public:
    explicit JsonParser(std::string_view src) : src_(src), pos_(0) {}

    Json Parse()
    {
        SkipWhitespace();
        if (pos_ >= src_.size())
        {
            throw std::runtime_error("Unexpected end of JSON input");
        }
        Json val = ParseValue();
        SkipWhitespace();
        if (pos_ < src_.size())
        {
            throw std::runtime_error("Extra characters after JSON object");
        }
        return val;
    }

private:
    std::string_view src_;
    size_t pos_;

    void SkipWhitespace()
    {
        while (pos_ < src_.size() && (src_[pos_] == ' ' || src_[pos_] == '\t' || src_[pos_] == '\r' || src_[pos_] == '\n'))
        {
            pos_++;
        }
    }

    char Peek()
    {
        if (pos_ >= src_.size())
            return '\0';
        return src_[pos_];
    }

    char GetNext()
    {
        if (pos_ >= src_.size())
            return '\0';
        return src_[pos_++];
    }

    Json ParseValue()
    {
        SkipWhitespace();
        char c = Peek();
        if (c == 'n')
            return ParseNull();
        if (c == 't' || c == 'f')
            return ParseBool();
        if (c == '"')
            return ParseString();
        if (c == '[')
            return ParseArray();
        if (c == '{')
            return ParseObject();
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c)))
            return ParseNumber();
        throw std::runtime_error(std::string("Unexpected character: ") + c);
    }

    Json ParseNull()
    {
        if (pos_ + 4 <= src_.size() && src_.substr(pos_, 4) == "null")
        {
            pos_ += 4;
            return Json(nullptr);
        }
        throw std::runtime_error("Invalid JSON null literal");
    }

    Json ParseBool()
    {
        if (pos_ + 4 <= src_.size() && src_.substr(pos_, 4) == "true")
        {
            pos_ += 4;
            return Json(true);
        }
        if (pos_ + 5 <= src_.size() && src_.substr(pos_, 5) == "false")
        {
            pos_ += 5;
            return Json(false);
        }
        throw std::runtime_error("Invalid JSON boolean literal");
    }

    Json ParseString()
    {
        GetNext(); // consume opening '"'
        std::string s;
        while (pos_ < src_.size())
        {
            char c = GetNext();
            if (c == '"')
            {
                return Json(s);
            }
            if (c == '\\')
            {
                if (pos_ >= src_.size())
                    throw std::runtime_error("Incomplete escape sequence");
                char esc = GetNext();
                switch (esc)
                {
                case '"':
                    s += '"';
                    break;
                case '\\':
                    s += '\\';
                    break;
                case '/':
                    s += '/';
                    break;
                case 'b':
                    s += '\b';
                    break;
                case 'f':
                    s += '\f';
                    break;
                case 'n':
                    s += '\n';
                    break;
                case 'r':
                    s += '\r';
                    break;
                case 't':
                    s += '\t';
                    break;
                case 'u':
                {
                    if (pos_ + 4 > src_.size())
                        throw std::runtime_error("Incomplete unicode escape");
                    std::string hex(src_.substr(pos_, 4));
                    pos_ += 4;
                    unsigned int codepoint = std::stoul(hex, nullptr, 16);
                    if (codepoint >= 0xD800 && codepoint <= 0xDBFF)
                    {
                        if (pos_ + 6 > src_.size() || src_[pos_] != '\\' || src_[pos_ + 1] != 'u')
                        {
                            throw std::runtime_error("Incomplete high surrogate unicode escape");
                        }
                        pos_ += 2;
                        std::string hex2(src_.substr(pos_, 4));
                        pos_ += 4;
                        unsigned int low = std::stoul(hex2, nullptr, 16);
                        if (low < 0xDC00 || low > 0xDFFF)
                            throw std::runtime_error("Invalid low surrogate");
                        codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                    }

                    if (codepoint <= 0x7F)
                    {
                        s += static_cast<char>(codepoint);
                    }
                    else if (codepoint <= 0x7FF)
                    {
                        s += static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F));
                        s += static_cast<char>(0x80 | (codepoint & 0x3F));
                    }
                    else if (codepoint <= 0xFFFF)
                    {
                        s += static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F));
                        s += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                        s += static_cast<char>(0x80 | (codepoint & 0x3F));
                    }
                    else
                    {
                        s += static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07));
                        s += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                        s += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                        s += static_cast<char>(0x80 | (codepoint & 0x3F));
                    }
                    break;
                }
                default:
                    throw std::runtime_error("Unknown string escape character");
                }
            }
            else
            {
                s += c;
            }
        }
        throw std::runtime_error("Unterminated JSON string");
    }

    Json ParseNumber()
    {
        size_t start = pos_;
        if (Peek() == '-')
            pos_++;

        if (Peek() == '0')
        {
            pos_++;
        }
        else if (std::isdigit(static_cast<unsigned char>(Peek())))
        {
            while (std::isdigit(static_cast<unsigned char>(Peek())))
                pos_++;
        }
        else
        {
            throw std::runtime_error("Invalid number format");
        }

        if (Peek() == '.')
        {
            pos_++;
            if (!std::isdigit(static_cast<unsigned char>(Peek())))
                throw std::runtime_error("Incomplete decimal fraction in number");
            while (std::isdigit(static_cast<unsigned char>(Peek())))
                pos_++;
        }

        if (Peek() == 'e' || Peek() == 'E')
        {
            pos_++;
            if (Peek() == '+' || Peek() == '-')
                pos_++;
            if (!std::isdigit(static_cast<unsigned char>(Peek())))
                throw std::runtime_error("Incomplete exponent in number");
            while (std::isdigit(static_cast<unsigned char>(Peek())))
                pos_++;
        }

        std::string num_str(src_.substr(start, pos_ - start));
        double val = std::stod(num_str);
        return Json(val);
    }

    Json ParseArray()
    {
        GetNext(); // consume opening '['
        Json arr = Json::Array();
        SkipWhitespace();
        if (Peek() == ']')
        {
            GetNext();
            return arr;
        }

        while (true)
        {
            arr.PushBack(ParseValue());
            SkipWhitespace();
            char c = GetNext();
            if (c == ']')
                break;
            if (c != ',')
                throw std::runtime_error("Expected ',' or ']' in JSON array");
        }
        return arr;
    }

    Json ParseObject()
    {
        GetNext(); // consume opening '{'
        Json obj = Json::Object();
        SkipWhitespace();
        if (Peek() == '}')
        {
            GetNext();
            return obj;
        }

        while (true)
        {
            SkipWhitespace();
            if (Peek() != '"')
                throw std::runtime_error("Expected string key in JSON object");
            Json key_json = ParseString();
            std::string key = key_json.AsString();

            SkipWhitespace();
            if (GetNext() != ':')
                throw std::runtime_error("Expected ':' after JSON key");

            obj[key] = ParseValue();

            SkipWhitespace();
            char c = GetNext();
            if (c == '}')
                break;
            if (c != ',')
                throw std::runtime_error("Expected ',' or '}' in JSON object");
        }
        return obj;
    }
};

Json Json::Parse(std::string_view json_str)
{
    JsonParser parser(json_str);
    return parser.Parse();
}
