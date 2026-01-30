#include "test_framework.h"
#include "ghostmem/GhostAllocator.h"
#include <vector>
#include <string>

// Test GhostAllocator with std::vector<int>
TEST(VectorAllocatorInt) {
    std::vector<int, GhostAllocator<int>> vec;
    
    for (int i = 0; i < 100; i++) {
        vec.push_back(i);
    }
    
    ASSERT_EQ(vec.size(), 100);
    
    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(vec[i], i);
    }
}

// Test GhostAllocator with large vector
TEST(LargeVectorAllocation) {
    std::vector<int, GhostAllocator<int>> vec;
    
    // Allocate enough to span multiple pages
    for (int i = 0; i < 5000; i++) {
        vec.push_back(i * 2);
    }
    
    ASSERT_EQ(vec.size(), 5000);
    
    // Verify random access
    ASSERT_EQ(vec[0], 0);
    ASSERT_EQ(vec[100], 200);
    ASSERT_EQ(vec[2500], 5000);
    ASSERT_EQ(vec[4999], 9998);
}

// Test GhostAllocator with std::string
TEST(StringAllocator) {
    using GhostString = std::basic_string<char, std::char_traits<char>, GhostAllocator<char>>;
    
    GhostString str;
    str = "Hello, GhostMem!";
    
    ASSERT_EQ(str.length(), 16);
    ASSERT_TRUE(str == "Hello, GhostMem!");
}

// Test multiple vectors causing page eviction
TEST(MultipleVectorEviction) {
    std::vector<std::vector<int, GhostAllocator<int>>*> vectors;
    
    // Create multiple vectors
    for (int v = 0; v < 3; v++) {
        auto* vec = new std::vector<int, GhostAllocator<int>>();
        
        // Fill each vector with data
        for (int i = 0; i < 2000; i++) {
            vec->push_back(v * 10000 + i);
        }
        
        vectors.push_back(vec);
    }
    
    // Verify all vectors retained their data
    for (size_t v = 0; v < vectors.size(); v++) {
        ASSERT_EQ((*vectors[v])[0], static_cast<int>(v * 10000));
        ASSERT_EQ((*vectors[v])[1000], static_cast<int>(v * 10000 + 1000));
    }
    
    // Cleanup
    for (auto* vec : vectors) {
        delete vec;
    }
}

// Test vector resize operations
TEST(VectorResize) {
    std::vector<int, GhostAllocator<int>> vec;
    
    vec.resize(1000, 42);
    ASSERT_EQ(vec.size(), 1000);
    ASSERT_EQ(vec[500], 42);
    
    vec.resize(2000, 99);
    ASSERT_EQ(vec.size(), 2000);
    ASSERT_EQ(vec[500], 42);
    ASSERT_EQ(vec[1500], 99);
}
