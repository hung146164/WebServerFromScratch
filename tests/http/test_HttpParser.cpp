#include <iostream>
#include <cassert>
#include <string>
#include <chrono>
#include <thread>
#include "common/LRUCustom.h"
#include "server/http/HttpUtils.h"
#include "server/http/Router.h"

// Defined to satisfy linker for Router.cpp
thread_local int current_worker_id = -1;

// Mock handler for testing Router
void mock_handler_exact(int fd, const HttpRequest &req) {
    (void)fd;
    (void)req;
}

void mock_handler_wildcard(int fd, const HttpRequest &req) {
    (void)fd;
    (void)req;
}

// 1. Test URL Decoding
void TestUrlDecode() {
    std::cout << "[*] Running TestUrlDecode...\n";
    
    // Normal decoding
    assert(Http::UrlDecode("hello%20world") == "hello world");
    assert(Http::UrlDecode("hello+world") == "hello world");
    
    // Complex characters
    assert(Http::UrlDecode("%E2%9C%94") == "✔"); // UTF-8 check
    assert(Http::UrlDecode("abc%2Fdef%3Fghi%3Djkl") == "abc/def?ghi=jkl");
    
    // Empty and no-op strings
    assert(Http::UrlDecode("") == "");
    assert(Http::UrlDecode("no_change") == "no_change");
    
    std::cout << "[+] TestUrlDecode PASSED!\n";
}

// 2. Test RC4 Cryptography
void TestRC4() {
    std::cout << "[*] Running TestRC4...\n";
    
    std::string key = "VDTSecretKey";
    std::string original = "The quick brown fox jumps over the lazy dog! 12345";
    std::string data = original;
    
    // Encrypt
    Http::RC4(key, data);
    assert(data != original); // Must be encrypted
    
    // Decrypt (RC4 is symmetric, running again decrypts)
    Http::RC4(key, data);
    assert(data == original); // Must be decrypted back
    
    std::cout << "[+] TestRC4 PASSED!\n";
}

// Structs to satisfy Node<T>::Reset() compiler constraints
struct MockString {
    std::string s;
    void Reset() {
        s.clear();
    }
};

struct MockInt {
    int i = 0;
    void Reset() {
        i = 0;
    }
};

// 3. Test LRU Cache
void TestLRUCache() {
    std::cout << "[*] Running TestLRUCache...\n";
    
    // Capacity 3
    LRUCustom<MockString> cache(3);
    
    assert(cache.OldestKey() == -1);
    
    cache.Put(10, MockString{"ten"});
    cache.Put(20, MockString{"twenty"});
    cache.Put(30, MockString{"thirty"});
    
    assert(cache.Full() == true);
    
    // The oldest key must be 10 (first one put in)
    assert(cache.OldestKey() == 10);
    
    // Access key 10 to make it recent
    MockString *val = cache.Get(10);
    assert(val != nullptr && val->s == "ten");
    
    // Now the oldest key should be 20
    assert(cache.OldestKey() == 20);
    
    // Put key 40, which should evict key 20 (oldest unused)
    cache.Put(40, MockString{"forty"});
    
    assert(cache.Get(20) == nullptr); // Evicted!
    assert(cache.Get(10) != nullptr); // Kept!
    assert(cache.Get(30) != nullptr); // Kept!
    assert(cache.Get(40) != nullptr); // Kept!
    
    // Test removal
    assert(cache.Remove(30) == true);
    assert(cache.Get(30) == nullptr);
    assert(cache.Full() == false);
    
    std::cout << "[+] TestLRUCache PASSED!\n";
}

// 4. Test Keep-Alive Timeout tracking
void TestLRUTimeout() {
    std::cout << "[*] Running TestLRUTimeout...\n";
    
    LRUCustom<MockInt> cache(5);
    cache.Put(100, MockInt{1});
    
    time_t t1 = cache.GetLastActiveTime(100);
    assert(t1 > 0);
    
    // Sleep 1 second to verify time difference
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    cache.MakeRecent(100);
    time_t t2 = cache.GetLastActiveTime(100);
    assert(t2 > t1);
    
    std::cout << "[+] TestLRUTimeout PASSED!\n";
}

int main() {
    std::cout << "=========================================================\n";
    
    TestUrlDecode();
    TestRC4();
    TestLRUCache();
    TestLRUTimeout();
    
    std::cout << "=========================================================\n";
    std::cout << "      ALL HTTP SERVER LOGIC UNIT TESTS PASSED!           \n";
    std::cout << "=========================================================\n";
    return 0;
}
