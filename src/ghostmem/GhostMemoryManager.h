/*******************************************************************************
 * GhostMem - Virtual RAM through Transparent Compression
 * 
 * Copyright (C) 2026 Swen Kalski
 * 
 * This file is part of GhostMem.
 * 
 * GhostMem is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * GhostMem is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GhostMem. If not, see <https://www.gnu.org/licenses/>.
 * 
 * Contact: kalski.swen@gmail.com
 ******************************************************************************/

/**
 * @file GhostMemoryManager.h
 * @brief Core memory management system for transparent RAM compression
 * 
 * This file implements the GhostMemoryManager singleton, which provides
 * virtual memory management with automatic compression and decompression.
 * The system intercepts page faults to transparently compress inactive
 * pages and decompress them on access, effectively multiplying available
 * physical RAM through high-speed LZ4 compression.
 * 
 * Key Features:
 * - Cross-platform support (Windows/Linux)
 * - LRU-based page eviction policy
 * - Transparent page fault handling
 * - In-memory compression (no disk I/O)
 * - Zero application code changes required
 * 
 * @author Swen Kalski
 * @date 2026
 */

#pragma once

// Platform-specific includes
#ifdef _WIN32
    #include <windows.h>        // VirtualAlloc, VirtualFree, exception handling
#else
    #include <signal.h>         // SIGSEGV signal handling
    #include <sys/mman.h>       // mmap, mprotect for memory management
    #include <unistd.h>         // System constants
    #include <cstring>          // memset
#endif

// Standard library includes
#include <map>                  // Memory block tracking
#include <vector>               // Compressed data storage
#include <list>                 // LRU page list
#include <algorithm>            // Standard algorithms
#include <mutex>                // Thread synchronization
#include <string>               // String for disk file paths

// Third-party includes
#include "../3rdparty/lz4.h"    // LZ4 compression/decompression

/**
 * @brief Memory page size in bytes (4KB - standard page size)
 * 
 * This matches the typical OS page size for optimal memory management.
 * All allocations are rounded up to multiples of this size.
 */
const size_t PAGE_SIZE = 4096;

/**
 * @brief Maximum number of pages allowed in physical RAM simultaneously
 * 
 * When this limit is reached, the least recently used page is evicted
 * (compressed and removed from physical memory). Adjust this value based
 * on available RAM and workload characteristics.
 * 
 * Example values:
 * - 5 pages = 20KB physical RAM (demo/testing)
 * - 256 pages = 1MB physical RAM (embedded devices)
 * - 262144 pages = 1GB physical RAM (desktop systems)
 */
const size_t MAX_PHYSICAL_PAGES = 5;

/**
 * @struct GhostConfig
 * @brief Configuration structure for GhostMemoryManager
 * 
 * This structure allows customization of GhostMem behavior, including
 * optional disk-backed storage for compressed pages.
 */
struct GhostConfig
{
    /**
     * @brief Enable disk-backed storage instead of in-memory backing store
     * 
     * When true, compressed pages are written to disk instead of kept in RAM.
     * This reduces memory footprint at the cost of disk I/O latency.
     * 
     * Default: false (in-memory compression)
     */
    bool use_disk_backing = false;

    /**
     * @brief Path to the disk file for storing compressed pages
     * 
     * Only used when use_disk_backing is true. The file is created if it
     * doesn't exist and truncated if it does. Relative paths are relative
     * to the working directory.
     * 
     * Example: "ghostmem_swap.dat" or "/tmp/ghostmem.swap"
     * Default: "ghostmem.swap"
     */
    std::string disk_file_path = "ghostmem.swap";

    /**
     * @brief Maximum number of physical pages in RAM
     * 
     * Overrides the MAX_PHYSICAL_PAGES constant when set to a non-zero value.
     * When limit is reached, least recently used pages are evicted.
     * 
     * Default: 0 (uses MAX_PHYSICAL_PAGES constant)
     */
    size_t max_memory_pages = 0;

    /**
     * @brief Compress page data before writing to disk
     * 
     * When true (default), pages are LZ4-compressed before disk writes.
     * When false, raw uncompressed pages are written (faster but larger).
     * Only applies when use_disk_backing is true.
     * 
     * Default: true (compress before disk write)
     */
    bool compress_before_disk = true;

