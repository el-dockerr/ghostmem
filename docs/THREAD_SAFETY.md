# GhostMem Thread Safety Documentation

## Table of Contents
- [Overview](#overview)
- [Thread Safety Guarantees](#thread-safety-guarantees)
- [Synchronization Mechanisms](#synchronization-mechanisms)
- [Multi-threaded Usage Patterns](#multi-threaded-usage-patterns)
- [Performance in Multi-threaded Context](#performance-in-multi-threaded-context)
- [Platform-Specific Considerations](#platform-specific-considerations)
- [Best Practices](#best-practices)
- [Known Limitations](#known-limitations)

---

## Overview

GhostMem is designed to be **thread-safe** for concurrent memory allocations, deallocations, and page fault handling across multiple threads. The implementation uses a recursive mutex to protect all internal data structures.

### Thread Safety Level: **Full**

✅ **Safe operations:**
- Concurrent allocations from multiple threads
- Concurrent access to different ghost memory blocks
- Concurrent access to same ghost memory block (reads)
- Page fault handling in multi-threaded environment
- STL containers with GhostAllocator in multiple threads

⚠️ **Not safe without external synchronization:**
- Concurrent writes to same memory location (standard data race)
- Concurrent modifications of same STL container (standard STL rules apply)

---

## Thread Safety Guarantees

### GhostMemoryManager Methods

| Method | Thread Safe? | Locking |
|--------|--------------|---------|
| `Instance()` | ✅ Yes | Static initialization |
| `AllocateGhost()` | ✅ Yes | Internal mutex |
| `DeallocateGhost()` | ✅ Yes | Internal mutex |
| `EvictLRUPage()` | ⚠️ Internal only | Requires mutex held |
| `RestorePage()` | ⚠️ Internal only | Requires mutex held |

### GhostAllocator Methods

| Method | Thread Safe? | Notes |
|--------|--------------|-------|
| `allocate()` | ✅ Yes | Calls `AllocateGhost()` |
| `deallocate()` | ✅ Yes | Calls `DeallocateGhost()` |
| Construction/Copy | ✅ Yes | Stateless |

### Page Fault Handlers

| Handler | Thread Safe? | Platform | Notes |
|---------|--------------|----------|-------|
| `VectoredHandler()` | ✅ Yes | Windows | Per-thread exception context |
| `SignalHandler()` | ⚠️ Limited | Linux | Signal handlers have restrictions |

---

## Synchronization Mechanisms

### 1. Recursive Mutex

```cpp
class GhostMemoryManager {
private:
    std::recursive_mutex mutex_;  // Protects all internal state
};
```

**Why recursive?**
- Allows same thread to acquire lock multiple times
- Needed because page fault handlers may be called while thread already holds lock
- Example: Thread calls `AllocateGhost()` → triggers page fault → handler needs lock

**Protected resources:**
- `managed_blocks` - Map of allocated memory blocks
- `active_pages` - LRU list of committed pages
- `backing_store` - Compressed page storage
- LRU ordering operations

---

### 2. Locking Strategy

#### AllocateGhost()
```cpp
void* GhostMemoryManager::AllocateGhost(size_t size) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);  // Acquire lock
    
    // Reserve virtual memory (thread-safe with lock)
    void* ptr = VirtualAlloc(...);  // Windows
    // or
    void* ptr = mmap(...);          // Linux
    
    // Register in managed_blocks (protected)
    managed_blocks[ptr] = rounded_size;
    
    return ptr;
    // Lock released automatically
}
```

#### Page Fault Handler (Windows)
```cpp
LONG WINAPI GhostMemoryManager::VectoredHandler(PEXCEPTION_POINTERS pExceptionInfo) {
    // Get fault address
    ULONG_PTR fault_addr = pExceptionInfo->ExceptionRecord->ExceptionInformation[1];
    auto& manager = Instance();
    
    std::lock_guard<std::recursive_mutex> lock(manager.mutex_);  // Acquire lock
    
    // Check if our address and restore if needed (protected)
    for (const auto& block : manager.managed_blocks) {
        // ... restoration logic ...
    }
    
    return EXCEPTION_CONTINUE_EXECUTION;
    // Lock released automatically
}
```

---

## Multi-threaded Usage Patterns

### Pattern 1: Independent Allocations

**Scenario:** Each thread allocates its own ghost memory.

```cpp
#include "ghostmem/GhostAllocator.h"
#include <thread>
#include <vector>

void worker_thread(int thread_id) {
    // Each thread has its own vector
    std::vector<int, GhostAllocator<int>> local_data;
    
    for (int i = 0; i < 100000; i++) {
        local_data.push_back(thread_id * 100000 + i);
    }
    
    // Process data...
}

int main() {
    std::vector<std::thread> threads;
    
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(worker_thread, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
}
```

**Thread safety:** ✅ Fully safe - no shared state between threads.

---

### Pattern 2: Shared Read-Only Access

**Scenario:** Multiple threads read from same ghost memory.

```cpp
#include "ghostmem/GhostAllocator.h"
#include <thread>
#include <vector>

void reader_thread(const std::vector<int, GhostAllocator<int>>& shared_data) {
    // Multiple threads reading same data - safe
    int sum = 0;
    for (size_t i = 0; i < shared_data.size(); i++) {
        sum += shared_data[i];
    }
    std::cout << "Sum: " << sum << std::endl;
}

int main() {
    // Prepare shared data
    std::vector<int, GhostAllocator<int>> shared_data;
    for (int i = 0; i < 1000000; i++) {
        shared_data.push_back(i);
    }
    
    // Multiple readers
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(reader_thread, std::cref(shared_data));
    }
    
    for (auto& t : threads) {
        t.join();
    }
}
```

**Thread safety:** ✅ Safe - concurrent reads are allowed, page faults handled safely.

---

### Pattern 3: Producer-Consumer with External Synchronization

**Scenario:** One thread writes, others read - needs external sync.

```cpp
#include "ghostmem/GhostAllocator.h"
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>

std::vector<int, GhostAllocator<int>> shared_queue;
std::mutex queue_mutex;
std::condition_variable cv;
bool done = false;

void producer() {
    for (int i = 0; i < 1000; i++) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            shared_queue.push_back(i);
        }
        cv.notify_one();
    }
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        done = true;
    }
    cv.notify_all();
}

void consumer(int id) {
    while (true) {
        std::unique_lock<std::mutex> lock(queue_mutex);
        cv.wait(lock, [] { return !shared_queue.empty() || done; });
        
        if (shared_queue.empty() && done) break;
        
        if (!shared_queue.empty()) {
            int value = shared_queue.back();
            shared_queue.pop_back();
            lock.unlock();
            
            // Process value...
            std::cout << "Consumer " << id << " got: " << value << std::endl;
        }
    }
}

int main() {
    std::thread prod(producer);
    std::thread cons1(consumer, 1);
    std::thread cons2(consumer, 2);
    
    prod.join();
    cons1.join();
    cons2.join();
}
```

**Thread safety:** ✅ Safe - external mutex protects container modifications, GhostMem handles memory safely.

---

### Pattern 4: Concurrent Allocations from Multiple Threads

**Scenario:** High-frequency allocations from many threads.

```cpp
#include "ghostmem/GhostMemoryManager.h"
#include <thread>
#include <vector>
#include <atomic>

std::atomic<int> success_count{0};

void allocator_thread() {
    for (int i = 0; i < 10; i++) {
        void* ptr = GhostMemoryManager::Instance().AllocateGhost(4096);
        if (ptr != nullptr) {
            // Use the memory
            int* data = static_cast<int*>(ptr);
            data[0] = 42;
            
            if (data[0] == 42) {
                success_count++;
            }
        }
    }
}

int main() {
    const int NUM_THREADS = 8;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(allocator_thread);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "Successful allocations: " << success_count << std::endl;
}
```

**Thread safety:** ✅ Fully safe - mutex protects internal state.

---

## Performance in Multi-threaded Context

### Lock Contention

**Potential bottleneck:** All threads contend for the same `mutex_`.

**Impact on performance:**

| Scenario | Contention | Performance |
|----------|------------|-------------|
| Few allocations, many accesses | Low | ⭐⭐⭐⭐⭐ Excellent |
| Many allocations, few threads | Low | ⭐⭐⭐⭐ Good |
| Many allocations, many threads | High | ⭐⭐⭐ Moderate |
| Constant thrashing (page faults) | Very High | ⭐⭐ Poor |

### Optimization Strategies

#### 1. Thread-Local Allocations
```cpp
// Each thread allocates once, uses many times
thread_local std::vector<int, GhostAllocator<int>> thread_buffer;

void worker() {
    thread_buffer.reserve(10000);  // One allocation
    
    for (int i = 0; i < 10000; i++) {
        thread_buffer.push_back(i);  // No lock after initial allocation
    }
}
```

#### 2. Batch Operations
```cpp
// Bad: Many small allocations
for (int i = 0; i < 1000; i++) {
    void* ptr = AllocateGhost(4096);  // 1000 mutex acquisitions
}

// Good: One large allocation
void* ptr = AllocateGhost(4096 * 1000);  // 1 mutex acquisition
```

#### 3. Increase MAX_PHYSICAL_PAGES
```cpp
// Reduce page fault frequency → less lock contention
const size_t MAX_PHYSICAL_PAGES = 1000;  // vs default 5
```

---

## Platform-Specific Considerations

### Windows (Vectored Exception Handler)

✅ **Thread-safe exception handling:**
- Each thread has its own exception context
- Handler registered process-wide but invoked per-thread
- No special considerations needed

```cpp
LONG WINAPI GhostMemoryManager::VectoredHandler(PEXCEPTION_POINTERS pExceptionInfo) {
    // pExceptionInfo is thread-specific
    // Safe to access without additional synchronization
}
```

---

### Linux (Signal Handler)

⚠️ **Signal handler limitations:**

POSIX signal handlers have restrictions:
1. Only async-signal-safe functions should be called
2. Mutexes are **NOT async-signal-safe**
3. GhostMem uses mutexes in signal handler anyway

**Why it works in practice:**
- Page faults are synchronous signals (triggered by faulting thread)
- Signal delivered to same thread that caused fault
- No re-entrancy from same thread (fault is resolved before returning)
- Modern Linux implementations are more permissive

**Potential issues:**
- If another signal arrives while handler is running
- If handler is interrupted by async signal (SIGINT, SIGTERM, etc.)

**Mitigation:**
```cpp
void GhostMemoryManager::InstallSignalHandler() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = SignalHandler;
    sigemptyset(&sa.sa_mask);
    
    // Block other signals during handler execution
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGTERM);
    
    sigaction(SIGSEGV, &sa, nullptr);
}
```

---

## Best Practices

### ✅ DO: Use Thread-Local Storage

```cpp
thread_local std::vector<int, GhostAllocator<int>> tls_buffer;

void thread_function() {
    // Each thread has its own buffer - minimal contention
    tls_buffer.push_back(42);
}
```

### ✅ DO: Pre-allocate in Single Thread

```cpp
std::vector<int, GhostAllocator<int>> shared_data;

// Allocate in main thread
shared_data.reserve(1000000);
for (int i = 0; i < 1000000; i++) {
    shared_data.push_back(i);
}

// Then use in multiple threads (read-only)
std::vector<std::thread> readers;
for (int i = 0; i < 4; i++) {
    readers.emplace_back([&shared_data]() {
        for (auto value : shared_data) {
            // Read is safe
        }
    });
}
```

### ✅ DO: Use External Synchronization for Container Modifications

```cpp
std::vector<int, GhostAllocator<int>> shared_vec;
std::mutex vec_mutex;

void thread_func() {
    std::lock_guard<std::mutex> lock(vec_mutex);
    shared_vec.push_back(42);  // Protected by external mutex
}
```

### ❌ DON'T: Rapidly Allocate/Deallocate Across Threads

```cpp
// Bad: High lock contention
void bad_pattern() {
    for (int i = 0; i < 10000; i++) {
        void* ptr = AllocateGhost(4096);
        // ... use briefly ...
        DeallocateGhost(ptr);
    }
}
```

### ❌ DON'T: Assume No Data Races

```cpp
// Bad: Data race on shared memory
std::vector<int, GhostAllocator<int>> shared;

void thread1() {
    shared[100] = 42;  // Data race!
}

void thread2() {
    shared[100] = 99;  // Data race!
}
```

---

## Known Limitations

### 1. Global Mutex Contention

**Issue:** Single global mutex protects all operations.

**Impact:** High-frequency allocations from many threads may contend.

**Future improvement:** Sharded locks per address range.

---

### 2. Signal Handler Safety (Linux)

**Issue:** Mutex in signal handler not async-signal-safe per POSIX.

**Status:** Works in practice but not guaranteed by standard.

**Mitigation:** 
- Use on known platforms (Linux 2.6+, modern glibc)
- Test thoroughly with your workload
- Consider platform-specific signal blocking

---

### 3. No Lock-Free Operations

**Issue:** Even simple queries need mutex.

**Impact:** Some overhead for read-heavy workloads.

**Workaround:** Cache information outside GhostMem when possible.

---

### 4. Page Fault Handler Serialization

**Issue:** Page faults are handled serially (one at a time).

**Impact:** Multiple threads faulting simultaneously will wait.

**Mitigation:** Increase `MAX_PHYSICAL_PAGES` to reduce fault frequency.

---

## Testing Thread Safety

### Example Test Case

```cpp
#include "ghostmem/GhostAllocator.h"
#include <thread>
#include <vector>
#include <atomic>

void test_concurrent_allocations() {
    const int NUM_THREADS = 8;
    const int ALLOCS_PER_THREAD = 100;
    
    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;
    
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&success_count]() {
            for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
                void* ptr = GhostMemoryManager::Instance().AllocateGhost(4096);
                if (ptr != nullptr) {
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
    
    assert(success_count == NUM_THREADS * ALLOCS_PER_THREAD);
    std::cout << "Thread safety test passed!" << std::endl;
}
```

---

## See Also

- [API Reference](API_REFERENCE.md) - Detailed API documentation
- [Integration Guide](INTEGRATION_GUIDE.md) - How to use GhostMem
- [Main README](../README.md) - Project overview
