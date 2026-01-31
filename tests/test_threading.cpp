#include "test_framework.h"
#include "ghostmem/GhostMemoryManager.h"
#include "ghostmem/GhostAllocator.h"
#include <thread>
#include <vector>
#include <atomic>

// Test concurrent allocations from multiple threads
TEST(ConcurrentAllocations) {
    const int NUM_THREADS = 4;
    const int ALLOCS_PER_THREAD = 10;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&success_count]() {
            for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
                void* ptr = GhostMemoryManager::Instance().AllocateGhost(4096);
                if (ptr != nullptr) {
                    // Write to trigger page fault
                    int* data = static_cast<int*>(ptr);
                    data[0] = 42;
                    if (data[0] == 42) {
                        success_count++;
                    }
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    ASSERT_EQ(success_count.load(), NUM_THREADS * ALLOCS_PER_THREAD);
}

// Test concurrent reads/writes to same allocation
TEST(ConcurrentReadWrite) {
    void* ptr = GhostMemoryManager::Instance().AllocateGhost(8192); // 2 pages
    ASSERT_NOT_NULL(ptr);
    
    int* data = static_cast<int*>(ptr);
    const int NUM_ELEMENTS = 2048; // 8192 / 4
    
    // Initialize data
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        data[i] = i;
    }
    
    const int NUM_THREADS = 4;
    std::vector<std::thread> threads;
    std::atomic<bool> all_correct{true};
    
    // Multiple threads reading the same data
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([data, &all_correct, NUM_ELEMENTS]() {
            for (int i = 0; i < NUM_ELEMENTS; i++) {
                if (data[i] != i) {
                    all_correct = false;
                    break;
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    ASSERT_TRUE(all_correct.load());
}

// Test concurrent use of GhostAllocator with vectors
TEST(ConcurrentVectorAllocations) {
    const int NUM_THREADS = 4;
    const int ELEMENTS_PER_VECTOR = 500;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([t, &success_count]() {
            std::vector<int, GhostAllocator<int>> vec;
            
            // Fill vector with thread-specific values
            for (int i = 0; i < ELEMENTS_PER_VECTOR; i++) {
                vec.push_back(t * 10000 + i);
            }
            
            // Verify all values
            bool correct = true;
            for (int i = 0; i < ELEMENTS_PER_VECTOR; i++) {
                if (vec[i] != t * 10000 + i) {
                    correct = false;
                    break;
                }
            }
            
            if (correct) {
                success_count++;
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    ASSERT_EQ(success_count.load(), NUM_THREADS);
}

// Test that page eviction works correctly with multiple threads
TEST(ConcurrentPageEviction) {
    const int NUM_THREADS = 3;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([t, &success_count]() {
            std::vector<void*> pages;
            
            // Allocate more than MAX_PHYSICAL_PAGES to force eviction
            for (size_t i = 0; i < MAX_PHYSICAL_PAGES + 2; i++) {
                void* ptr = GhostMemoryManager::Instance().AllocateGhost(4096);
                if (ptr) {
                    int* data = static_cast<int*>(ptr);
                    data[0] = static_cast<int>(t * 1000 + i);
                    pages.push_back(ptr);
                }
            }
            
            // Verify all pages still have correct data
            bool correct = true;
            for (size_t i = 0; i < pages.size(); i++) {
                int* data = static_cast<int*>(pages[i]);
                if (data[0] != static_cast<int>(t * 1000 + i)) {
                    correct = false;
                    break;
                }
            }
            
            if (correct) {
                success_count++;
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    ASSERT_EQ(success_count.load(), NUM_THREADS);
}

// Test concurrent access pattern that triggers compression/decompression
TEST(ConcurrentCompressionCycle) {
    const int NUM_THREADS = 2;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([t, &success_count]() {
            void* ptr = GhostMemoryManager::Instance().AllocateGhost(4096);
            if (ptr) {
                int* data = static_cast<int*>(ptr);
                
                // Write pattern
                for (int i = 0; i < 1024; i++) {
                    data[i] = t * 10000 + i;
                }
                
                // Force potential eviction by allocating more pages
                std::vector<void*> dummy;
                for (size_t i = 0; i < MAX_PHYSICAL_PAGES; i++) {
                    void* d = GhostMemoryManager::Instance().AllocateGhost(4096);
                    int* dd = static_cast<int*>(d);
                    dd[0] = 999;
                    dummy.push_back(d);
                }
                
                // Verify original data (should decompress if needed)
                bool correct = true;
                for (int i = 0; i < 1024; i++) {
                    if (data[i] != t * 10000 + i) {
                        correct = false;
                        break;
                    }
                }
                
                if (correct) {
                    success_count++;
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    ASSERT_EQ(success_count.load(), NUM_THREADS);
}