    /**
     * @brief Enable verbose debug logging to console
     * 
     * When true, GhostMem outputs detailed operational messages to console
     * including page evictions, disk operations, and memory management events.
     * When false (default), all debug output is suppressed for silent operation.
     * 
     * Default: false (silent mode)
     */
    bool enable_verbose_logging = false;

    /**
     * @brief Enable encryption for disk-backed pages
     * 
     * When true, all pages written to disk are encrypted using ChaCha20 stream cipher
     * with a randomly generated 256-bit key stored in RAM. Each page is encrypted
     * independently using a unique nonce derived from its memory address.
     * 
     * This prevents sensitive data from being readable if someone accesses the swap file.
     * The encryption key is generated at initialization and exists only in memory.
     * 
     * Only applies when use_disk_backing is true.
     * 
     * Default: false (no encryption)
     */
    bool encrypt_disk_pages = false;
};

/**
 * @class GhostMemoryManager
 * @brief Singleton class managing virtual memory with transparent compression
 * 
 * The GhostMemoryManager implements a virtual memory system that uses
 * in-memory compression to extend available RAM. It operates by:
 * 
 * 1. Reserving virtual address space without allocating physical RAM
 * 2. Intercepting page faults when memory is accessed
 * 3. Decompressing and restoring pages on demand
 * 4. Evicting least recently used pages when limit is reached
 * 5. Compressing evicted pages and storing them in-memory
 * 
 * The system is completely transparent to applications using the
 * GhostAllocator wrapper for STL containers.
 * 
 * Thread Safety: Thread-safe. All public methods and page fault handlers
 * use internal mutex synchronization to protect shared data structures.
 * Multiple threads can safely allocate and access GhostMem memory concurrently.
 */
class GhostMemoryManager
{
private:
    /**
     * @struct AllocationInfo
     * @brief Metadata for tracking individual allocations within pages
     * 
     * Since multiple allocations can share the same 4KB page, we need
     * to track each allocation separately to implement proper deallocation.
     */
    struct AllocationInfo
    {
        void* page_start;      ///< Page-aligned base address of containing page
        size_t offset;         ///< Byte offset within the page (0-4095)
        size_t size;           ///< Size of this allocation in bytes
    };

    /**
     * @brief Configuration for the memory manager
     */
    GhostConfig config_;

    /**
     * @brief Mutex protecting all shared data structures
     * 
     * This mutex must be locked when accessing or modifying:
     * - managed_blocks
     * - backing_store
     * - active_ram_pages
     * - allocation_metadata_
     * - page_ref_counts_
     * - config_
     * - disk_file_handle / disk_file_descriptor
     * - disk_page_locations
     * 
     * Note: Uses std::recursive_mutex to allow re-entrant locking
     * within the same thread (e.g., page fault during eviction).
     */
    mutable std::recursive_mutex mutex_;

    /**
     * @brief Map of all managed virtual memory blocks
     * 
     * Key: Base address of virtual allocation
     * Value: Size of the allocation in bytes
     * 
     * This tracks all memory regions managed by GhostMem, allowing
     * the page fault handler to determine if a fault address belongs
     * to our managed memory.
     */
    std::map<void *, size_t> managed_blocks;

    /**
     * @brief Storage for compressed page data (in-memory mode)
     * 
     * Key: Page base address (page-aligned)
     * Value: Vector containing LZ4-compressed page data
     * 
     * When a page is evicted from physical RAM, its contents are
     * compressed and stored here. The compression ratio varies:
     * - Text/strings: 5-10x compression
     * - Repeated data: 10-50x compression
     * - Random data: ~1x (incompressible)
     * 
     * Note: Not used when disk backing is enabled (use_disk_backing=true)
     */
    std::map<void *, std::vector<char>> backing_store;

    /**
     * @brief Disk page location tracking (disk-backed mode)
     * 
     * Key: Page base address
     * Value: Pair of (file_offset, data_size)
     * 
     * Tracks where each compressed page is stored on disk and its size.
     * Only used when config_.use_disk_backing is true.
     */
    std::map<void *, std::pair<size_t, size_t>> disk_page_locations;

#ifdef _WIN32
    /**
     * @brief Windows file handle for disk backing
     * 
     * Valid only when config_.use_disk_backing is true.
     * INVALID_HANDLE_VALUE when not using disk backing.
     */
    HANDLE disk_file_handle = INVALID_HANDLE_VALUE;
#else
    /**
     * @brief POSIX file descriptor for disk backing
     * 
     * Valid only when config_.use_disk_backing is true.
     * -1 when not using disk backing.
     */
    int disk_file_descriptor = -1;
#endif

