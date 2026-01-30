#include "test_framework.h"
#include "ghostmem/GhostMemoryManager.h"

// Test LRU eviction policy
TEST(LRUEviction) {
    std::vector<void*> pages;
    
    // Allocate exactly MAX_PHYSICAL_PAGES pages
    for (size_t i = 0; i < MAX_PHYSICAL_PAGES; i++) {
        void* ptr = GhostMemoryManager::Instance().AllocateGhost(4096);
        ASSERT_NOT_NULL(ptr);
        
        int* data = static_cast<int*>(ptr);
        data[0] = static_cast<int>(i);
        
        pages.push_back(ptr);
    }
    
    // Access first page to make it most recently used
    int* first = static_cast<int*>(pages[0]);
    int val = first[0];
    ASSERT_EQ(val, 0);
    
    // Allocate one more page - should evict the second page (now LRU)
    void* new_page = GhostMemoryManager::Instance().AllocateGhost(4096);
    ASSERT_NOT_NULL(new_page);
    int* new_data = static_cast<int*>(new_page);
    new_data[0] = 999;
    
    // First page should still be accessible (was recently used)
    ASSERT_EQ(first[0], 0);
}

// Test repeated access updates LRU
TEST(RepeatedAccessUpdatesLRU) {
    std::vector<void*> pages;
    
    // Allocate MAX_PHYSICAL_PAGES
    for (size_t i = 0; i < MAX_PHYSICAL_PAGES; i++) {
        void* ptr = GhostMemoryManager::Instance().AllocateGhost(4096);
        int* data = static_cast<int*>(ptr);
        data[0] = static_cast<int>(i + 100);
        pages.push_back(ptr);
    }
    
    // Repeatedly access the last page
    int* last = static_cast<int*>(pages[MAX_PHYSICAL_PAGES - 1]);
    for (int i = 0; i < 5; i++) {
        int val = last[0];
        ASSERT_EQ(val, static_cast<int>(MAX_PHYSICAL_PAGES - 1 + 100));
    }
    
    // Allocate new page - last page should still be in RAM
    void* new_page = GhostMemoryManager::Instance().AllocateGhost(4096);
    int* new_data = static_cast<int*>(new_page);
    new_data[0] = 555;
    
    // Last page should still be correct
    ASSERT_EQ(last[0], static_cast<int>(MAX_PHYSICAL_PAGES - 1 + 100));
}

// Test that evicted pages can be restored
TEST(EvictedPagesRestored) {
    std::vector<void*> pages;
    
    // Allocate more than MAX_PHYSICAL_PAGES
    for (size_t i = 0; i < MAX_PHYSICAL_PAGES + 3; i++) {
        void* ptr = GhostMemoryManager::Instance().AllocateGhost(4096);
        ASSERT_NOT_NULL(ptr);
        
        int* data = static_cast<int*>(ptr);
        data[0] = static_cast<int>(i * 10);
        data[1] = static_cast<int>(i * 10 + 1);
        
        pages.push_back(ptr);
    }
    
    // Early pages should have been evicted
    // Access them - they should be restored correctly
    for (size_t i = 0; i < 3; i++) {
        int* data = static_cast<int*>(pages[i]);
        ASSERT_EQ(data[0], static_cast<int>(i * 10));
        ASSERT_EQ(data[1], static_cast<int>(i * 10 + 1));
    }
}
