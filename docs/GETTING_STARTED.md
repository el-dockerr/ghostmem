# GhostMem Getting Started Guide

## Table of Contents
- [Quick Start](#quick-start)
- [Basic Usage](#basic-usage)
- [Configuration Options](#configuration-options)
- [Complete Examples](#complete-examples)
- [Common Use Cases](#common-use-cases)
- [Next Steps](#next-steps)

---

## Quick Start

Get up and running with GhostMem in under 5 minutes!

### Step 1: Include Headers

```cpp
#include "ghostmem/GhostMemoryManager.h"
#include "ghostmem/GhostAllocator.h"
#include <vector>
#include <iostream>
```

### Step 2: Use with STL Containers

```cpp
int main() {
    // Create a vector with GhostAllocator
    std::vector<int, GhostAllocator<int>> vec;
    
    // Use it like a normal vector - compression happens automatically!
    for (int i = 0; i < 10000; i++) {
        vec.push_back(i);
    }
    
    // Access elements normally
    std::cout << "Value at index 5000: " << vec[5000] << std::endl;
    
    return 0;
}
```

**That's it!** GhostMem will automatically:
- Compress inactive memory pages
- Decompress pages when accessed
- Manage memory limits
- Clean up on program exit

---

## Basic Usage

### Without Configuration (Default Behavior)

By default, GhostMem uses **in-memory compression** with these settings:
- Maximum 5 pages (20KB) in physical RAM
- LZ4 compression for evicted pages
- Silent operation (no console output)

```cpp
#include "ghostmem/GhostAllocator.h"
#include <vector>

int main() {
    // No configuration needed - just use it!
    std::vector<int, GhostAllocator<int>> data;
    
    for (int i = 0; i < 100000; i++) {
        data.push_back(i * 2);
    }
    
    return 0;
}
```

### With Custom Configuration

For more control, initialize GhostMem with a custom configuration:

```cpp
#include "ghostmem/GhostMemoryManager.h"
#include "ghostmem/GhostAllocator.h"
#include <vector>

int main() {
    // Create configuration
    GhostConfig config;
    config.max_memory_pages = 256;  // 1MB physical RAM
    
    // Initialize before using
    if (!GhostMemoryManager::Instance().Initialize(config)) {
        std::cerr << "Failed to initialize GhostMem\n";
        return 1;
    }
    
    // Now use GhostAllocator
    std::vector<double, GhostAllocator<double>> vec;
    // ... your code ...
    
    return 0;
}
```

---

## Configuration Options

### GhostConfig Structure

```cpp
struct GhostConfig {
    bool use_disk_backing = false;           // Use disk instead of RAM for compressed pages
    std::string disk_file_path = "ghostmem.swap";  // Disk file location
    size_t max_memory_pages = 0;             // Page limit (0 = use default)
    bool compress_before_disk = true;        // Compress before writing to disk
    bool enable_verbose_logging = false;     // Enable debug output
};
```

### Memory Limits

Control how much physical RAM GhostMem uses:

```cpp
GhostConfig config;

// Small devices (256KB RAM)
config.max_memory_pages = 64;

// Medium (1MB RAM)
config.max_memory_pages = 256;

// Large (100MB RAM)
config.max_memory_pages = 25600;

GhostMemoryManager::Instance().Initialize(config);
```

### Disk-Backed Storage

Enable disk storage for compressed pages to minimize RAM usage:

```cpp
GhostConfig config;
config.use_disk_backing = true;              // Enable disk backing
config.disk_file_path = "myapp_ghost.swap";  // Custom file path
config.compress_before_disk = true;          // Compress before disk write
config.max_memory_pages = 100;               // Limit active RAM pages

if (!GhostMemoryManager::Instance().Initialize(config)) {
    std::cerr << "Failed to open disk file\n";
    return 1;
}
```

**Benefits:**
- ✅ Reduces RAM footprint significantly
- ✅ Can handle datasets larger than available RAM
- ⚠️ Slower than in-memory compression (disk I/O latency)

**File Location:**
- Relative paths are relative to working directory
- File is created automatically if it doesn't exist
- File is truncated on each program start

### Verbose Logging

Enable detailed console output for debugging and monitoring:

```cpp
GhostConfig config;
config.enable_verbose_logging = true;  // Enable debug output

GhostMemoryManager::Instance().Initialize(config);

// Now you'll see messages like:
// [GhostMem] Disk backing enabled: ghostmem.swap (compress=yes)
// [GhostMem] Page fully freed: 0x7fff12340000
// [GhostMem] Zombie page freed during eviction: 0x7fff12341000
```

**When to enable:**
- 🔍 Debugging memory issues
- 📊 Monitoring page eviction behavior
- 🧪 Performance testing

**Default:** `false` (silent operation)

---

## Complete Examples

### Example 1: Simple Vector with Default Settings

```cpp
#include "ghostmem/GhostAllocator.h"
#include <vector>
#include <iostream>

int main() {
    std::vector<int, GhostAllocator<int>> numbers;
    
    // Fill with 10,000 integers
    for (int i = 0; i < 10000; i++) {
        numbers.push_back(i);
    }
    
    // Access random element
    std::cout << "Element 5000: " << numbers[5000] << std::endl;
    
    return 0;
}
```

### Example 2: Disk-Backed with Verbose Logging

```cpp
#include "ghostmem/GhostMemoryManager.h"
#include "ghostmem/GhostAllocator.h"
#include <vector>
#include <string>
#include <iostream>

int main() {
    // Configure for disk backing with logging
    GhostConfig config;
    config.use_disk_backing = true;
    config.disk_file_path = "data.swap";
    config.max_memory_pages = 5000;  // ~20MB RAM limit
    config.enable_verbose_logging = true;
    
    if (!GhostMemoryManager::Instance().Initialize(config)) {
        std::cerr << "Initialization failed\n";
        return 1;
    }
    
    std::cout << "GhostMem initialized successfully\n";
    
    // Use with vector
    std::vector<int, GhostAllocator<int>> data;
    
    for (int i = 0; i < 100000; i++) {
        data.push_back(i);
        
        if (i % 10000 == 0) {
            std::cout << "Progress: " << i << " elements\n";
        }
    }
    
    std::cout << "Final size: " << data.size() << std::endl;
    std::cout << "Sample value: " << data[50000] << std::endl;
    
    return 0;
}
```

### Example 3: String Processing with Custom Memory Limit

```cpp
#include "ghostmem/GhostMemoryManager.h"
#include "ghostmem/GhostAllocator.h"
#include <string>
#include <vector>
#include <iostream>

// Define string type with GhostAllocator
using GhostString = std::basic_string<char, 
                                      std::char_traits<char>, 
                                      GhostAllocator<char>>;

int main() {
    // Configure with custom limits
    GhostConfig config;
    config.max_memory_pages = 512;  // 2MB RAM limit
    
    GhostMemoryManager::Instance().Initialize(config);
    
    // Create vector of strings
    std::vector<GhostString, GhostAllocator<GhostString>> log_lines;
    
    // Simulate log processing
    for (int i = 0; i < 10000; i++) {
        GhostString line = GhostString("Log entry #") + 
                          std::to_string(i) + 
                          GhostString(": System operational");
        log_lines.push_back(line);
    }
    
    std::cout << "Processed " << log_lines.size() << " log lines\n";
    std::cout << "First: " << log_lines[0].c_str() << std::endl;
    std::cout << "Last: " << log_lines[9999].c_str() << std::endl;
    
    return 0;
}
```

### Example 4: Production Configuration (Silent + Disk)

```cpp
#include "ghostmem/GhostMemoryManager.h"
#include "ghostmem/GhostAllocator.h"
#include <vector>
#include <iostream>

int main() {
    // Production settings: disk backing, no logging
    GhostConfig config;
    config.use_disk_backing = true;
    config.disk_file_path = "/tmp/myapp.swap";  // Linux/Mac
    // config.disk_file_path = "C:\\Temp\\myapp.swap";  // Windows
    config.compress_before_disk = true;
    config.max_memory_pages = 10000;  // ~40MB RAM limit
    config.enable_verbose_logging = false;  // Silent for production
    
    if (!GhostMemoryManager::Instance().Initialize(config)) {
        // Silent initialization failure handling
        std::cerr << "Critical: Memory manager initialization failed\n";
        return 1;
    }
    
    // Application continues silently
    std::vector<int, GhostAllocator<int>> data;
    
    // Process large dataset
    for (int i = 0; i < 1000000; i++) {
        data.push_back(i * i);
    }
    
    std::cout << "Processing complete: " << data.size() << " elements\n";
    
    return 0;
}
```

---

## Common Use Cases

### IoT Device with Limited RAM

```cpp
GhostConfig config;
config.max_memory_pages = 32;  // Only 128KB RAM
config.use_disk_backing = true;
config.disk_file_path = "/data/ghost.swap";
config.enable_verbose_logging = false;  // Silent on embedded device

GhostMemoryManager::Instance().Initialize(config);
```

### Development/Testing

```cpp
GhostConfig config;
config.max_memory_pages = 100;  // Small limit to force swapping
config.enable_verbose_logging = true;  // See what's happening
config.use_disk_backing = false;  // Fast in-memory testing

GhostMemoryManager::Instance().Initialize(config);
```

### AI Model Inference

```cpp
GhostConfig config;
config.max_memory_pages = 50000;  // ~200MB active RAM
config.use_disk_backing = true;
config.disk_file_path = "model_cache.swap";
config.compress_before_disk = true;
config.enable_verbose_logging = false;

GhostMemoryManager::Instance().Initialize(config);
```

### Data Processing Pipeline

```cpp
GhostConfig config;
config.max_memory_pages = 25600;  // ~100MB RAM
config.use_disk_backing = true;
config.disk_file_path = "./pipeline_swap.dat";
config.enable_verbose_logging = false;

GhostMemoryManager::Instance().Initialize(config);

// Use with multiple containers
std::vector<Record, GhostAllocator<Record>> input_buffer;
std::vector<Result, GhostAllocator<Result>> output_buffer;
```

---

## Next Steps

### Learn More

- **[Integration Guide](INTEGRATION_GUIDE.md)** - Detailed integration instructions and build options
- **[API Reference](API_REFERENCE.md)** - Complete API documentation
- **[Thread Safety](THREAD_SAFETY.md)** - Multi-threading guidelines and best practices
- **[Performance Metrics](PERFORMANCE_METRICS.md)** - Performance testing and optimization
- **[Main README](../README.md)** - Project overview and architecture

### Key Concepts

1. **Page-based Management**: GhostMem works with 4KB pages
2. **LRU Eviction**: Least recently used pages are compressed first
3. **Transparent Operation**: Page faults trigger automatic decompression
4. **Thread-Safe**: Safe for concurrent allocations and access

### Best Practices

✅ **Do:**
- Initialize with `Initialize()` for custom configuration
- Use disk backing for large datasets or extreme memory constraints
- Test with verbose logging enabled during development
- Choose appropriate `max_memory_pages` for your workload

❌ **Don't:**
- Don't call `Initialize()` after starting allocations
- Don't mix GhostAllocator with regular allocators for the same container
- Don't rely on verbose output in production (performance impact)
- Don't forget to handle `Initialize()` failure when disk backing is enabled

### Troubleshooting

**Problem: No disk file created**
- Solution: Ensure you call `Initialize(config)` before using `GhostAllocator`
- Solution: Check that `config.use_disk_backing = true` is set

**Problem: Want to see what's happening**
- Solution: Set `config.enable_verbose_logging = true`

**Problem: Too much memory usage**
- Solution: Reduce `config.max_memory_pages`
- Solution: Enable `config.use_disk_backing = true`

**Problem: Performance too slow**
- Solution: Increase `config.max_memory_pages`
- Solution: Use in-memory mode (`use_disk_backing = false`)
- Solution: Disable verbose logging in production

---

## Quick Reference Card

```cpp
// Minimal usage
std::vector<int, GhostAllocator<int>> vec;

// With disk backing
GhostConfig cfg;
cfg.use_disk_backing = true;
cfg.disk_file_path = "swap.dat";
GhostMemoryManager::Instance().Initialize(cfg);

// With verbose logging
cfg.enable_verbose_logging = true;

// Custom memory limit
cfg.max_memory_pages = 1024;  // 4MB RAM
```

---

**Ready to optimize your memory usage?** Start with the simplest example and add configuration options as needed!

For comprehensive documentation, see the [API Reference](API_REFERENCE.md) and [Integration Guide](INTEGRATION_GUIDE.md).
