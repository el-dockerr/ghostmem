# GhostMem API Reference

## Table of Contents
- [Core Classes](#core-classes)
  - [GhostMemoryManager](#ghostmemorymanager)
  - [GhostAllocator](#ghostallocator)
- [Memory States](#memory-states)
- [Configuration Constants](#configuration-constants)
- [Version Information](#version-information)

---

## Core Classes

### GhostMemoryManager

**Header:** `ghostmem/GhostMemoryManager.h`

The central singleton class that manages virtual memory allocation, page fault handling, compression/decompression, and LRU eviction.

#### Public Methods

##### `static GhostMemoryManager& Instance()`
Gets the singleton instance of GhostMemoryManager.

**Returns:** Reference to the singleton instance.

**Thread Safety:** Thread-safe. First call initializes the instance.

**Example:**
```cpp
auto& manager = GhostMemoryManager::Instance();
```

---

##### `void* AllocateGhost(size_t size)`
Allocates virtual memory that will be managed by GhostMem's compression system.

**Parameters:**
- `size`: Number of bytes to allocate (will be rounded up to page boundaries)

**Returns:** 
- Pointer to allocated memory on success
- `nullptr` on failure

**Thread Safety:** Thread-safe with internal mutex locking.

**Behavior:**
1. Rounds `size` up to nearest page boundary (4096 bytes)
2. Reserves virtual address space (no physical RAM committed initially)
3. Registers the memory block in internal tracking structures
4. Physical RAM is committed on first access (via page fault)

**Example:**
```cpp
// Allocate 8KB of ghost memory (2 pages)
void* ptr = GhostMemoryManager::Instance().AllocateGhost(8192);
if (ptr != nullptr) {
    int* data = static_cast<int*>(ptr);
    data[0] = 42;  // First access triggers page fault and commits RAM
}
```

**Memory Layout:**
```
Virtual Address Space:  [=========== size bytes ===========]
Physical RAM:           [not allocated until accessed]
After first access:     [== 4KB page ==][remaining pages...]
```

---

##### `void DeallocateGhost(void* ptr)` ⚠️
Deallocates ghost memory.

**Parameters:**
- `ptr`: Pointer previously returned by `AllocateGhost()`

**Thread Safety:** Thread-safe with internal mutex locking.

**Status:** ⚠️ **Currently not fully implemented** - stub exists but memory is not fully released. Planned for future version.

---

#### Internal Methods (Advanced Users)

##### `void EvictLRUPage()`
Evicts the least recently used page from physical RAM.

**Thread Safety:** Must be called with mutex already held.

**Behavior:**
1. Identifies least recently used page from active list
2. Compresses page data using LZ4
3. Stores compressed data in backing store
4. Decommits physical RAM for that page
5. Moves page to frozen state

**Called automatically when:**
- Physical page limit (`MAX_PHYSICAL_PAGES`) is reached
- New page needs to be committed to RAM

---

##### `void RestorePage(void* page_addr)`
Restores a previously evicted page to physical RAM.

**Parameters:**
- `page_addr`: Address of the frozen page to restore

**Thread Safety:** Must be called with mutex already held.

**Behavior:**
1. Finds compressed data in backing store
2. Decompresses using LZ4
3. Commits physical RAM for the page
4. Copies decompressed data back
5. Updates page state to active
6. May trigger eviction if at physical page limit

**Called automatically by:**
- Page fault handlers when accessing frozen pages

---

#### Platform-Specific Handlers

##### Windows: `static LONG WINAPI VectoredHandler(PEXCEPTION_POINTERS pExceptionInfo)`
Vectored exception handler for Windows page faults.

**Registered at:** Startup via `AddVectoredExceptionHandler()`

**Handles:** `EXCEPTION_ACCESS_VIOLATION` exceptions

**Behavior:**
1. Checks if fault address is in managed ghost memory range
2. If yes: Restores the page and returns `EXCEPTION_CONTINUE_EXECUTION`
3. If no: Returns `EXCEPTION_CONTINUE_SEARCH` (let OS handle it)

---

##### Linux: `static void SignalHandler(int sig, siginfo_t* info, void* context)`
POSIX signal handler for Linux page faults.

**Registered at:** Startup via `sigaction(SIGSEGV, ...)`

**Handles:** `SIGSEGV` signals

**Behavior:**
1. Checks if fault address is in managed ghost memory range
2. If yes: Restores the page
3. If no: Terminates process (fault not from ghost memory)

---

#### Internal Data Structures

##### `std::unordered_map<void*, size_t> managed_blocks`
Tracks all allocated ghost memory blocks.

**Key:** Starting address of allocation  
**Value:** Size in bytes

---

##### `std::list<void*> active_pages`
LRU list of currently active (uncompressed) pages in physical RAM.

**Front:** Most recently used  
**Back:** Least recently used (first to be evicted)

---

##### `std::unordered_map<void*, std::vector<char>> backing_store`
Compressed storage for evicted pages.

**Key:** Page address  
**Value:** Compressed page data (LZ4 compressed)

---

##### `std::recursive_mutex mutex_`
Protects all internal data structures.

**Type:** `std::recursive_mutex` (allows same thread to lock multiple times)

**Locked by:**
- `AllocateGhost()`
- `DeallocateGhost()`
- Page fault handlers (via `VectoredHandler` / `SignalHandler`)

---

### GhostAllocator<T>

**Header:** `ghostmem/GhostAllocator.h`

STL-compatible allocator that uses GhostMemoryManager for allocation.

#### Template Parameters
- `T`: Type of objects to allocate

#### Type Definitions

```cpp
using value_type = T;
using size_type = std::size_t;
using difference_type = std::ptrdiff_t;
using pointer = T*;
using const_pointer = const T*;
using reference = T&;
using const_reference = const T&;

template<typename U>
struct rebind {
    using other = GhostAllocator<U>;
};
```

---

#### Public Methods

##### `GhostAllocator() noexcept`
Default constructor.

**Example:**
```cpp
GhostAllocator<int> alloc;
```

---

##### `template<typename U> GhostAllocator(const GhostAllocator<U>&) noexcept`
Copy constructor for different types (rebinding).

**Example:**
```cpp
GhostAllocator<int> alloc1;
GhostAllocator<double> alloc2(alloc1);  // Rebinding
```

---

##### `T* allocate(std::size_t n)`
Allocates memory for `n` objects of type `T`.

**Parameters:**
- `n`: Number of objects to allocate space for

**Returns:** Pointer to allocated memory

**Throws:** `std::bad_alloc` if allocation fails

**Behavior:**
1. Calculates bytes needed: `n * sizeof(T)`
2. Calls `GhostMemoryManager::Instance().AllocateGhost(bytes)`
3. Returns pointer cast to `T*`

**Example:**
```cpp
GhostAllocator<int> alloc;
int* ptr = alloc.allocate(100);  // Space for 100 integers
```

---

##### `void deallocate(T* ptr, std::size_t n)`
Deallocates memory previously allocated with `allocate()`.

**Parameters:**
- `ptr`: Pointer to deallocate
- `n`: Number of objects (currently unused)

**Example:**
```cpp
alloc.deallocate(ptr, 100);
```

---

#### Usage with STL Containers

##### std::vector
```cpp
#include "ghostmem/GhostAllocator.h"
#include <vector>

std::vector<int, GhostAllocator<int>> vec;
vec.push_back(42);
```

##### std::string
```cpp
#include "ghostmem/GhostAllocator.h"
#include <string>

using GhostString = std::basic_string<char, std::char_traits<char>, GhostAllocator<char>>;
GhostString str = "Hello, GhostMem!";
```

##### std::map
```cpp
#include "ghostmem/GhostAllocator.h"
#include <map>

using GhostMap = std::map<int, std::string, 
                          std::less<int>,
                          GhostAllocator<std::pair<const int, std::string>>>;
GhostMap map;
map[1] = "one";
```

##### std::list
```cpp
#include "ghostmem/GhostAllocator.h"
#include <list>

std::list<double, GhostAllocator<double>> list;
list.push_back(3.14);
```

---

## Memory States

Ghost memory pages transition through three states:

### 1. Reserved
**Description:** Virtual address space allocated, no physical RAM used.

**Characteristics:**
- Returned by `AllocateGhost()`
- Zero physical memory footprint
- Accessing triggers page fault

**Transitions to:** Committed when first accessed

---

### 2. Committed (Active)
**Description:** Page backed by physical RAM, actively in use.

**Characteristics:**
- Listed in `active_pages` LRU
- Consumes physical RAM
- Fast access (no page fault)
- Moved to front of LRU on each access

**Transitions to:** Frozen when evicted (LRU reaches limit)

---

### 3. Frozen
**Description:** Page compressed and stored in backing store, physical RAM freed.

**Characteristics:**
- Stored in `backing_store` map (compressed)
- Zero physical RAM footprint
- Accessing triggers restoration
- LZ4 compressed data in process memory

**Transitions to:** Committed when accessed again

---

### State Diagram

```
┌───────────┐
│           │  AllocateGhost()
│ Reserved  │◄───────────────────────┐
│           │                        │
└─────┬─────┘                        │
      │                              │
      │ First Access                 │
      │ (Page Fault)                 │
      │                              │
┌─────▼─────┐                        │
│           │                        │
│ Committed │                        │
│ (Active)  │                        │
│           │                        │
└─────┬─────┘                        │
      │                              │
      │ LRU Eviction                 │
      │ (Compress + Decommit)        │
      │                              │
┌─────▼─────┐                        │
│           │                        │
│  Frozen   │                        │
│(Compressed)                        │
│           │                        │
└─────┬─────┘                        │
      │                              │
      │ Subsequent Access            │
      │ (Page Fault + Decompress)    │
      │                              │
      └──────────────────────────────┘
```

---

## Configuration Constants

**File:** `src/ghostmem/GhostMemoryManager.h`

### `PAGE_SIZE`
```cpp
const size_t PAGE_SIZE = 4096;
```

**Description:** Size of memory pages in bytes.

**Default:** 4096 (4KB, typical for x86/x64)

**Usage:** All allocations are rounded up to page boundaries.

**Modify when:** Porting to architectures with different page sizes (rare).

---

### `MAX_PHYSICAL_PAGES`
```cpp
const size_t MAX_PHYSICAL_PAGES = 5;
```

**Description:** Maximum number of pages that can be simultaneously active (uncompressed) in physical RAM.

**Default:** 5 pages = 20KB of physical RAM

**Usage:** Controls the physical memory limit. When exceeded, LRU page is evicted.

**Tune for your workload:**
- **Low memory systems (IoT):** 3-10 pages
- **Desktop applications:** 100-1000 pages
- **Server applications:** 1000-10000 pages

**Formula:**
```
Physical RAM used = MAX_PHYSICAL_PAGES × PAGE_SIZE
Effective virtual RAM = (allocated pages) × (compression ratio)
```

**Example tuning:**
```cpp
// IoT device with 16MB RAM, allow 1MB for ghost memory
const size_t MAX_PHYSICAL_PAGES = 256;  // 256 × 4KB = 1MB

// Desktop app with 8GB RAM, allow 100MB for ghost memory
const size_t MAX_PHYSICAL_PAGES = 25600;  // 25600 × 4KB = 100MB
```

---

## Version Information

**Header:** `ghostmem/Version.h`

### Constants

```cpp
#define GHOSTMEM_VERSION_MAJOR 0
#define GHOSTMEM_VERSION_MINOR 9
#define GHOSTMEM_VERSION_PATCH 0
```

### Functions

##### `int GetVersionMajor()`
Returns the major version number.

##### `int GetVersionMinor()`
Returns the minor version number.

##### `int GetVersionPatch()`
Returns the patch version number.

##### `const char* GetVersionString()`
Returns the version as a string (e.g., "0.9.0").

##### `int GetVersionNumber()`
Returns the version as an integer (e.g., 900 for version 0.9.0).

**Example:**
```cpp
#include "ghostmem/Version.h"

std::cout << "GhostMem version: " 
          << GhostMem::GetVersionString() << std::endl;
```

---

## Error Handling

### Allocation Failures
`AllocateGhost()` returns `nullptr` when:
- Virtual memory reservation fails (OS limit reached)
- Out of address space

**Check for nullptr:**
```cpp
void* ptr = GhostMemoryManager::Instance().AllocateGhost(size);
if (ptr == nullptr) {
    // Handle allocation failure
    throw std::bad_alloc();
}
```

### Page Fault Handling
Page faults are handled internally:
- **Windows:** Vectored exception handler
- **Linux:** SIGSEGV signal handler

If a page fault occurs outside ghost memory ranges:
- **Windows:** Exception continues to other handlers
- **Linux:** Process terminates with SIGSEGV

---

## Performance Considerations

### When to Use GhostMem

✅ **Good use cases:**
- Data with temporal locality (hot/cold access patterns)
- Compressible data (text, repeated values, sparse matrices)
- Memory-constrained environments
- Avoiding disk I/O

❌ **Poor use cases:**
- Uniform random access patterns (no cold data to compress)
- Already compressed data (JPEG, MP3, encrypted data)
- Real-time systems requiring guaranteed latency
- Small allocations (< 1 page overhead)

### Optimization Tips

1. **Batch operations:** Access related data together to keep pages hot
2. **Tune MAX_PHYSICAL_PAGES:** Balance between physical RAM usage and page fault frequency
3. **Data locality:** Structure data for sequential access when possible
4. **Compression-friendly formats:** Use plain text, uncompressed formats where possible

---

## Platform Differences

| Feature | Windows | Linux |
|---------|---------|-------|
| Virtual Memory API | VirtualAlloc/VirtualFree | mmap/munmap |
| Page Fault Handler | Vectored Exception Handler | SIGSEGV Signal Handler |
| Page Protection | PAGE_NOACCESS / PAGE_READWRITE | PROT_NONE / PROT_READ\|PROT_WRITE |
| Thread Safety | Yes (recursive_mutex) | Yes (recursive_mutex) |
| Signal Safety | N/A | ⚠️ Limited (mutexes not async-signal-safe) |

---

## See Also

- [Integration Guide](INTEGRATION_GUIDE.md) - How to use GhostMem in your project
- [Thread Safety](THREAD_SAFETY.md) - Detailed thread safety documentation
- [Main README](../README.md) - Project overview and quick start