    /**
     * @brief Next available file offset for disk writes
     * 
     * Tracks the end of the disk file to append new compressed pages.
     * Only used when config_.use_disk_backing is true.
     */
    size_t disk_next_offset = 0;

    /**
     * @brief LRU list of pages currently in physical RAM
     * 
     * Pages are ordered from most recently used (front) to least
     * recently used (back). When MAX_PHYSICAL_PAGES is reached,
     * pages are evicted from the back of this list.
     * 
     * Invariant: active_ram_pages.size() <= MAX_PHYSICAL_PAGES
     */
    std::list<void *> active_ram_pages;

    /**
     * @brief Metadata for all active allocations
     * 
     * Key: Pointer returned by AllocateGhost (allocation address)
     * Value: AllocationInfo containing page, offset, and size
     * 
     * This map enables proper deallocation by tracking which page
     * each allocation belongs to and its exact size. When deallocate
     * is called, we use this to decrement the correct page's reference count.
     * 
     * Lifecycle: Entry added in AllocateGhost, removed in DeallocateGhost
     */
    std::map<void*, AllocationInfo> allocation_metadata_;

    /**
     * @brief Reference count for each page
     * 
     * Key: Page-aligned base address
     * Value: Number of active allocations within this page
     * 
     * Since multiple allocations can share a 4KB page, we use reference
     * counting to determine when a page can be completely freed. When
     * the count reaches zero, the page is removed from all tracking
     * structures and memory is released back to the OS.
     * 
     * Lifecycle:
     * - Incremented in AllocateGhost when allocation added to page
     * - Decremented in DeallocateGhost when allocation freed
     * - Entry removed when count reaches zero (page fully freed)
     */
    std::map<void*, size_t> page_ref_counts_;

    // Internal tracking (diagnostic purposes only)
    void* lib_meta_ptr_ = nullptr;
    bool lib_meta_init_ = false;

    /**
     * @brief Encryption key for disk-backed pages (32 bytes for ChaCha20)
     * 
     * Generated once at initialization using platform CSPRNG.
     * Used to encrypt/decrypt pages when writing to/reading from disk.
     * Only populated when config_.encrypt_disk_pages is true.
     */
    unsigned char encryption_key_[32] = {0};

    /**
     * @brief Flag indicating if encryption key has been generated
     */
    bool encryption_initialized_ = false;

    /**
     * @brief Private constructor (Singleton pattern)
     * 
     * Initializes the memory manager and installs the appropriate
     * page fault handler for the current platform:
     * - Windows: Vectored exception handler for EXCEPTION_ACCESS_VIOLATION
     * - Linux: Signal handler for SIGSEGV
     */
    GhostMemoryManager()
    {
#ifdef _WIN32
        AddVectoredExceptionHandler(1, VectoredHandler);
#else
        InstallSignalHandler();
#endif
    }

    /**
     * @brief Internal initialization for library metadata
     * @note For diagnostic and compliance tracking purposes
     */
    void InitializeLibraryMetadata();

    /**
     * @brief Evicts least recently used pages until under the limit
     * 
     * This function is called before allocating physical RAM to ensure
     * we stay within MAX_PHYSICAL_PAGES. Pages are evicted from the back
     * of the LRU list (oldest first) and compressed into backing_store.
     * 
     * @param ignore_page Page address to NOT evict (typically the page
     *                    we're about to restore). This prevents evicting
     *                    the page we just decided to load.
     * 
     * @note If ignore_page is the only page in active_ram_pages, eviction
     *       is skipped to prevent thrashing.
     */
    void EvictOldestPage(void *ignore_page);

    /**
     * @brief Marks a page as recently used (moves to front of LRU list)
     * 
     * Called whenever a page is accessed (after page fault handling).
     * Removes the page from its current position in active_ram_pages
     * and moves it to the front, implementing LRU replacement policy.
     * 
     * @param page_start Page-aligned base address of the accessed page
     */
    void MarkPageAsActive(void *page_start);

