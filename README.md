<div align="center">
  <img src="docs/images/ghostmem-maskot-192x192.png" alt="GMlib Logo" width="200"/>

# **GMlib** - GhostMem Library
**Version 1.0.0**

> **Virtual RAM through Transparent Compression** – A modern memory management system for IoT devices and AI applications


[![Documentation](https://img.shields.io/badge/docs-online-informational?style=flat&link=https://www.dockerr.blog/software/ghostmem/)](https://www.dockerr.blog/software/ghostmem)
[![Build and Test](https://github.com/el-dockerr/GhostMem/actions/workflows/build-and-test.yml/badge.svg)](https://github.com/el-dockerr/GhostMem/actions/workflows/build-and-test.yml)
[![Release](https://github.com/el-dockerr/GhostMem/actions/workflows/release.yml/badge.svg)](https://github.com/YOUR_USERNAME/el-dockerr/actions/workflows/release.yml)

</div>

## Features:
* Allows to compress memory of a running program just by adding a simple library
* Allows to use disk as memory - software can literally run without any usage of RAM
* **Proper memory lifecycle management** - Full deallocation support with automatic cleanup
* Thread-safe - Safe for concurrent allocations and deallocations
* No feature creep inside

## 📦 Downloads

Pre-built binaries are available from the [Releases page](https://github.com/el-dockerr/GhostMem/releases).

**Latest Release:**
- **Windows (x64)**: `ghostmem-windows-x64.zip` - Includes DLL, static library, headers, and demo
- **Linux (x64)**: `ghostmem-linux-x64.tar.gz` - Includes SO, static library, headers, and demo

Each release package contains:
```
ghostmem-{platform}/
├── lib/
│   ├── ghostmem_shared.{dll|so}  # Shared library
│   └── ghostmem.{lib|a}          # Static library
├── include/
│   ├── GhostMemoryManager.h
│   ├── GhostAllocator.h
│   └── Version.h
├── bin/
│   └── ghostmem_demo             # Demo application
└── README.md
```

## Overview

### The Memory Crisis

Modern software has become a **RAM monster**. Vendors ship bloated applications that consume gigabytes of memory for basic tasks. Web browsers with a dozen tabs eat 8GB+. Electron apps bundle entire Chrome instances. AI companies push models that demand 16GB, 32GB, or even 64GB of RAM – effectively forcing expensive hardware upgrades on users.

**This is unacceptable.**

While software vendors carelessly waste memory resources, users are left with two choices: buy more expensive RAM or suffer from constant disk swapping that cripples performance. IoT devices and edge computing? Forget it – most "modern" software won't even run.

### The Solution: GhostMem

GhostMem is a thread safe smart memory management system that extends available RAM through **in-memory compression** rather than traditional disk-based swapping. By intercepting page faults and using high-speed LZ4 compression, GhostMem creates the illusion of having more physical memory than actually available – all without requiring disk I/O or application code changes.

**GhostMem lets you reclaim control over your memory.** Run AI models on modest hardware. Deploy sophisticated applications on IoT devices. Stop the vendor-imposed RAM tax.

This is the practical realization of the scam "softRAM" concept from the 90's, but actually working and mostly production-ready.

## How It Works

```
┌─────────────────────────────────────────────────────┐
│  Application (std::vector, std::string, etc.)       │
└─────────────────┬───────────────────────────────────┘
                  │ Uses GhostAllocator
┌─────────────────▼───────────────────────────────────┐
│         GhostMemoryManager                          │
│                                                     │
│  ┌──────────────┐      ┌──────────────┐             │
│  │ Physical RAM │      │  Compressed  │             │
│  │ (5 pages max)│◄────►│ Backing Store│             │
│  │   Active     │ LZ4  │(RAM or Disk) │             │
│  └──────────────┘      └──────────────┘             │
│         ▲                                           │
│         │ Page Fault Handler (Vectored Exception)   │
└─────────┼───────────────────────────────────────────┘
          │
    Access to frozen page triggers decompression
```

### Key Mechanisms

1. **Virtual Memory Reservation**: Pages are reserved but not committed (no RAM used initially)
2. **Page Fault Interception**: Vectored exception handler catches access violations
3. **LRU Eviction**: When physical page limit is reached, least recently used page is evicted
4. **LZ4 Compression**: Evicted pages are compressed and stored in-memory or on disk
5. **Transparent Restoration**: On next access, page is decompressed and restored instantly
6. **Configurable Backing**: Choose between in-memory (default) or disk-backed storage

### Storage Modes

**In-Memory Mode (Default)**
- Compressed pages stored in process memory
- Fastest performance (microsecond latency)
- No disk I/O required
- Best for: Desktop applications, sufficient RAM

**Disk-Backed Mode (Optional)**
- Compressed pages written to disk file
- Minimal memory footprint
- Configurable compression
- Best for: IoT devices, memory-constrained systems, batch processing

```cpp
// Enable disk-backed storage
GhostConfig config;
config.use_disk_backing = true;
config.disk_file_path = "ghostmem.swap";
config.compress_before_disk = true;
config.max_memory_pages = 256;  // 1MB RAM limit

GhostMemoryManager::Instance().Initialize(config);
```

## Advantages Over Traditional Swapping

### 🚀 **Speed**
- **In-Memory Mode**: Everything happens in RAM (even compressed data)
- **LZ4 is Fast**: Compression/decompression runs at GB/s speeds
- **Low Latency**: Microseconds (in-memory) or sub-milliseconds (SSD-backed)

### 💾 **Efficiency**
- **High Compression Ratios**: Typical data compresses 2-10x (especially text, AI model weights)
- **Flexible Storage**: Choose in-memory or disk-backed based on constraints
- **Reduced Memory Footprint**: 10 pages of virtual memory can fit in 2 pages of physical RAM

### 🔒 **Transparency**
- **No Code Changes**: Works with existing C++ containers (std::vector, std::string, etc.)
- **Automatic Management**: Application doesn't need to know about compression
- **STL Compatible**: Drop-in `GhostAllocator` for any STL container

### ⚡ **IoT & AI Optimized**
- **Embedded Friendly**: Optional disk backing for extreme memory constraints
- **Predictable Performance**: No kernel swap subsystem interference  
- **AI Model Inference**: Keep model weights compressed until needed
- **Edge Devices**: Run larger models on memory-constrained hardware

### 🛡️ **Control**
- **Fine-grained Policy**: Custom eviction strategies (LRU, priority-based, etc.)
- **Predictable Behavior**: You control what gets compressed and when
- **No Kernel Overhead**: Stays in user-space, no context switches

## Use Cases

### 📱 **IoT Devices**
Run sophisticated applications on devices with limited RAM (e.g., 16MB devices running 40MB workloads)

### 🤖 **AI Inference**
- Load large language models on edge devices
- Keep inactive layers compressed
- Decompress only the layers being computed

### 🎮 **Gaming & Simulation**
- Keep distant scene data compressed
- Expand usable memory 3-5x through compression

### 📊 **Data Processing**
- Process datasets larger than RAM
- Compress inactive data structures automatically

## Quick Start

```cpp
#include "ghostmem/GhostMemoryManager.h"
#include "ghostmem/GhostAllocator.h"

// Use with any STL container
std::vector<int, GhostAllocator<int>> vec;

for (int i = 0; i < 100000; i++) {
    vec.push_back(i);  // Automatic compression when RAM limit reached
}

int val = vec[50000];  // Automatic decompression on access
```

## Configuration

Adjust these constants in [GhostMemoryManager.h](src/ghostmem/GhostMemoryManager.h):

```cpp
const size_t PAGE_SIZE = 4096;           // Memory page size
const size_t MAX_PHYSICAL_PAGES = 5;     // Max pages in physical RAM
```

## Architecture

### Components

- **GhostMemoryManager**: Core singleton managing virtual/physical memory mapping
- **GhostAllocator**: STL-compatible allocator template
- **Platform Handlers**: 
  - Windows: Vectored exception handler for page fault interception
  - Linux: SIGSEGV signal handler for page fault interception
- **LZ4**: High-speed compression library (3rdparty)

### Memory States

1. **Reserved**: Virtual address allocated, no RAM used
2. **Committed + Active**: Page in physical RAM, in active LRU list
3. **Frozen**: Page compressed in backing_store, RAM decommitted

## Performance Characteristics

| Operation | Typical Time |
|-----------|-------------|
| Page compression (4KB) | ~10-50 µs |
| Page decompression (4KB) | ~5-30 µs |
| Page fault handling | ~20-100 µs |
| Disk swap (comparison) | ~5-50 ms |

**GhostMem is 100-1000x faster than disk-based swapping!**

## Requirements

### Platform Support
- ✅ **Windows** (uses VirtualAlloc and vectored exception handling)
- ✅ **Linux** (uses mmap and SIGSEGV signal handling)
- 🔄 **macOS** (planned - Mach exceptions)

### Build Requirements
- C++17 or later
- CMake 3.10+ (recommended) or direct compiler usage
- LZ4 library (included in 3rdparty/)
- **Windows**: MSVC 2017+ or MinGW-w64
- **Linux**: GCC 7+ or Clang 5+

## Building

### Quick Build (No CMake Required)

#### Windows
```batch
# Run from "Developer Command Prompt for VS"
build-simple.bat
```

#### Linux
```bash
chmod +x build-simple.sh
./build-simple.sh
```

### Recommended Build (Using CMake)

#### Windows
```batch
# Automatically detects Visual Studio and builds
build.bat

# Or manually:
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

#### Linux
```bash
# Automatically builds with optimal settings
chmod +x build.sh
./build.sh

# Or manually:
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Build Outputs

After building, you'll get:

```
build/
├── Release/                    (Windows)
│   ├── ghostmem.lib           (static library)
│   ├── ghostmem.dll           (shared library)
│   └── ghostmem_demo.exe      (demo program)
│
└── (Linux)
    ├── libghostmem.a          (static library)
    ├── libghostmem.so         (shared library)
    └── ghostmem_demo          (demo program)
```

### Running the Demo

**Windows:**
```batch
cd build\Release
ghostmem_demo.exe
```

**Linux:**
```bash
cd build
./ghostmem_demo
```

### Running Tests

The project includes comprehensive test suites for correctness and performance:

**Run all tests:**
```batch
# Windows
cd build\Release
ghostmem_tests.exe

# Linux
cd build
./ghostmem_tests
```

**Run performance metrics tests only:**
```batch
# Windows
run_metrics.bat

# Linux
chmod +x run_metrics.sh
./run_metrics.sh
```

The metrics tests measure:
- **Compression ratios** for different data types (text, sparse data, random data)
- **Memory savings** achieved through compression (typically 60-95%)
- **Performance overhead** compared to standard C++ allocation (3-5x slowdown)
- **Speed comparisons** between malloc and GhostMem operations

Results are saved to `metrics_results/` with timestamps for comparison across versions.

For detailed information about performance metrics and how to use them for improvements, see [docs/PERFORMANCE_METRICS.md](docs/PERFORMANCE_METRICS.md).

## Roadmap

### 🐧 **Linux & Cross-Platform Support**
- ✅ Linux implementation using signal handlers for page fault handling
- [ ] macOS support using Mach exceptions (if someone dare to spend me a MAC - I would never buy this)
- ✅ Build scripts for creating shared libraries (DLL/SO)
- ✅ `build.bat` / `build.sh` - CMake-based build for Windows/Linux
- ✅ `build-simple.bat` / `build-simple.sh` - Direct compilation without CMake
- ✅ CMake configuration for cross-platform builds
- ✅ Platform abstraction layer for memory management APIs
- ✅ ARM architecture support for embedded devices

### 🧪 **Testing & Quality**
- ✅ Unit tests for core components
  - Memory allocation/deallocation
  - Compression/decompression cycles
  - LRU eviction policy
  - Page fault handling
- ✅ Performance metrics tests → **[tests/test_metrics.cpp](tests/test_metrics.cpp)**
  - Compression ratio measurements
  - Memory savings estimation
  - Speed comparisons (malloc vs GhostMem)
  - Access pattern performance
- [ ] Integration tests with real applications
- [ ] Stress tests (concurrent access, high memory pressure)
- [ ] Memory leak detection and validation
- ✅ CI/CD pipeline (GitHub Actions)

### 📚 **Documentation**
- ✅ API Reference documentation → **[docs/API_REFERENCE.md](docs/API_REFERENCE.md)**
  - Detailed function/class documentation
  - Memory lifecycle diagrams
  - Thread safety guarantees
- ✅ Integration guides → **[docs/INTEGRATION_GUIDE.md](docs/INTEGRATION_GUIDE.md)**
  - Using GhostMem as a DLL/SO
  - Custom allocator examples
  - Configuration best practices
- ✅ Thread safety documentation → **[docs/THREAD_SAFETY.md](docs/THREAD_SAFETY.md)**
  - Multi-threading guarantees and patterns
  - Performance in concurrent scenarios
  - Platform-specific considerations
- ✅ Performance metrics guide → **[docs/PERFORMANCE_METRICS.md](docs/PERFORMANCE_METRICS.md)**
  - Understanding compression ratios
  - Performance benchmarking methodology
  - How to use metrics for improvements
  - KPIs and optimization targets
- [ ] Performance tuning guide
  - Choosing optimal `MAX_PHYSICAL_PAGES`
  - Workload-specific configurations
  - Profiling and monitoring
- [ ] Architecture deep-dive
  - Internal data structures
  - Eviction algorithm details
  - Platform-specific implementations
- [ ] Case studies and real-world examples
  - IoT device deployments
  - AI inference optimization
  - Gaming applications

### 🚀 **Features**
- ✅ Thread safety and multi-threading support
- ✅ Proper memory deallocation and lifecycle management
- [ ] Smart eviction policies (frequency-based, priority, access patterns)
- [ ] Memory pool support for faster allocation
- [ ] Statistics and monitoring API

## Limitations & Current Status

- **Cross-Platform**: Works on Windows and Linux
- **Current**: Static configuration (no runtime tuning)
- **In Progress**: See Roadmap above for planned improvements

## Technical Details

### Why This Works

Modern applications have **temporal locality**: they don't access all memory uniformly. GhostMem exploits this by:

1. Keeping "hot" data decompressed in RAM
2. Compressing "cold" data and freeing physical RAM
3. Restoring data transparently when accessed again

### Compression Ratios

Real-world data compression with LZ4:

- **Text/Strings**: 5-10x compression
- **Repeated data**: 10-50x compression  
- **AI model weights**: 2-4x compression
- **Random data**: 1x (no compression, but no harm)

Even with conservative 2x compression, you effectively **double your usable RAM**.

## 🚀 Creating a Release

### For Maintainers

To create a new release with downloadable binaries:

1. **Update version number** in [Version.h](src/ghostmem/Version.h) and this README
2. **Commit and push** your changes
3. **Create and push a version tag**:
   ```bash
   git tag -a v0.9.0 -m "Release version 0.9.0"
   git push origin v0.9.0
   ```

4. The [Release workflow](.github/workflows/release.yml) will automatically:
   - Build binaries for Windows (x64) and Linux (x64)
   - Create release packages with libraries, headers, and demo
   - Publish a GitHub Release with downloadable artifacts

Alternatively, trigger a release manually from GitHub Actions:
- Go to **Actions** → **Release** → **Run workflow**
- Enter the tag name (e.g., `v0.9.0`)

### Manual Release Build

If you need to build release packages locally:

**Windows:**
```batch
.\build.bat
cd release
powershell Compress-Archive -Path ghostmem-windows-x64 -DestinationPath ghostmem-windows-x64.zip
```

**Linux:**
```bash
./build.sh
mkdir -p release/ghostmem-linux-x64/{lib,include,bin}
cp build/libghostmem_shared.so release/ghostmem-linux-x64/lib/
cp build/libghostmem.a release/ghostmem-linux-x64/lib/
cp src/ghostmem/*.h release/ghostmem-linux-x64/include/
cp build/ghostmem_demo release/ghostmem-linux-x64/bin/
cd release
tar -czf ghostmem-linux-x64.tar.gz ghostmem-linux-x64/
```

## Credits

Inspired by the scam of "DoubleRAM" concept, but actually implemented with sound engineering principles and modern compression techniques. Designed and written by Swen Kalski

Built with:
- LZ4 compression by Yann Collet
- Windows virtual memory management APIs
- Modern C++ and STL

## License

**GhostMem (GMlib)** is released under the **GNU General Public License v3.0 (GPLv3)**.

Copyright (C) 2026 Swen Kalski

**GhostMem Maskot** is also released under the **GNU General Public License v3.0 (GPLv3)** for usage along the GMlib.

Copyright (C) 2026 Jasmin Kalini

### Important License Information

- ✅ **Free and Open Source**: You are free to use, modify, and distribute GhostMem under GPLv3 terms
- ⚖️ **License Enforcement**: This project is actively monitored for GPL compliance. Violation of GPLv3 terms will be pursued legally
- 🏢 **Commercial/Proprietary Use**: If you wish to use GhostMem in a closed-source or proprietary project, you **must** contact the author for a commercial license
- 📧 **Commercial Licensing**: For licensing outside GPLv3 terms, contact: [kalski.swen@gmail.com]

### Your Rights Under GPLv3

- Use the software for any purpose
- Study and modify the source code
- Share copies of the software
- Share modified versions

### Your Obligations Under GPLv3

- **Copyleft**: Any distributed modifications must also be licensed under GPLv3
- **Source Disclosure**: You must provide source code for any distributed versions
- **License Notice**: You must include the GPLv3 license and copyright notice
- **State Changes**: You must document any modifications made to the code

See the [LICENSE](LICENSE) file for the full GPLv3 license text.

**⚠️ Compliance Notice**: Companies and individuals using GhostMem must comply with GPLv3 terms. Non-compliance will result in legal action to protect open-source rights. Furthermore I added pattern to identify the usage of GMLib. You are always advised to comply with the license.

---

**GhostMem** – Because your IoT device deserves better than swapping to an SD card. 👻✨
