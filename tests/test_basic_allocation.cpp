#include "test_framework.h"
#include "ghostmem/GhostMemoryManager.h"
#include "ghostmem/GhostAllocator.h"
#include <cstring>

// Test basic memory allocation
TEST(BasicAllocation) {
    void* ptr = GhostMemoryManager::Instance().AllocateGhost(4096);
    ASSERT_NOT_NULL(ptr);
}

// Test multiple allocations
TEST(MultipleAllocations) {
    void* ptr1 = GhostMemoryManager::Instance().AllocateGhost(4096);
    void* ptr2 = GhostMemoryManager::Instance().AllocateGhost(4096);
    void* ptr3 = GhostMemoryManager::Instance().AllocateGhost(8192);
    
    ASSERT_NOT_NULL(ptr1);
    ASSERT_NOT_NULL(ptr2);
    ASSERT_NOT_NULL(ptr3);
    ASSERT_NE(ptr1, ptr2);
    ASSERT_NE(ptr2, ptr3);
}

// Test writing and reading from allocated memory
TEST(WriteAndRead) {
    void* ptr = GhostMemoryManager::Instance().AllocateGhost(4096);
    ASSERT_NOT_NULL(ptr);
    
    // Write data (this will trigger page fault and commit)
    int* data = static_cast<int*>(ptr);
    data[0] = 42;
    data[1] = 100;
    data[2] = 999;
    
    // Read back
    ASSERT_EQ(data[0], 42);
    ASSERT_EQ(data[1], 100);
    ASSERT_EQ(data[2], 999);
}

// Test page alignment
TEST(PageAlignment) {
    void* ptr = GhostMemoryManager::Instance().AllocateGhost(100);
    ASSERT_NOT_NULL(ptr);
    
    // Pointer should be page-aligned
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    ASSERT_EQ(addr % 4096, 0);
}

// Test writing patterns across page boundary
TEST(CrossPageWrite) {
    void* ptr = GhostMemoryManager::Instance().AllocateGhost(8192); // 2 pages
    ASSERT_NOT_NULL(ptr);
    
    char* data = static_cast<char*>(ptr);
    
    // Write pattern across both pages
    for (int i = 0; i < 8192; i++) {
        data[i] = static_cast<char>(i % 256);
    }
    
    // Verify pattern
    for (int i = 0; i < 8192; i++) {
        ASSERT_EQ(data[i], static_cast<char>(i % 256));
    }
}
