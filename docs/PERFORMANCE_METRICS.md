# GhostMem Performance Metrics Guide

## Overview

This document explains the performance and compression metrics tests for GhostMem and how to use them for evaluating improvements.

## Test Categories

### 1. Compression Ratio Tests

These tests measure how effectively GhostMem compresses different types of data:

#### Test: `CompressionMetrics_HighlyCompressibleData`
- **Data Type**: Repeating pattern (0xAAAAAAAA)
- **Expected Compression**: 50:1 to 100:1
- **Use Case**: Best-case scenario showing maximum RAM savings
- **Real-world analog**: Sparse arrays, zero-initialized buffers

#### Test: `CompressionMetrics_TextData`
- **Data Type**: Repeating text strings
- **Expected Compression**: 5:1 to 10:1
- **Use Case**: String-heavy applications, text processing
- **Real-world analog**: Log buffers, string pools

#### Test: `CompressionMetrics_RandomData`
- **Data Type**: Random bytes (incompressible)
- **Expected Compression**: ~1:1 (no compression)
- **Use Case**: Worst-case scenario
- **Real-world analog**: Encrypted data, compressed images/videos

#### Test: `CompressionMetrics_SparseData`
- **Data Type**: 99% zeros, 1% data
- **Expected Compression**: 100:1 or better
- **Use Case**: Sparse matrices, sparse data structures
- **Real-world analog**: Scientific computing, graph algorithms

### 2. Performance Comparison Tests

These tests compare GhostMem performance against standard C++ memory allocation:

#### Test: `PerformanceMetrics_AllocationSpeed`
- **Measures**: Allocation and initialization time
- **Comparison**: malloc vs. GhostMem
- **Current Results**: ~44x slowdown for GhostMem
- **Why**: Virtual memory reservation + page fault setup overhead
- **Optimization Target**: Reduce overhead through batch operations

#### Test: `PerformanceMetrics_AccessPatterns_Sequential`
- **Measures**: Sequential read/write performance
- **Pattern**: Linear array traversal
- **Current Results**: ~5x slowdown for GhostMem
- **Why**: Page fault handling on first access
- **Optimization Target**: Optimize page fault handler, reduce contention

#### Test: `PerformanceMetrics_AccessPatterns_Random`
- **Measures**: Random access performance
- **Pattern**: Random reads from array
- **Current Results**: ~3.8x slowdown for GhostMem
- **Why**: Distributed page fault pattern
- **Optimization Target**: Improve LRU cache efficiency

#### Test: `PerformanceMetrics_CompressionCycleOverhead`
- **Measures**: Compression/decompression cycle time
- **Scenario**: Multiple pages with forced eviction
- **Current Results**: Varies based on data compressibility
- **Why**: LZ4 compression/decompression time
- **Optimization Target**: Parallel compression, better eviction strategy

### 3. Memory Savings Estimation Tests

#### Test: `MemoryMetrics_EstimatedSavings`
- **Measures**: Theoretical RAM savings for different data types
- **Key Metrics**:
  - **Highly compressible data**: 93% savings
  - **Text data**: 80.7% savings
  - **Sparse data**: 94% savings
  - **Mixed data**: 61.7% savings
  - **Random data**: -5% (overhead, no savings)

## Key Performance Indicators (KPIs)

### Compression Efficiency
```
Compression Ratio = Original Size / Compressed Size
Memory Savings % = (1 - (Physical + Compressed) / Virtual) × 100
```

### Performance Overhead
```
Slowdown Factor = GhostMem Time / Standard C++ Time
Overhead per Operation = (GhostMem Time - Standard Time) / Operations
```

## How to Use These Metrics for Improvements

### 1. Establish Baseline
Before making any changes:
```bash
cd build/Release
./ghostmem_tests > baseline_metrics.txt
```

### 2. Make Improvements
Example optimization areas:
- **LZ4 Settings**: Adjust compression level
- **Page Size**: Experiment with larger/smaller pages
- **LRU Policy**: Try different eviction strategies
- **Parallel Compression**: Compress multiple pages simultaneously
- **Pre-allocation**: Batch page fault handling

### 3. Measure Impact
After changes:
```bash
./ghostmem_tests > new_metrics.txt
diff baseline_metrics.txt new_metrics.txt
```

