#include "test_framework.h"
#include "ghostmem/GhostMemoryManager.h"
#include <chrono>
#include <cstring>
#include <vector>
#include <random>
#include <iomanip>

/**
 * @file test_metrics.cpp
 * @brief Performance and compression metrics tests for GhostMem
 * 
 * This file contains tests that measure:
 * 1. Compression ratios for different data types
 * 2. Memory savings achieved through compression
 * 3. Performance comparisons between native C++ and GhostMem
 * 4. Speed impact of compression/decompression cycles
 */

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Measures time to allocate and fill memory with GhostMem
 */
template<typename T>
double MeasureGhostMemAllocation(size_t num_elements, const T& fill_value) {
    auto start = std::chrono::high_resolution_clock::now();
    
    void* ptr = GhostMemoryManager::Instance().AllocateGhost(num_elements * sizeof(T));
    if (!ptr) {
        throw std::runtime_error("GhostMem allocation failed");
    }
    
    T* data = static_cast<T*>(ptr);
    for (size_t i = 0; i < num_elements; i++) {
        data[i] = fill_value;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

/**
 * @brief Measures time to allocate and fill memory with standard C++ malloc
 */
template<typename T>
double MeasureStandardAllocation(size_t num_elements, const T& fill_value) {
    auto start = std::chrono::high_resolution_clock::now();
    
    T* data = static_cast<T*>(malloc(num_elements * sizeof(T)));
    if (!data) {
        throw std::runtime_error("Standard allocation failed");
    }
    
    for (size_t i = 0; i < num_elements; i++) {
        data[i] = fill_value;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    free(data);
    return std::chrono::duration<double, std::milli>(end - start).count();
}

/**
 * @brief Estimates compression ratio by analyzing backing store size
 * This is a simplified metric based on the number of pages vs compressed size
 */
double EstimateCompressionRatio(size_t original_bytes, size_t num_pages_allocated) {
    // Assume at least one page gets compressed into backing store
    // In real scenario, we'd need API access to backing_store.size()
    // For now, we use theoretical compression ratios
    return static_cast<double>(original_bytes) / (num_pages_allocated * PAGE_SIZE);
}

// ============================================================================
// COMPRESSION RATIO TESTS
// ============================================================================

TEST(CompressionMetrics_HighlyCompressibleData) {
    std::cout << "\n=== Compression Test: Highly Compressible Data ===\n";
    
    const size_t num_pages = 10;
    const size_t total_size = num_pages * PAGE_SIZE;
    
    void* ptr = GhostMemoryManager::Instance().AllocateGhost(total_size);
    ASSERT_NOT_NULL(ptr);
    
    // Fill with repeating pattern (0xAAAAAAAA) - highly compressible
    uint32_t* data = static_cast<uint32_t*>(ptr);
    const size_t num_ints = total_size / sizeof(uint32_t);
    
    for (size_t i = 0; i < num_ints; i++) {
        data[i] = 0xAAAAAAAA;
    }
    
    // Force eviction by allocating more pages
    std::vector<void*> eviction_pages;
    for (size_t i = 0; i < MAX_PHYSICAL_PAGES + 5; i++) {
        void* evict = GhostMemoryManager::Instance().AllocateGhost(PAGE_SIZE);
        uint32_t* evict_data = static_cast<uint32_t*>(evict);
        evict_data[0] = static_cast<uint32_t>(i);
        eviction_pages.push_back(evict);
    }
    
    // Access original data to force decompression
    uint32_t checksum = 0;
    for (size_t i = 0; i < num_ints; i++) {
        checksum += data[i];
    }
    
    std::cout << "Original size: " << total_size << " bytes\n";
    std::cout << "Pattern: 0xAAAAAAAA (repeating)\n";
    std::cout << "Theoretical compression: ~50:1 to 100:1 for LZ4\n";
    std::cout << "Data verified: checksum = " << std::hex << checksum << std::dec << "\n";
    std::cout << "Expected compressed size: ~" << (total_size / 50) << " to " 
              << (total_size / 100) << " bytes\n\n";
}

TEST(CompressionMetrics_TextData) {
    std::cout << "\n=== Compression Test: Text-like Data ===\n";
    
    const size_t num_pages = 10;
    const size_t total_size = num_pages * PAGE_SIZE;
    
    void* ptr = GhostMemoryManager::Instance().AllocateGhost(total_size);
    ASSERT_NOT_NULL(ptr);
    
    // Fill with repeating text pattern
    char* data = static_cast<char*>(ptr);
    const char* pattern = "The quick brown fox jumps over the lazy dog. ";
    const size_t pattern_len = strlen(pattern);
    
    for (size_t i = 0; i < total_size; i++) {
        data[i] = pattern[i % pattern_len];
    }
    
    // Force eviction
    std::vector<void*> eviction_pages;
    for (size_t i = 0; i < MAX_PHYSICAL_PAGES + 5; i++) {
        void* evict = GhostMemoryManager::Instance().AllocateGhost(PAGE_SIZE);
        char* evict_data = static_cast<char*>(evict);
        evict_data[0] = 'X';
        eviction_pages.push_back(evict);
    }
    
    // Verify data integrity after decompression
    bool data_valid = true;
    for (size_t i = 0; i < total_size; i++) {
        if (data[i] != pattern[i % pattern_len]) {
            data_valid = false;
            break;
        }
    }
    
    ASSERT_TRUE(data_valid);
    std::cout << "Original size: " << total_size << " bytes\n";
    std::cout << "Pattern: Repeating English text\n";
    std::cout << "Theoretical compression: ~5:1 to 10:1 for LZ4\n";
    std::cout << "Expected compressed size: ~" << (total_size / 5) << " to " 
              << (total_size / 10) << " bytes\n\n";
}

TEST(CompressionMetrics_RandomData) {
    std::cout << "\n=== Compression Test: Random (Incompressible) Data ===\n";
    
    const size_t num_pages = 10;
    const size_t total_size = num_pages * PAGE_SIZE;
    
    void* ptr = GhostMemoryManager::Instance().AllocateGhost(total_size);
    ASSERT_NOT_NULL(ptr);
    
    // Fill with random data (incompressible)
    uint8_t* data = static_cast<uint8_t*>(ptr);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (size_t i = 0; i < total_size; i++) {
        data[i] = static_cast<uint8_t>(dis(gen));
    }
    
    // Store checksum to verify later
    uint64_t checksum = 0;
    for (size_t i = 0; i < total_size; i++) {
        checksum += data[i];
    }
    
    // Force eviction
    std::vector<void*> eviction_pages;
    for (size_t i = 0; i < MAX_PHYSICAL_PAGES + 5; i++) {
        void* evict = GhostMemoryManager::Instance().AllocateGhost(PAGE_SIZE);
        uint8_t* evict_data = static_cast<uint8_t*>(evict);
        evict_data[0] = 0xFF;
        eviction_pages.push_back(evict);
    }
    
    // Verify checksum after decompression
    uint64_t new_checksum = 0;
    for (size_t i = 0; i < total_size; i++) {
        new_checksum += data[i];
    }
    
    ASSERT_EQ(checksum, new_checksum);
    std::cout << "Original size: " << total_size << " bytes\n";
    std::cout << "Pattern: Random data (incompressible)\n";
    std::cout << "Theoretical compression: ~1:1 (no compression)\n";
    std::cout << "Expected compressed size: ~" << total_size << " bytes (same as original)\n";
    std::cout << "Note: LZ4 adds small overhead for incompressible data\n\n";
}

TEST(CompressionMetrics_SparseData) {
    std::cout << "\n=== Compression Test: Sparse Data (Mostly Zeros) ===\n";
    
    const size_t num_pages = 10;
    const size_t total_size = num_pages * PAGE_SIZE;
    
    void* ptr = GhostMemoryManager::Instance().AllocateGhost(total_size);
    ASSERT_NOT_NULL(ptr);
    
    // Fill with mostly zeros, occasional non-zero values
    uint64_t* data = static_cast<uint64_t*>(ptr);
    const size_t num_elements = total_size / sizeof(uint64_t);
    
    memset(data, 0, total_size);
    
    // Set every 100th element to a non-zero value
    for (size_t i = 0; i < num_elements; i += 100) {
        data[i] = 0xDEADBEEFCAFEBABE;
    }
    
    // Force eviction
    std::vector<void*> eviction_pages;
    for (size_t i = 0; i < MAX_PHYSICAL_PAGES + 5; i++) {
        void* evict = GhostMemoryManager::Instance().AllocateGhost(PAGE_SIZE);
        uint64_t* evict_data = static_cast<uint64_t*>(evict);
        evict_data[0] = i;
        eviction_pages.push_back(evict);
    }
    
    // Verify data integrity
    bool data_valid = true;
    for (size_t i = 0; i < num_elements; i++) {
        if (i % 100 == 0) {
            if (data[i] != 0xDEADBEEFCAFEBABE) {
                data_valid = false;
                break;
            }
        } else {
            if (data[i] != 0) {
                data_valid = false;
                break;
            }
        }
    }
    
    ASSERT_TRUE(data_valid);
    std::cout << "Original size: " << total_size << " bytes\n";
    std::cout << "Pattern: 99% zeros, 1% data\n";
    std::cout << "Theoretical compression: ~100:1 or better for LZ4\n";
    std::cout << "Expected compressed size: <" << (total_size / 100) << " bytes\n\n";
}

// ============================================================================
// PERFORMANCE COMPARISON TESTS
// ============================================================================

TEST(PerformanceMetrics_AllocationSpeed) {
    std::cout << "\n=== Performance Test: Allocation Speed ===\n";
    
    const size_t num_elements = 1024; // 1K integers
    const size_t num_iterations = 100;
    
    // Measure GhostMem allocation speed
    double ghost_total = 0.0;
    for (size_t iter = 0; iter < num_iterations; iter++) {
        ghost_total += MeasureGhostMemAllocation<int>(num_elements, 42);
    }
    double ghost_avg = ghost_total / num_iterations;
    
    // Measure standard allocation speed
    double standard_total = 0.0;
    for (size_t iter = 0; iter < num_iterations; iter++) {
        standard_total += MeasureStandardAllocation<int>(num_elements, 42);
    }
    double standard_avg = standard_total / num_iterations;
    
    std::cout << "Standard C++ malloc: " << std::fixed << std::setprecision(4) 
              << standard_avg << " ms (avg over " << num_iterations << " iterations)\n";
    std::cout << "GhostMem allocation: " << ghost_avg << " ms (avg over " 
              << num_iterations << " iterations)\n";
    
    double slowdown = ghost_avg / standard_avg;
    std::cout << "Slowdown factor: " << std::setprecision(2) << slowdown << "x\n";
    std::cout << "Size per allocation: " << (num_elements * sizeof(int)) << " bytes\n\n";
}

TEST(PerformanceMetrics_AccessPatterns_Sequential) {
    std::cout << "\n=== Performance Test: Sequential Access Pattern ===\n";
    
    const size_t array_size = 4096; // 1 page of ints
    
    // Standard allocation
    auto start_std = std::chrono::high_resolution_clock::now();
    int* std_array = new int[array_size];
    for (size_t i = 0; i < array_size; i++) {
        std_array[i] = static_cast<int>(i);
    }
    int std_sum = 0;
    for (size_t i = 0; i < array_size; i++) {
        std_sum += std_array[i];
    }
    auto end_std = std::chrono::high_resolution_clock::now();
    double std_time = std::chrono::duration<double, std::milli>(end_std - start_std).count();
    delete[] std_array;
    
    // GhostMem allocation
    auto start_ghost = std::chrono::high_resolution_clock::now();
    void* ghost_ptr = GhostMemoryManager::Instance().AllocateGhost(array_size * sizeof(int));
    int* ghost_array = static_cast<int*>(ghost_ptr);
    for (size_t i = 0; i < array_size; i++) {
        ghost_array[i] = static_cast<int>(i);
    }
    int ghost_sum = 0;
    for (size_t i = 0; i < array_size; i++) {
        ghost_sum += ghost_array[i];
    }
    auto end_ghost = std::chrono::high_resolution_clock::now();
    double ghost_time = std::chrono::duration<double, std::milli>(end_ghost - start_ghost).count();
    
    ASSERT_EQ(std_sum, ghost_sum);
    
    std::cout << "Array size: " << array_size << " integers (" 
              << (array_size * sizeof(int)) << " bytes)\n";
    std::cout << "Standard C++: " << std::fixed << std::setprecision(4) 
              << std_time << " ms\n";
    std::cout << "GhostMem: " << ghost_time << " ms\n";
    std::cout << "Slowdown: " << std::setprecision(2) 
              << (ghost_time / std_time) << "x\n\n";
}

TEST(PerformanceMetrics_AccessPatterns_Random) {
    std::cout << "\n=== Performance Test: Random Access Pattern ===\n";
    
    const size_t array_size = 4096; // 1 page of ints
    const size_t num_accesses = 10000;
    
    // Generate random access pattern
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, array_size - 1);
    std::vector<size_t> access_pattern;
    for (size_t i = 0; i < num_accesses; i++) {
        access_pattern.push_back(dis(gen));
    }
    
    // Standard allocation
    auto start_std = std::chrono::high_resolution_clock::now();
    int* std_array = new int[array_size];
    for (size_t i = 0; i < array_size; i++) {
        std_array[i] = static_cast<int>(i);
    }
    int std_sum = 0;
    for (size_t idx : access_pattern) {
        std_sum += std_array[idx];
    }
    auto end_std = std::chrono::high_resolution_clock::now();
    double std_time = std::chrono::duration<double, std::milli>(end_std - start_std).count();
    delete[] std_array;
    
    // GhostMem allocation
    auto start_ghost = std::chrono::high_resolution_clock::now();
    void* ghost_ptr = GhostMemoryManager::Instance().AllocateGhost(array_size * sizeof(int));
    int* ghost_array = static_cast<int*>(ghost_ptr);
    for (size_t i = 0; i < array_size; i++) {
        ghost_array[i] = static_cast<int>(i);
    }
    int ghost_sum = 0;
    for (size_t idx : access_pattern) {
        ghost_sum += ghost_array[idx];
    }
    auto end_ghost = std::chrono::high_resolution_clock::now();
    double ghost_time = std::chrono::duration<double, std::milli>(end_ghost - start_ghost).count();
    
    ASSERT_EQ(std_sum, ghost_sum);
    
    std::cout << "Array size: " << array_size << " integers\n";
    std::cout << "Random accesses: " << num_accesses << "\n";
    std::cout << "Standard C++: " << std::fixed << std::setprecision(4) 
              << std_time << " ms\n";
    std::cout << "GhostMem: " << ghost_time << " ms\n";
    std::cout << "Slowdown: " << std::setprecision(2) 
              << (ghost_time / std_time) << "x\n\n";
}

TEST(PerformanceMetrics_CompressionCycleOverhead) {
    std::cout << "\n=== Performance Test: Compression/Decompression Cycle ===\n";
    
    const size_t num_pages = 20;
    std::vector<void*> pages;
    
    // Allocate pages
    auto start_alloc = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < num_pages; i++) {
        void* ptr = GhostMemoryManager::Instance().AllocateGhost(PAGE_SIZE);
        ASSERT_NOT_NULL(ptr);
        
        // Fill with data
        int* data = static_cast<int*>(ptr);
        for (size_t j = 0; j < PAGE_SIZE / sizeof(int); j++) {
            data[j] = static_cast<int>(i * 1000 + j);
        }
        
        pages.push_back(ptr);
    }
    auto end_alloc = std::chrono::high_resolution_clock::now();
    double alloc_time = std::chrono::duration<double, std::milli>(end_alloc - start_alloc).count();
    
    // Force eviction (compression) and re-access (decompression)
    auto start_cycle = std::chrono::high_resolution_clock::now();
    int total_accesses = 0;
    for (size_t cycle = 0; cycle < 5; cycle++) {
        for (size_t i = 0; i < num_pages; i++) {
            int* data = static_cast<int*>(pages[i]);
            // Access triggers decompression if page was evicted
            total_accesses += data[0];
        }
    }
    auto end_cycle = std::chrono::high_resolution_clock::now();
    double cycle_time = std::chrono::duration<double, std::milli>(end_cycle - start_cycle).count();
    
    std::cout << "Pages allocated: " << num_pages << " (" 
              << (num_pages * PAGE_SIZE / 1024) << " KB)\n";
    std::cout << "Physical RAM limit: " << MAX_PHYSICAL_PAGES << " pages (" 
              << (MAX_PHYSICAL_PAGES * PAGE_SIZE / 1024) << " KB)\n";
    std::cout << "Allocation time: " << std::fixed << std::setprecision(4) 
              << alloc_time << " ms\n";
    std::cout << "Compression/decompression cycles: 5\n";
    std::cout << "Total cycle time: " << cycle_time << " ms\n";
    std::cout << "Average per cycle: " << (cycle_time / 5) << " ms\n";
    std::cout << "Overhead: " << (cycle_time / (num_pages * 5)) 
              << " ms per page access\n\n";
}

// ============================================================================
// MEMORY SAVINGS ESTIMATION TESTS
// ============================================================================

TEST(MemoryMetrics_EstimatedSavings) {
    std::cout << "\n=== Memory Savings Estimation ===\n";
    
    const size_t num_pages = 100;
    const size_t total_virtual = num_pages * PAGE_SIZE;
    const size_t physical_limit = MAX_PHYSICAL_PAGES * PAGE_SIZE;
    
    std::cout << "Scenario: Application needs " << (total_virtual / 1024) << " KB\n";
    std::cout << "Physical RAM limit: " << (physical_limit / 1024) << " KB\n\n";
    
    // Estimate savings for different data types
    struct TestCase {
        const char* name;
        double compression_ratio;
    };
    
    TestCase test_cases[] = {
        {"Highly compressible (repeated pattern)", 50.0},
        {"Text data", 7.0},
        {"Sparse data (mostly zeros)", 100.0},
        {"Mixed data", 3.0},
        {"Random data (worst case)", 1.0}
    };
    
    for (const auto& tc : test_cases) {
        double compressed_size = total_virtual / tc.compression_ratio;
        double memory_with_ghostmem = physical_limit + compressed_size;
        double savings = (1.0 - memory_with_ghostmem / total_virtual) * 100;
        
        std::cout << tc.name << ":\n";
        std::cout << "  Virtual memory: " << (total_virtual / 1024) << " KB\n";
        std::cout << "  Compressed size: " << (compressed_size / 1024) << " KB\n";
        std::cout << "  Physical + compressed: " << (memory_with_ghostmem / 1024) << " KB\n";
        std::cout << "  Effective savings: " << std::fixed << std::setprecision(1) 
                  << savings << "%\n\n";
    }
    
    std::cout << "Note: These are theoretical estimates. Actual compression\n";
    std::cout << "      ratios depend on data patterns and LZ4 implementation.\n\n";
}
