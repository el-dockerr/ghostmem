#include "test_framework.h"
#include "ghostmem/GhostMemoryManager.h"
#include "ghostmem/GhostAllocator.h"
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <mutex>

// Test concurrent allocations from multiple threads
TEST(ConcurrentAllocations) {
    const int NUM_THREADS = 4;
    const int ALLOCS_PER_THREAD = 10;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&success_count, ALLOCS_PER_THREAD]() {
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
        threads.emplace_back([t, &success_count, ELEMENTS_PER_VECTOR]() {
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

// Test concurrent manipulation of random data across multiple threads
TEST(ConcurrentRandomDataManipulation) {
#ifdef _WIN32
    const int NUM_THREADS = 3;  // Reduced for Windows to avoid timeout issues
    const int DATA_SIZE = 512;  // Reduced data size for Windows
#else
    const int NUM_THREADS = 6;
    const int DATA_SIZE = 1024;  // 1024 integers per thread
#endif
    std::vector<std::thread> threads;
    std::mutex cout_mutex;
    
    // Structure to hold thread data and verification info
    struct ThreadData {
        void* ghost_ptr;
        std::vector<int> expected_values;
        int thread_id;
    };
    
    std::vector<ThreadData> thread_data(NUM_THREADS);
    
    // Phase 1: Each thread allocates memory and writes random data
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([t, &thread_data, &cout_mutex, DATA_SIZE]() {
            // Each thread gets its own random seed for reproducibility
            std::mt19937 rng(12345 + t);
            std::uniform_int_distribution<int> dist(-100000, 100000);
            
            // Allocate ghost memory for this thread
            void* ptr = GhostMemoryManager::Instance().AllocateGhost(DATA_SIZE * sizeof(int));
            if (ptr == nullptr) {
                return;
            }
            
            int* data = static_cast<int*>(ptr);
            thread_data[t].ghost_ptr = ptr;
            thread_data[t].thread_id = t;
            thread_data[t].expected_values.reserve(DATA_SIZE);
            
            // Generate and write random numbers
            for (int i = 0; i < DATA_SIZE; i++) {
                int random_value = dist(rng);
                data[i] = random_value;
                thread_data[t].expected_values.push_back(random_value);
            }
            
            // Simulate some work and additional writes
            for (int i = 0; i < DATA_SIZE; i += 10) {
                data[i] = data[i] * 2;  // Modify every 10th element
                thread_data[t].expected_values[i] = thread_data[t].expected_values[i] * 2;
            }
        });
    }
    
    // Wait for all threads to finish phase 1
    for (auto& thread : threads) {
        thread.join();
    }
    threads.clear();
    
    // Phase 2: Create pressure on memory system
    std::vector<void*> pressure_allocations;
    for (int i = 0; i < MAX_PHYSICAL_PAGES + 5; i++) {
        void* ptr = GhostMemoryManager::Instance().AllocateGhost(4096);
        if (ptr) {
            int* data = static_cast<int*>(ptr);
            data[0] = i;  // Write to ensure it's backed
            pressure_allocations.push_back(ptr);
        }
    }
    
    // Phase 3: Verify data in parallel across multiple threads
    std::atomic<int> verification_success{0};
    std::atomic<int> verification_failures{0};
    
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([t, &thread_data, &verification_success, &verification_failures, &cout_mutex, DATA_SIZE]() {
            if (thread_data[t].ghost_ptr == nullptr) {
                return;
            }
            
            int* data = static_cast<int*>(thread_data[t].ghost_ptr);
            bool all_correct = true;
            int mismatch_count = 0;
            
            // Verify all values match what we stored
            for (int i = 0; i < DATA_SIZE; i++) {
                if (data[i] != thread_data[t].expected_values[i]) {
                    all_correct = false;
                    mismatch_count++;
                }
            }
            
            if (all_correct) {
                verification_success++;
            } else {
                verification_failures++;
                std::lock_guard<std::mutex> lock(cout_mutex);
                // Only print in debug if needed
            }
        });
    }
    
    // Wait for verification to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    ASSERT_EQ(verification_success.load(), NUM_THREADS);
    ASSERT_EQ(verification_failures.load(), 0);
}
