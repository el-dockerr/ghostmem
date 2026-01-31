# GhostMem Integration Guide

## Table of Contents
- [Quick Start](#quick-start)
- [Using Pre-built Binaries](#using-pre-built-binaries)
- [Building from Source](#building-from-source)
- [Integration Methods](#integration-methods)
- [Custom Allocator Examples](#custom-allocator-examples)
- [Configuration Best Practices](#configuration-best-practices)
- [Troubleshooting](#troubleshooting)

---

## Quick Start

### 1. Download or Build

**Option A: Download pre-built binaries**
- Get the latest release from [GitHub Releases](https://github.com/el-dockerr/GhostMem/releases)
- Extract the archive

**Option B: Build from source**
```bash
git clone https://github.com/el-dockerr/GhostMem.git
cd GhostMem
./build.sh        # Linux
# or
build.bat         # Windows
```

### 2. Basic Usage

```cpp
#include "ghostmem/GhostMemoryManager.h"
#include "ghostmem/GhostAllocator.h"
#include <vector>

int main() {
    // Use with STL containers
    std::vector<int, GhostAllocator<int>> vec;
    
    // Push data - compression happens automatically
    for (int i = 0; i < 1000000; i++) {
        vec.push_back(i);
    }
    
    // Access data - decompression happens automatically
    int value = vec[500000];
    
    return 0;
}
```

---

## Using Pre-built Binaries

### Windows (DLL)

#### 1. Extract Release Package
```
ghostmem-windows-x64/
├── lib/
│   ├── ghostmem_shared.dll    # Dynamic library
│   └── ghostmem.lib           # Static library
├── include/
│   ├── GhostMemoryManager.h
│   ├── GhostAllocator.h
│   └── Version.h
└── bin/
    └── ghostmem_demo.exe
```

#### 2. Copy Files to Your Project
```
YourProject/
├── include/
│   └── ghostmem/              # Copy all headers here
├── lib/
│   ├── ghostmem_shared.dll    # Copy DLL here
│   └── ghostmem.lib           # Copy static lib here
└── src/
    └── main.cpp
```

#### 3. Configure Your Build System

**Visual Studio:**
1. **Include directories:** Add `include/` to include paths
2. **Library directories:** Add `lib/` to library paths
3. **Link library:** Add `ghostmem.lib` to linker input
4. **Runtime:** Copy `ghostmem_shared.dll` to output directory

**CMake:**
```cmake
cmake_minimum_required(VERSION 3.10)
project(MyApp)

# Add include directory
include_directories(${CMAKE_SOURCE_DIR}/include)

# Add library directory
link_directories(${CMAKE_SOURCE_DIR}/lib)

# Create executable
add_executable(myapp src/main.cpp)

# Link against static library
target_link_libraries(myapp ghostmem)

# Or link against DLL (copy DLL to output dir)
# target_link_libraries(myapp ghostmem_shared)
# add_custom_command(TARGET myapp POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#     "${CMAKE_SOURCE_DIR}/lib/ghostmem_shared.dll"
#     $<TARGET_FILE_DIR:myapp>)
```

**Makefile:**
```makefile
CXX = cl.exe
CXXFLAGS = /I include /EHsc /std:c++17
LDFLAGS = /link /LIBPATH:lib ghostmem.lib

myapp.exe: src/main.cpp
	$(CXX) $(CXXFLAGS) src/main.cpp $(LDFLAGS) /Fe:myapp.exe
```

#### 4. Build and Run
```batch
# Build
cmake --build . --config Release

# Run (DLL must be in same dir or in PATH)
myapp.exe
```

---

### Linux (Shared Object)

#### 1. Extract Release Package
```
ghostmem-linux-x64/
├── lib/
│   ├── libghostmem.so         # Shared library
│   └── libghostmem.a          # Static library
├── include/
│   ├── GhostMemoryManager.h
│   ├── GhostAllocator.h
│   └── Version.h
└── bin/
    └── ghostmem_demo
```

#### 2. Install System-Wide (Optional)
```bash
# Copy headers
sudo cp -r include/ghostmem /usr/local/include/

# Copy libraries
sudo cp lib/libghostmem.so /usr/local/lib/
sudo cp lib/libghostmem.a /usr/local/lib/

# Update library cache
sudo ldconfig
```

#### 3. Configure Your Build System

**CMake:**
```cmake
cmake_minimum_required(VERSION 3.10)
project(MyApp)

# If installed system-wide, just link
add_executable(myapp src/main.cpp)
target_link_libraries(myapp ghostmem pthread)

# Or use local installation
# include_directories(${CMAKE_SOURCE_DIR}/include)
# link_directories(${CMAKE_SOURCE_DIR}/lib)
# target_link_libraries(myapp ghostmem pthread)
```

**Makefile:**
```makefile
CXX = g++
CXXFLAGS = -std=c++17 -I include
LDFLAGS = -L lib -lghostmem -lpthread

myapp: src/main.cpp
	$(CXX) $(CXXFLAGS) src/main.cpp $(LDFLAGS) -o myapp

# If using shared library, may need to set LD_LIBRARY_PATH
run: myapp
	LD_LIBRARY_PATH=lib ./myapp
```

**pkg-config (Advanced):**
```bash
# Create ghostmem.pc file
cat > /usr/local/lib/pkgconfig/ghostmem.pc << EOF
prefix=/usr/local
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: GhostMem
Description: Virtual RAM through transparent compression
Version: 0.9.0
Libs: -L\${libdir} -lghostmem -lpthread
Cflags: -I\${includedir}
EOF

# Use in Makefile
CXXFLAGS = $(shell pkg-config --cflags ghostmem)
LDFLAGS = $(shell pkg-config --libs ghostmem)
```

#### 4. Build and Run
```bash
# Build
make

# Run with local shared library
LD_LIBRARY_PATH=lib ./myapp

# Or if installed system-wide
./myapp
```

---

## Building from Source

### Including in Your Project

#### Method 1: Add as Subdirectory (CMake)

```cmake
# Your CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(MyApp)

# Add GhostMem as subdirectory
add_subdirectory(external/GhostMem)

# Create your executable
add_executable(myapp src/main.cpp)

# Link against GhostMem
target_link_libraries(myapp ghostmem)
target_include_directories(myapp PRIVATE external/GhostMem/src)
```

**Project structure:**
```
MyApp/
├── CMakeLists.txt
├── external/
│   └── GhostMem/           # Git submodule or copied source
└── src/
    └── main.cpp
```

#### Method 2: Direct Source Integration

Copy GhostMem source files directly into your project:

```
MyApp/
├── src/
│   ├── main.cpp
│   └── ghostmem/
│       ├── GhostMemoryManager.cpp
│       ├── GhostMemoryManager.h
│       ├── GhostAllocator.h
│       └── Version.h
└── 3rdparty/
    ├── lz4.c
    └── lz4.h
```

**Compile everything together:**
```bash
g++ -std=c++17 src/main.cpp \
    src/ghostmem/GhostMemoryManager.cpp \
    3rdparty/lz4.c \
    -I src -pthread -o myapp
```

---

## Integration Methods

### 1. Using GhostAllocator with STL Containers

#### std::vector
```cpp
#include "ghostmem/GhostAllocator.h"
#include <vector>

// Basic usage
std::vector<int, GhostAllocator<int>> ghost_vec;
ghost_vec.push_back(42);

// With reserve
std::vector<double, GhostAllocator<double>> data;
data.reserve(1000000);  // Reserve space (still virtual)
for (int i = 0; i < 1000000; i++) {
    data.push_back(i * 3.14);
}
```

#### std::string
```cpp
#include "ghostmem/GhostAllocator.h"
#include <string>

// Type alias for convenience
using GhostString = std::basic_string<
    char, 
    std::char_traits<char>, 
    GhostAllocator<char>
>;

GhostString text = "This string uses ghost memory!";
text += " Compression happens automatically.";
```

#### std::map
```cpp
#include "ghostmem/GhostAllocator.h"
#include <map>

using GhostMap = std::map<
    int, 
    std::string,
    std::less<int>,
    GhostAllocator<std::pair<const int, std::string>>
>;

GhostMap cache;
cache[1] = "one";
cache[2] = "two";
```

#### std::unordered_map
```cpp
#include "ghostmem/GhostAllocator.h"
#include <unordered_map>

using GhostHashMap = std::unordered_map<
    std::string,
    int,
    std::hash<std::string>,
    std::equal_to<std::string>,
    GhostAllocator<std::pair<const std::string, int>>
>;

GhostHashMap lookup;
lookup["answer"] = 42;
```

---

### 2. Direct Memory Allocation

```cpp
#include "ghostmem/GhostMemoryManager.h"

// Allocate raw memory
void* ptr = GhostMemoryManager::Instance().AllocateGhost(1024 * 1024);  // 1MB
if (ptr == nullptr) {
    // Handle allocation failure
    throw std::bad_alloc();
}

// Use the memory
int* data = static_cast<int*>(ptr);
data[0] = 42;
data[1] = 100;

// Note: Deallocation not fully implemented yet
// GhostMemoryManager::Instance().DeallocateGhost(ptr);
```

---

### 3. Custom Allocator Wrapper

Create your own allocator that adds features:

```cpp
#include "ghostmem/GhostAllocator.h"
#include <memory>

template<typename T>
class LoggingGhostAllocator : public GhostAllocator<T> {
public:
    using Base = GhostAllocator<T>;
    using typename Base::pointer;
    using typename Base::size_type;
    
    pointer allocate(size_type n) {
        std::cout << "Allocating " << n << " objects of size " 
                  << sizeof(T) << std::endl;
        return Base::allocate(n);
    }
    
    void deallocate(pointer p, size_type n) {
        std::cout << "Deallocating " << n << " objects" << std::endl;
        Base::deallocate(p, n);
    }
};

// Usage
std::vector<int, LoggingGhostAllocator<int>> logged_vec;
logged_vec.push_back(42);  // Prints allocation message
```

---

## Custom Allocator Examples

### Example 1: Large Dataset Processing

```cpp
#include "ghostmem/GhostAllocator.h"
#include <vector>
#include <numeric>

void process_large_dataset() {
    // Allocate vector for 10 million integers (~40MB)
    std::vector<int, GhostAllocator<int>> dataset;
    dataset.reserve(10'000'000);
    
    // Fill with data
    for (int i = 0; i < 10'000'000; i++) {
        dataset.push_back(i);
    }
    
    // Process in chunks (keeps memory hot)
    const size_t chunk_size = 100'000;
    for (size_t i = 0; i < dataset.size(); i += chunk_size) {
        size_t end = std::min(i + chunk_size, dataset.size());
        
        // Sum this chunk
        int sum = std::accumulate(
            dataset.begin() + i, 
            dataset.begin() + end, 
            0
        );
        
        std::cout << "Chunk " << i/chunk_size << " sum: " << sum << std::endl;
    }
}
```

### Example 2: AI Model Weights

```cpp
#include "ghostmem/GhostAllocator.h"
#include <vector>
#include <array>

class NeuralNetworkLayer {
public:
    using WeightVector = std::vector<float, GhostAllocator<float>>;
    
    NeuralNetworkLayer(size_t input_size, size_t output_size) 
        : weights_(input_size * output_size) 
    {
        // Initialize weights (compressed automatically when not in use)
        for (size_t i = 0; i < weights_.size(); i++) {
            weights_[i] = (rand() % 100) / 100.0f;
        }
    }
    
    WeightVector forward(const std::vector<float>& input) {
        // Accessing weights triggers decompression if needed
        WeightVector output;
        // ... neural network math ...
        return output;
    }
    
private:
    WeightVector weights_;  // Compressed when not in use
};

int main() {
    // Create multiple layers - only active layer stays in RAM
    std::vector<NeuralNetworkLayer> model;
    model.emplace_back(1024, 512);   // Layer 1
    model.emplace_back(512, 256);    // Layer 2
    model.emplace_back(256, 10);     // Layer 3 (output)
    
    // Run inference - layers decompress as needed
    std::vector<float> input(1024, 0.5f);
    auto result = model[0].forward(input);
    // ... continue through layers ...
}
```

### Example 3: Caching System

```cpp
#include "ghostmem/GhostAllocator.h"
#include <unordered_map>
#include <string>

class GhostCache {
public:
    using CacheMap = std::unordered_map<
        std::string,
        std::vector<char, GhostAllocator<char>>,
        std::hash<std::string>,
        std::equal_to<std::string>,
        GhostAllocator<std::pair<const std::string, 
                                  std::vector<char, GhostAllocator<char>>>>
    >;
    
    void put(const std::string& key, const std::vector<char>& data) {
        // Store in ghost memory - compressed automatically
        std::vector<char, GhostAllocator<char>> ghost_data(
            data.begin(), data.end()
        );
        cache_[key] = std::move(ghost_data);
    }
    
    std::vector<char> get(const std::string& key) {
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            // Decompresses automatically on access
            return std::vector<char>(it->second.begin(), it->second.end());
        }
        return {};
    }
    
private:
    CacheMap cache_;
};
```

---

## Configuration Best Practices

### 1. Tuning MAX_PHYSICAL_PAGES

Edit `src/ghostmem/GhostMemoryManager.h`:

```cpp
// For IoT devices (limited RAM)
const size_t MAX_PHYSICAL_PAGES = 10;      // 40KB physical RAM

// For desktop applications
const size_t MAX_PHYSICAL_PAGES = 1024;    // 4MB physical RAM

// For server applications
const size_t MAX_PHYSICAL_PAGES = 10000;   // 40MB physical RAM
```

**Guidelines:**
- **Too low:** Frequent compression/decompression (higher CPU, lower performance)
- **Too high:** More physical RAM used, less compression benefit
- **Optimal:** Balance based on your working set size

**Formula:**
```
Working Set Size ≈ MAX_PHYSICAL_PAGES × PAGE_SIZE
```

**Benchmarking:**
```cpp
#include <chrono>

auto start = std::chrono::high_resolution_clock::now();
// ... your workload ...
auto end = std::chrono::high_resolution_clock::now();

auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
    end - start
).count();

std::cout << "Time: " << duration << "ms" << std::endl;
```

Test with different `MAX_PHYSICAL_PAGES` values to find optimal setting.

---

### 2. Access Patterns

**✅ Good: Sequential access**
```cpp
std::vector<int, GhostAllocator<int>> data(1000000);

// Good - accesses pages sequentially
for (size_t i = 0; i < data.size(); i++) {
    data[i] = i * 2;
}
```

**❌ Bad: Random access**
```cpp
// Bad - may thrash between pages
for (int i = 0; i < 1000000; i++) {
    size_t random_idx = rand() % data.size();
    data[random_idx]++;  // Random access causes page faults
}
```

---

### 3. Data Organization

**✅ Good: Group related data**
```cpp
struct DataChunk {
    std::array<int, 1000> values;  // Fits in one page
};

std::vector<DataChunk, GhostAllocator<DataChunk>> chunks;
```

**❌ Bad: Scattered data**
```cpp
// Each element in different page - bad locality
std::vector<std::unique_ptr<LargeObject>, 
            GhostAllocator<std::unique_ptr<LargeObject>>> scattered;
```

---

### 4. Pre-allocation

**✅ Good: Reserve capacity upfront**
```cpp
std::vector<int, GhostAllocator<int>> vec;
vec.reserve(1000000);  // Reserve virtual space

for (int i = 0; i < 1000000; i++) {
    vec.push_back(i);  // No reallocations
}
```

**❌ Bad: Grow dynamically**
```cpp
std::vector<int, GhostAllocator<int>> vec;
// Vector grows and reallocates multiple times
for (int i = 0; i < 1000000; i++) {
    vec.push_back(i);  // May cause multiple reallocations
}
```

---

## Troubleshooting

### Problem: Linker errors

**Error:** `undefined reference to GhostMemoryManager::Instance()`

**Solution:** Make sure you're linking against the library:
```cmake
target_link_libraries(myapp ghostmem pthread)  # Linux
target_link_libraries(myapp ghostmem)          # Windows
```

---

### Problem: DLL not found (Windows)

**Error:** `The code execution cannot proceed because ghostmem_shared.dll was not found.`

**Solution:** 
1. Copy DLL to executable directory
2. Or add DLL directory to PATH
3. Or use static linking instead

---

### Problem: Shared library not found (Linux)

**Error:** `error while loading shared libraries: libghostmem.so: cannot open shared object file`

**Solution:**
```bash
# Option 1: Set LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/path/to/ghostmem/lib:$LD_LIBRARY_PATH

# Option 2: Install system-wide
sudo cp lib/libghostmem.so /usr/local/lib/
sudo ldconfig

# Option 3: Use rpath in CMake
set_target_properties(myapp PROPERTIES
    BUILD_RPATH "/path/to/ghostmem/lib"
    INSTALL_RPATH "/usr/local/lib")
```

---

### Problem: Segmentation fault on access

**Possible causes:**
1. **Accessing deallocated memory** - Don't use pointers after deallocation
2. **Stack overflow** - Reduce `MAX_PHYSICAL_PAGES` or increase stack size
3. **Signal handler conflict** (Linux) - Check for conflicting SIGSEGV handlers

**Debug:**
```bash
# Linux - run with debugger
gdb ./myapp
(gdb) run
(gdb) bt  # Show backtrace when it crashes

# Check system limits
ulimit -a
```

---

### Problem: Poor performance

**Diagnosis:**
1. Too many page faults (MAX_PHYSICAL_PAGES too low)
2. Poor access patterns (random access)
3. Data not compressible (encrypted, already compressed)

**Solution:**
1. Increase `MAX_PHYSICAL_PAGES`
2. Improve data locality
3. Profile with your workload
4. Consider if GhostMem is right fit for your use case

---

## Next Steps

- Read the [API Reference](API_REFERENCE.md) for detailed function documentation
- Check [Thread Safety](THREAD_SAFETY.md) for multi-threading guidelines
- See [examples/](../src/main.cpp) for more code samples
- Review [README](../README.md) for architecture details
