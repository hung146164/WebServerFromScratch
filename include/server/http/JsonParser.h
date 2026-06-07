/*!
    \file JsonParser.h
    \brief JSON Parser and Serializer using linked list
    \author HungForre
    \date 7/6/2026
    \copyright VDT
*/
#ifndef CPPSERVER_HTTP_JSONPARSER_H
#define CPPSERVER_HTTP_JSONPARSER_H

#include <string_view>
#include <string>

namespace Http
{
    // ─── Cấu trúc dữ liệu ────────────────────────────────────────────────────

    enum class JsonType
    {
        OBJECT,
        ARRAY,
        STRING,
        NUMBER,
        BOOLEAN,
        NUL
    };

    struct JsonNode
    {
        JsonType type = JsonType::NUL;
        std::string_view key;     // Tên trường nếu nằm trong Object
        std::string_view val_str; // Giá trị thô (zero-copy, trỏ vào buffer gốc)

        JsonNode *next = nullptr;  // Phần tử kế tiếp cùng cấp (anh em)
        JsonNode *child = nullptr; // Phần tử con đầu tiên (nếu là Object/Array)
    };

    // ─── Bộ PARSE JSON (Chuỗi -> Cây DSLK) ──────────────────────────────────

    class JsonParser
    {
    public:
        /// Parse chuỗi JSON thành cây JsonNode
        /// Dùng khi: Handler nhận POST/PUT request có body JSON
        ///
        /// std::string_view body = req.body;  // '{"name":"Nam","age":20}'
        /// JsonNode* root = JsonParser::Parse(body);
        ///
        /// Tại sao dùng string_view: Zero-copy, các node trỏ thẳng vào
        /// buffer gốc của req.body, không tốn thêm bộ nhớ
        ///
        /// QUAN TRỌNG: root phải được giải phóng bằng Free(root) sau khi dùng
        static JsonNode *Parse(std::string_view json);

        /// Giải phóng toàn bộ cây JsonNode sau khi dùng xong
        /// Dùng khi: Cuối mỗi handler sau khi đã xử lý xong JSON
        static void Free(JsonNode *node);

        // ─── Hàm tra cứu trong cây ───────────────────────────────────────────

        /// Lấy node con theo tên key (tìm trong Object)
        /// JsonNode* name_node = JsonParser::Get(root, "name");
        static JsonNode *Get(JsonNode *node, std::string_view key);

        /// Lấy giá trị string trực tiếp (tiện hơn Get rồi đọc val_str)
        /// std::string_view name = JsonParser::GetString(root, "name"); // "Nam"
        static std::string_view GetString(JsonNode *node, std::string_view key);

        /// Lấy giá trị số nguyên
        /// int age = JsonParser::GetInt(root, "age"); // 20
        static int GetInt(JsonNode *node, std::string_view key);

        /// Lấy giá trị số thực
        /// double score = JsonParser::GetDouble(root, "score"); // 9.5
        static double GetDouble(JsonNode *node, std::string_view key);

        /// Lấy giá trị boolean
        /// bool active = JsonParser::GetBool(root, "active"); // true
        static bool GetBool(JsonNode *node, std::string_view key);

        /// Kiểm tra node có tồn tại và đúng kiểu không
        /// Dùng trước khi GetString/GetInt để tránh crash nếu JSON thiếu field
        static bool Has(JsonNode *node, std::string_view key);

    private:
        // ─── Hàm parse nội bộ (người dùng không gọi trực tiếp) ──────────────

        static void SkipWhitespace(std::string_view json, size_t &idx);
        static JsonNode *ParseValue(std::string_view json, size_t &idx);
        static JsonNode *ParseObject(std::string_view json, size_t &idx);
        static JsonNode *ParseArray(std::string_view json, size_t &idx);
        static JsonNode *ParseString(std::string_view json, size_t &idx);
        static JsonNode *ParsePrimitive(std::string_view json, size_t &idx);
    };

    // ─── Bộ SERIALIZE JSON (Cây DSLK -> Chuỗi) ──────────────────────────────

    class JsonBuilder
    {
    public:
        /// Chuyển cây JsonNode thành chuỗi JSON để gửi về Client
        /// Dùng khi: Cần trả về dữ liệu từ DB dưới dạng JSON
        ///
        /// JsonNode* root = ...; // Tự xây dựng cây từ dữ liệu DB
        /// std::string json = JsonBuilder::Serialize(root);
        /// Http::JSON(fd, 200, json);
        static std::string Serialize(const JsonNode *node);

        // ─── Hàm xây dựng cây JsonNode từ dữ liệu ────────────────────────────

        /// Tạo node kiểu Object (JSON object rỗng {})
        /// Dùng khi: Muốn xây dựng JSON response từ dữ liệu trong code
        static JsonNode *NewObject();

        /// Tạo node kiểu Array (JSON array rỗng [])
        static JsonNode *NewArray();

        /// Tạo node kiểu String
        /// Ví dụ: JsonBuilder::NewString("name", "Nam")
        static JsonNode *NewString(std::string_view key, std::string_view value);

        /// Tạo node kiểu Number (số nguyên)
        static JsonNode *NewInt(std::string_view key, int value);

        /// Tạo node kiểu Number (số thực)
        static JsonNode *NewDouble(std::string_view key, double value);

        /// Tạo node kiểu Boolean
        static JsonNode *NewBool(std::string_view key, bool value);

        /// Thêm node con vào Object hoặc Array
        static void Append(JsonNode *parent, JsonNode *child);
    };

} // namespace Http

#endif