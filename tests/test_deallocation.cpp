#include "test_framework.h"
#include "ghostmem/GhostMemoryManager.h"
#include "ghostmem/GhostAllocator.h"
#include <vector>
#include <cstring>

// Test basic deallocation doesn't crash
TEST(BasicDeallocation) {
    void* ptr = GhostMemoryManager::Instance().AllocateGhost(4096);
    ASSERT_NOT_NULL(ptr);
    
    // Write some data to trigger page commit
    int* data = static_cast<int*>(ptr);
    data[0] = 42;
    
    // Deallocate should not crash
    GhostMemoryManager::Instance().DeallocateGhost(ptr, 4096);
}

// Test deallocating nullptr is safe (standard behavior)
TEST(DeallocateNullptr) {
    GhostMemoryManager::Instance().DeallocateGhost(nullptr, 4096);
    // Should not crash
}

// Test multiple allocations and deallocations
TEST(MultipleAllocDealloc) {
    void* ptr1 = GhostMemoryManager::Instance().AllocateGhost(4096);
    void* ptr2 = GhostMemoryManager::Instance().AllocateGhost(4096);
    void* ptr3 = GhostMemoryManager::Instance().AllocateGhost(8192);
    
    ASSERT_NOT_NULL(ptr1);
    ASSERT_NOT_NULL(ptr2);
    ASSERT_NOT_NULL(ptr3);
    
    // Write to trigger commits
    static_cast<int*>(ptr1)[0] = 1;
    static_cast<int*>(ptr2)[0] = 2;
    static_cast<int*>(ptr3)[0] = 3;
    
    // Deallocate in different order
    GhostMemoryManager::Instance().DeallocateGhost(ptr2, 4096);
    GhostMemoryManager::Instance().DeallocateGhost(ptr1, 4096);
    GhostMemoryManager::Instance().DeallocateGhost(ptr3, 8192);
}

// Test multi-page allocation deallocation
TEST(MultiPageDeallocation) {
    // Allocate 3 pages worth of memory
    size_t size = 3 * 4096;
    void* ptr = GhostMemoryManager::Instance().AllocateGhost(size);
    ASSERT_NOT_NULL(ptr);
    
    // Write to all pages to trigger commits
    int* data = static_cast<int*>(ptr);
    data[0] = 1;           // First page
    data[1024] = 2;        // Second page
    data[2048] = 3;        // Third page
    
    // Deallocate should clean up all pages
    GhostMemoryManager::Instance().DeallocateGhost(ptr, size);
}

// Test vector with GhostAllocator destructor behavior
TEST(VectorDestructor) {
    {
        std::vector<int, GhostAllocator<int>> vec;
        
        // Fill vector with data
        for (int i = 0; i < 1000; i++) {
            vec.push_back(i);
        }
        
        ASSERT_EQ(vec.size(), 1000);
        ASSERT_EQ(vec[500], 500);
        
        // Vector goes out of scope here - destructor should call deallocate
    }
    // If we get here without crashing, deallocation worked
}

// Test allocate, use, deallocate, then allocate again
TEST(AllocDeallocReuse) {
    // First allocation
    void* ptr1 = GhostMemoryManager::Instance().AllocateGhost(4096);
    ASSERT_NOT_NULL(ptr1);
    static_cast<int*>(ptr1)[0] = 100;
    GhostMemoryManager::Instance().DeallocateGhost(ptr1, 4096);
    
    // Second allocation - might reuse address space
    void* ptr2 = GhostMemoryManager::Instance().AllocateGhost(4096);
    ASSERT_NOT_NULL(ptr2);
    static_cast<int*>(ptr2)[0] = 200;
    ASSERT_EQ(static_cast<int*>(ptr2)[0], 200);
    GhostMemoryManager::Instance().DeallocateGhost(ptr2, 4096);
    
    // Third allocation
    void* ptr3 = GhostMemoryManager::Instance().AllocateGhost(4096);
    ASSERT_NOT_NULL(ptr3);
    GhostMemoryManager::Instance().DeallocateGhost(ptr3, 4096);
}

// Test deallocating evicted (compressed) page
TEST(DeallocateEvictedPage) {
    // Allocate many pages to trigger eviction (MAX_PHYSICAL_PAGES = 5)
    std::vector<void*> ptrs;
    for (int i = 0; i < 10; i++) {
        void* ptr = GhostMemoryManager::Instance().AllocateGhost(4096);
        ASSERT_NOT_NULL(ptr);
        // Write to trigger page commit
        static_cast<int*>(ptr)[0] = i;
        ptrs.push_back(ptr);
    }
    
    // Some pages should now be evicted/compressed
    // Deallocate them anyway - should clean up compressed data
    for (size_t i = 0; i < ptrs.size(); i++) {
        GhostMemoryManager::Instance().DeallocateGhost(ptrs[i], 4096);
    }
}

