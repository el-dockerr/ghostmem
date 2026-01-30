#include "test_framework.h"
#include "ghostmem/GhostMemoryManager.h"
#include <cstring>

// Test that data survives compression/decompression cycle
TEST(CompressionCycle) {
    // Allocate MAX_PHYSICAL_PAGES + 1 pages to force eviction
    std::vector<void*> pages;
    
    for (size_t i = 0; i <= MAX_PHYSICAL_PAGES; i++) {
        void* ptr = GhostMemoryManager::Instance().AllocateGhost(4096);
        ASSERT_NOT_NULL(ptr);
        
        // Write unique data to each page
        int* data = static_cast<int*>(ptr);
        data[0] = static_cast<int>(i * 1000);
        
        pages.push_back(ptr);
    }
    
    // The first page should have been evicted and compressed
    // Now access it again - should decompress correctly
    int* first_page = static_cast<int*>(pages[0]);
    ASSERT_EQ(first_page[0], 0);
}

// Test compression of highly compressible data
TEST(HighlyCompressibleData) {
    void* ptr = GhostMemoryManager::Instance().AllocateGhost(4096);
    ASSERT_NOT_NULL(ptr);
    
    // Fill with repeating pattern (highly compressible)
    int* data = static_cast<int*>(ptr);
    for (int i = 0; i < 1024; i++) {
        data[i] = 0xAAAAAAAA;
    }
    
    // Verify data
    for (int i = 0; i < 1024; i++) {
        ASSERT_EQ(data[i], 0xAAAAAAAA);
    }
    
    // Force eviction by allocating more pages
    std::vector<void*> dummy_pages;
    for (size_t i = 0; i < MAX_PHYSICAL_PAGES; i++) {
        void* dummy = GhostMemoryManager::Instance().AllocateGhost(4096);
        int* dummy_data = static_cast<int*>(dummy);
        dummy_data[0] = static_cast<int>(i);
        dummy_pages.push_back(dummy);
    }
    
    // Access original page - should decompress correctly
    for (int i = 0; i < 1024; i++) {
        ASSERT_EQ(data[i], 0xAAAAAAAA);
    }
}

// Test compression of text-like data
TEST(TextDataCompression) {
    void* ptr = GhostMemoryManager::Instance().AllocateGhost(4096);
    ASSERT_NOT_NULL(ptr);
    
    // Fill with text-like repeating pattern
    char* data = static_cast<char*>(ptr);
    const char* pattern = "ABCDEFGH";
    for (int i = 0; i < 4096; i++) {
        data[i] = pattern[i % 8];
    }
    
    // Force eviction
    std::vector<void*> dummy_pages;
    for (size_t i = 0; i < MAX_PHYSICAL_PAGES; i++) {
        void* dummy = GhostMemoryManager::Instance().AllocateGhost(4096);
        char* dummy_data = static_cast<char*>(dummy);
        dummy_data[0] = 'X';
        dummy_pages.push_back(dummy);
    }
    
    // Verify original data after decompression
    for (int i = 0; i < 4096; i++) {
        ASSERT_EQ(data[i], pattern[i % 8]);
    }
}
