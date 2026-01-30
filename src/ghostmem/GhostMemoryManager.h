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
 * Thread Safety: Currently NOT thread-safe. Use from single thread only.
 */
class GhostMemoryManager
{
private:
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
     * @brief Storage for compressed page data
     * 
     * Key: Page base address (page-aligned)
     * Value: Vector containing LZ4-compressed page data
     * 
     * When a page is evicted from physical RAM, its contents are
     * compressed and stored here. The compression ratio varies:
     * - Text/strings: 5-10x compression
     * - Repeated data: 10-50x compression
     * - Random data: ~1x (incompressible)
     */
    std::map<void *, std::vector<char>> backing_store;

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
     * @note Currently no matching Free function (PoC limitation)
     */
    void *AllocateGhost(size_t size);

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