// Test string allocations (good compression candidates)
TEST(StringDeallocation) {
    using GhostString = std::basic_string<char, std::char_traits<char>, GhostAllocator<char>>;
    
    {
        GhostString str1(1000, 'A');
        GhostString str2(2000, 'B');
        GhostString str3(500, 'C');
        
        ASSERT_EQ(str1.length(), 1000);
        ASSERT_EQ(str2.length(), 2000);
        ASSERT_EQ(str3.length(), 500);
        
        ASSERT_EQ(str1[0], 'A');
        ASSERT_EQ(str2[0], 'B');
        ASSERT_EQ(str3[0], 'C');
        
        // Strings go out of scope - destructors should deallocate
    }
}

// Test mixed operations with compression
TEST(MixedOpsWithCompression) {
    std::vector<void*> ptrs;
    
    // Allocate 8 pages
    for (int i = 0; i < 8; i++) {
        void* ptr = GhostMemoryManager::Instance().AllocateGhost(4096);
        ASSERT_NOT_NULL(ptr);
        static_cast<int*>(ptr)[0] = i;
        ptrs.push_back(ptr);
    }
    
    // Deallocate some pages (should be evicted)
    GhostMemoryManager::Instance().DeallocateGhost(ptrs[0], 4096);
    GhostMemoryManager::Instance().DeallocateGhost(ptrs[1], 4096);
    
    // Allocate more pages (triggers eviction)
    for (int i = 8; i < 12; i++) {
        void* ptr = GhostMemoryManager::Instance().AllocateGhost(4096);
        ASSERT_NOT_NULL(ptr);
        static_cast<int*>(ptr)[0] = i;
        ptrs.push_back(ptr);
    }
    
    // Access a middle page (should restore from compression)
    int val = static_cast<int*>(ptrs[4])[0];
    ASSERT_EQ(val, 4);
    
    // Deallocate remaining pages
    for (size_t i = 2; i < ptrs.size(); i++) {
        GhostMemoryManager::Instance().DeallocateGhost(ptrs[i], 4096);
    }
}

// Test double-free protection (should warn but not crash)
TEST(DoubleFreeProtection) {
    void* ptr = GhostMemoryManager::Instance().AllocateGhost(4096);
    ASSERT_NOT_NULL(ptr);
    static_cast<int*>(ptr)[0] = 42;
    
    // First deallocation
    GhostMemoryManager::Instance().DeallocateGhost(ptr, 4096);
    
    // Second deallocation - should print warning but not crash
    GhostMemoryManager::Instance().DeallocateGhost(ptr, 4096);
}

// Test large vector operations with deallocation
TEST(LargeVectorOperations) {
    using GhostVec = std::vector<int, GhostAllocator<int>>;
    
    {
        GhostVec vec;
        
        // Push many elements to trigger multiple page allocations
        for (int i = 0; i < 5000; i++) {
            vec.push_back(i);
        }
        
        ASSERT_EQ(vec.size(), 5000);
        
        // Verify data integrity
        for (int i = 0; i < 5000; i++) {
            ASSERT_EQ(vec[i], i);
        }
        
        // Clear should trigger deallocation of internal buffer
        vec.clear();
        vec.shrink_to_fit();
        
        // Reuse the vector
        for (int i = 0; i < 1000; i++) {
            vec.push_back(i * 2);
        }
        
        ASSERT_EQ(vec.size(), 1000);
        ASSERT_EQ(vec[500], 1000);
        
        // Vector destructor will deallocate remaining memory
    }
}

// Test with small allocations (sub-page)
TEST(SmallAllocations) {
    void* ptr1 = GhostMemoryManager::Instance().AllocateGhost(100);
    void* ptr2 = GhostMemoryManager::Instance().AllocateGhost(200);
    void* ptr3 = GhostMemoryManager::Instance().AllocateGhost(300);
    
    ASSERT_NOT_NULL(ptr1);
    ASSERT_NOT_NULL(ptr2);
    ASSERT_NOT_NULL(ptr3);
    
    // Write to trigger commits
    memset(ptr1, 0xAA, 100);
    memset(ptr2, 0xBB, 200);
    memset(ptr3, 0xCC, 300);
    
    // Deallocate
    GhostMemoryManager::Instance().DeallocateGhost(ptr1, 100);
    GhostMemoryManager::Instance().DeallocateGhost(ptr2, 200);
    GhostMemoryManager::Instance().DeallocateGhost(ptr3, 300);
}