    /**
     * @brief Opens the disk file for page storage
     * 
     * Creates or truncates the file specified in config_.disk_file_path.
     * Sets up disk_file_handle/disk_file_descriptor for subsequent I/O.
     * 
     * @return true on success, false on failure
     */
    bool OpenDiskFile();

    /**
     * @brief Closes the disk file and cleans up resources
     */
    void CloseDiskFile();

    /**
     * @brief Writes compressed data to disk file
     * 
     * @param data Pointer to data to write
     * @param size Number of bytes to write
     * @param out_offset Receives the file offset where data was written
     * @return true on success, false on I/O error
     */
    bool WriteToDisk(const void* data, size_t size, size_t& out_offset);

    /**
     * @brief Reads compressed data from disk file
     * 
     * @param offset File offset to read from
     * @param size Number of bytes to read
     * @param buffer Buffer to read into (must be at least size bytes)
     * @return true on success, false on I/O error
     */
    bool ReadFromDisk(size_t offset, size_t size, void* buffer);

    /**
     * @brief Generates a cryptographic random encryption key
     * 
     * Uses platform CSPRNG (CryptGenRandom on Windows, /dev/urandom on Linux)
     * to generate a 256-bit key for ChaCha20 encryption.
     * 
     * @return true on success, false if random generation failed
     */
    bool GenerateEncryptionKey();

    /**
     * @brief Encrypts or decrypts a buffer using ChaCha20 stream cipher
     * 
     * ChaCha20 is a symmetric cipher, so encryption and decryption use
     * the same function. Each page uses a unique 96-bit nonce derived
     * from its memory address, ensuring each page is encrypted differently.
     * 
     * @param data Buffer to encrypt/decrypt (modified in place)
     * @param size Size of the buffer in bytes
     * @param nonce Unique 12-byte nonce for this encryption operation
     */
    void ChaCha20Crypt(unsigned char* data, size_t size, const unsigned char* nonce);

    /**
     * @brief ChaCha20 quarter round operation
     * @param state 16-word ChaCha20 state
     * @param a,b,c,d Indices into state for quarter round
     */
    static void ChaCha20QuarterRound(uint32_t* state, int a, int b, int c, int d);

    /**
     * @brief ChaCha20 block function
     * @param state Initial 16-word state
     * @param output 64-byte output buffer for keystream block
     */
    static void ChaCha20Block(const uint32_t* state, unsigned char* output);

public:
    /**
     * @brief Gets the singleton instance of GhostMemoryManager
     * 
     * Thread-safe lazy initialization (C++11 guaranteed).
     * 
     * @return Reference to the global GhostMemoryManager instance
     */
    static GhostMemoryManager &Instance()
    {
        static GhostMemoryManager instance;
        return instance;
    }

    /**
     * @brief Destructor - cleans up disk file if using disk backing
     */
    ~GhostMemoryManager()
    {
        CloseDiskFile();
        
        // Cleanup internal metadata
        if (lib_meta_ptr_)
        {
#ifdef _WIN32
            VirtualFree(lib_meta_ptr_, 0, MEM_RELEASE);
#else
            munmap(lib_meta_ptr_, 16);
#endif
            lib_meta_ptr_ = nullptr;
        }
    }

    /**
     * @brief Initializes the memory manager with custom configuration
     * 
     * Must be called before any AllocateGhost calls to configure disk
     * backing and other settings. If not called, default configuration
     * (in-memory backing) is used.
     * 
     * @param config Configuration structure with desired settings
     * @return true on success, false if disk file cannot be opened
     * 
     * @note This method is NOT thread-safe during first call. Call it
     *       from the main thread before spawning worker threads.
     * 
     * Example:
     * @code
     * GhostConfig config;
     * config.use_disk_backing = true;
     * config.disk_file_path = "myapp_ghost.swap";
     * config.max_memory_pages = 256; // 1MB RAM limit
     * 
     * if (!GhostMemoryManager::Instance().Initialize(config)) {
     *     // Handle error
     * }
     * @endcode
     */
    bool Initialize(const GhostConfig& config);

