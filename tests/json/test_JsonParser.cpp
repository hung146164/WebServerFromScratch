#include <gtest/gtest.h>
#include "server/http/JsonParser.h"

// Test parse object hợp lệ
TEST(JsonParserTest, ParseValidObject)
{
    std::string_view json = "{\"name\":\"Nam\",\"age\":20,\"score\":9.5,\"active\":true,\"skills\":null}";
    Http::JsonNode* root = Http::JsonParser::Parse(json);
    
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->type, Http::JsonType::OBJECT);
    
    // Đọc trường string
    EXPECT_TRUE(Http::JsonParser::Has(root, "name"));
    EXPECT_EQ(Http::JsonParser::GetString(root, "name"), "Nam");
    
    // Đọc trường number (int & double)
    EXPECT_TRUE(Http::JsonParser::Has(root, "age"));
    EXPECT_EQ(Http::JsonParser::GetInt(root, "age"), 20);
    EXPECT_TRUE(Http::JsonParser::Has(root, "score"));
    EXPECT_DOUBLE_EQ(Http::JsonParser::GetDouble(root, "score"), 9.5);
    
    // Đọc trường boolean
    EXPECT_TRUE(Http::JsonParser::Has(root, "active"));
    EXPECT_TRUE(Http::JsonParser::GetBool(root, "active"));
    
    // Đọc trường null
    Http::JsonNode* null_node = Http::JsonParser::Get(root, "skills");
    ASSERT_NE(null_node, nullptr);
    EXPECT_EQ(null_node->type, Http::JsonType::NUL);
    
    // Dọn dẹp
    Http::JsonParser::Free(root);
}

// Test parse array hợp lệ
TEST(JsonParserTest, ParseValidArray)
{
    std::string_view json = "[10, 20, 30]";
    Http::JsonNode* root = Http::JsonParser::Parse(json);
    
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->type, Http::JsonType::ARRAY);
    
    Http::JsonNode* curr = root->child;
    int count = 0;
    int expected_vals[] = {10, 20, 30};
    
    while (curr) {
        EXPECT_EQ(curr->type, Http::JsonType::NUMBER);
        // Do mảng không có key nên key phải rỗng
        EXPECT_TRUE(curr->key.empty());
        
        // Vì node là Number và không nằm trong object (ko có key),
        // ta có thể kiểm tra giá trị thô val_str
        int val = std::atoi(std::string(curr->val_str).c_str());
        EXPECT_EQ(val, expected_vals[count]);
        
        count++;
        curr = curr->next;
    }
    EXPECT_EQ(count, 3);
    
    Http::JsonParser::Free(root);
}

// Test parse chuỗi trống (bộ parser sẽ trả về nullptr hợp lý)
TEST(JsonParserTest, ParseEmptyString)
{
    std::string_view json = "";
    Http::JsonNode* root = Http::JsonParser::Parse(json);
    EXPECT_EQ(root, nullptr);
}

// Test parse chỉ có khoảng trắng
TEST(JsonParserTest, ParseOnlyWhitespace)
{
    std::string_view json = "   \n\t  ";
    Http::JsonNode* root = Http::JsonParser::Parse(json);
    EXPECT_EQ(root, nullptr);
}