### 4. Compare Results
Look for:
- **Improved slowdown factors**: Lower is better
- **Higher compression ratios**: More RAM savings
- **Lower cycle overhead**: Faster compress/decompress
- **No regressions**: Ensure correctness tests still pass

## Interpreting Results

### Acceptable Performance Targets

| Metric | Target | Current | Status |
|--------|--------|---------|--------|
| Allocation overhead | <10x | 44x | ⚠️ Needs improvement |
| Sequential access | <2x | 5x | ⚠️ Needs improvement |
| Random access | <3x | 3.8x | ⚙️ Acceptable |
| Compression ratio (text) | >5:1 | 5-10:1 | ✅ Good |
| Memory savings (text) | >70% | 80.7% | ✅ Excellent |

### When to Use GhostMem

**Recommended** when:
- Data is compressible (text, sparse data, patterns)
- Memory is limited and expensive
- Working set exceeds physical RAM
- Performance overhead < 5x is acceptable

**Not recommended** when:
- Data is random/encrypted (incompressible)
- Performance is critical (<2x overhead required)
- Working set fits easily in physical RAM
- Frequent small allocations needed

## Advanced Metrics

### Custom Performance Tests

You can add custom tests to `test_metrics.cpp`:

```cpp
TEST(CustomMetric_YourUseCase) {
    std::cout << "\n=== Custom Test: Your Description ===\n";
    
    // Your benchmark code here
    auto start = std::chrono::high_resolution_clock::now();
    
    // ... test operations ...
    
    auto end = std::chrono::high_resolution_clock::now();
    double time_ms = std::chrono::duration<double, std::milli>(end - start).count();
    
    std::cout << "Result: " << time_ms << " ms\n";
}
```

### Collecting System Metrics

For deeper analysis, consider adding:
- **CPU usage**: Track compression CPU overhead
- **Memory bandwidth**: Measure memory I/O
- **Page fault count**: Count actual page faults
- **Cache statistics**: L1/L2 cache hit rates

## Continuous Integration

Add metrics to CI/CD:

```yaml
# .github/workflows/performance.yml
- name: Run performance tests
  run: |
    cd build/Release
    ./ghostmem_tests | tee metrics.txt
    
- name: Compare with baseline
  run: |
    python scripts/compare_metrics.py baseline.txt metrics.txt
```

## Troubleshooting

### Tests Run Too Fast (0.0000 ms)
- Increase iteration count
- Use larger data sets
- Add more complex operations

### High Variance in Results
- Run multiple iterations
- Calculate average and standard deviation
- Disable CPU frequency scaling
- Close background applications

### Out of Memory Errors
- Reduce `num_pages` in tests
- Increase `MAX_PHYSICAL_PAGES`
- Use smaller test data sets

## Future Improvements

Potential enhancements to metrics tests:

1. **Real-world workload simulation**
   - Database buffer pool
   - Image processing pipeline
   - Large graph algorithms

2. **Multi-threaded metrics**
   - Concurrent compression efficiency
   - Thread contention measurement
   - Lock-free data structure performance

3. **Memory fragmentation tests**
   - External fragmentation tracking
   - Internal fragmentation analysis

4. **Power consumption metrics**
   - Energy per compression cycle
   - Power efficiency vs. performance

5. **Detailed statistics API**
   ```cpp
   struct GhostMemStats {
       size_t total_compressions;
       size_t total_decompressions;
       size_t total_compressed_bytes;
       size_t total_original_bytes;
       double avg_compression_ratio;
       double avg_compression_time_ms;
   };
   
   GhostMemStats stats = GhostMemoryManager::Instance().GetStatistics();
   ```

## References

- [LZ4 Compression Benchmarks](https://github.com/lz4/lz4)
- [Virtual Memory Performance](https://www.kernel.org/doc/html/latest/admin-guide/mm/index.html)
- [Memory Compression in Operating Systems](https://lwn.net/Articles/545244/)

## Contact

For questions or suggestions about metrics:
- Email: kalski.swen@gmail.com
- GitHub Issues: [GhostMem Issues](https://github.com/yourusername/GhostMem/issues)