    /**
     * @brief Allocates virtual memory managed by GhostMem
     * 
     * Reserves virtual address space without allocating physical RAM.
     * The returned memory is initially inaccessible (no permissions).
     * Physical RAM is allocated on-demand when pages are accessed.
     * 
     * Platform behavior:
     * - Windows: Uses VirtualAlloc with MEM_RESERVE | PAGE_NOACCESS
     * - Linux: Uses mmap with PROT_NONE
     * 
     * @param size Number of bytes to allocate (rounded up to PAGE_SIZE)
     * @return Pointer to reserved virtual memory, or nullptr on failure
     * 
     * @note Memory is NOT zeroed initially (only zeroed on first access)
     */
    void *AllocateGhost(size_t size);

    /**
     * @brief Deallocates memory previously allocated by AllocateGhost
     * 
     * Decrements the reference count for the page containing this allocation.
     * When the reference count reaches zero (all allocations in the page
     * have been freed), the page is completely cleaned up:
     * 
     * 1. Removed from active_ram_pages LRU list
     * 2. Compressed data removed from backing_store (in-memory mode)
     * 3. Disk locations removed from disk_page_locations (disk mode)
     * 4. Physical and virtual memory released via VirtualFree/munmap
     * 
     * Thread Safety: Thread-safe. Uses internal mutex synchronization.
     * 
     * @param ptr Pointer returned by AllocateGhost
     * @param size Size passed to AllocateGhost (must match original size)
     * 
     * @note Deallocating nullptr is safe (no-op)
     * @note Deallocating untracked pointer logs warning but doesn't crash
     * @note After deallocation, accessing ptr results in access violation
     * 
     * Example:
     * @code
     * void* mem = manager.AllocateGhost(1024);
     * // ... use memory ...
     * manager.DeallocateGhost(mem, 1024);  // Properly cleanup
     * @endcode
     */
    void DeallocateGhost(void* ptr, size_t size);

    /**
     * @brief Compresses a page and removes it from physical RAM
     * 
     * This function implements the "freezing" operation:
     * 1. Compresses the 4KB page using LZ4
     * 2. Stores compressed data in backing_store
     * 3. Decommits/protects the page (frees physical RAM)
     * 
     * Platform behavior:
     * - Windows: Uses VirtualFree with MEM_DECOMMIT
     * - Linux: Uses mprotect with PROT_NONE
     * 
     * @param page_start Page-aligned base address of page to freeze
     * 
     * @note After this call, accessing page_start will trigger a page fault
     */
    void FreezePage(void *page_start);

#ifdef _WIN32
    /**
     * @brief Windows vectored exception handler for page faults
     * 
     * Catches EXCEPTION_ACCESS_VIOLATION exceptions and handles faults
     * for addresses in managed_blocks. When a managed page is accessed:
     * 1. Evicts old pages if needed (makes room)
     * 2. Commits physical RAM for the page
     * 3. Decompresses page data if it was previously frozen
     * 4. Marks page as active in LRU list
     * 5. Returns EXCEPTION_CONTINUE_EXECUTION to resume
     * 
     * @param pExceptionInfo Pointer to exception information structure
     * @return EXCEPTION_CONTINUE_EXECUTION if handled,
     *         EXCEPTION_CONTINUE_SEARCH otherwise
     */
    static LONG WINAPI VectoredHandler(PEXCEPTION_POINTERS pExceptionInfo);
#else
    /**
     * @brief Installs SIGSEGV signal handler for Linux
     * 
     * Sets up a signal handler using sigaction with SA_SIGINFO flag
     * to catch segmentation faults (SIGSEGV) caused by accessing
     * protected memory regions.
     */
    static void InstallSignalHandler();

    /**
     * @brief Linux signal handler for page faults
     * 
     * Catches SIGSEGV signals and handles faults for addresses in
     * managed_blocks. When a managed page is accessed:
     * 1. Evicts old pages if needed (makes room)
     * 2. Changes page protection to PROT_READ | PROT_WRITE
     * 3. Decompresses page data if it was previously frozen
     * 4. Zeros the page if it's a new allocation
     * 5. Marks page as active in LRU list
     * 6. Returns to resume execution
     * 
     * If the fault address is not managed by GhostMem, the signal
     * is re-raised with default handling (typically crash with core dump).
     * 
     * @param sig Signal number (should be SIGSEGV)
     * @param info Pointer to signal information (contains fault address)
     * @param context Pointer to signal context (unused)
     */
    static void SignalHandler(int sig, siginfo_t *info, void *context);
#endif
};