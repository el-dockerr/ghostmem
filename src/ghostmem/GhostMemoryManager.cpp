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
 * @file GhostMemoryManager.cpp
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

#include "GhostMemoryManager.h"

#ifdef _WIN32
// Windows implementation
#else
// Linux/POSIX implementation
#include <cstdio>
#endif

void GhostMemoryManager::EvictOldestPage(void *ignore_page)
{
    // Note: Caller must hold mutex_
    
    // While we are over the limit...
    while (active_ram_pages.size() >= MAX_PHYSICAL_PAGES)
    {

        void *victim = active_ram_pages.back();

        // Protection: If the victim is the page we need right now,
        // take the second-to-last page instead!
        if (victim == ignore_page)
        {
            if (active_ram_pages.size() < 2)
                break; // Emergency brake: We only have this one page

            // Take the second-to-last one
            auto it = active_ram_pages.end();
            --it;
            --it;
            victim = *it;

            // Manually remove from the list
            active_ram_pages.erase(it);
        }
        else
        {
            active_ram_pages.pop_back();
        }

        //[Manager] RAM full! Evicting page victim
        FreezePage(victim);
    }
}

// Internal function: Mark page as "recently used"
void GhostMemoryManager::MarkPageAsActive(void *page_start)
{
    // Note: Caller must hold mutex_
    
    // First check if it's already in the list (shouldn't be, but better safe than sorry)
    active_ram_pages.remove(page_start);

    // Insert at front (Most Recently Used)
    active_ram_pages.push_front(page_start);
}

void td::lock_guard<std::recursive_mutex> lock(mutex_);
    
    s*GhostMemoryManager::AllocateGhost(size_t size)
{
    size_t aligned_size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    
#ifdef _WIN32
    void *ptr = VirtualAlloc(NULL, aligned_size, MEM_RESERVE, PAGE_NOACCESS);
#else
    // Reserve address space without allocating physical pages
    void *ptr = mmap(NULL, aligned_size, PROT_NONE, 
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) ptr = nullptr;
#endif
    
    if (ptr)
    {
        managed_blocks[ptr] = aligned_size;
        //[Alloc] Virtual region: ptr reserved
    }
    return ptr;
}

void GhNote: Caller must hold mutex_
    
    // ostMemoryManager::FreezePage(void *page_start)
{
    // 1. Compress
    int max_dst_size = LZ4_compressBound(PAGE_SIZE);
    std::vector<char> compressed_data(max_dst_size);
    int compressed_size = LZ4_compress_default((const char *)page_start, compressed_data.data(), PAGE_SIZE, max_dst_size);

    if (compressed_size > 0)
    {
        compressed_data.resize(compressed_size);
        backing_store[page_start] = compressed_data; // Store in the vault

        // 2. Release RAM (Decommit)
#ifdef _WIN32
        VirtualFree(page_start, PAGE_SIZE, MEM_DECOMMIT);
#else
        // On Linux, we use mprotect to make it inaccessible again
        mprotect(page_start, PAGE_SIZE, PROT_NONE);
#endif
    }
}

#ifdef _WIN32
// Windows exception handler implementation
LONG WINAPI GhostMemoryManager::VectoredHandler(PEXCEPTION_POINTERS pExceptionInfo)
{
        
        // Lock mutex for thread-safe access to shared data structures
        std::lock_guard<std::recursive_mutex> lock(manager.mutex_);
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
    {
        ULONG_PTR fault_addr = pExceptionInfo->ExceptionRecord->ExceptionInformation[1];
        auto &manager = Instance();

        // Is it our address?
        for (const auto &block : manager.managed_blocks)
        {
            ULONG_PTR start = (ULONG_PTR)block.first;
            ULONG_PTR end = start + block.second;

            if (fault_addr >= start && fault_addr < end)
            {
                void *page_start = (void *)(fault_addr & ~(PAGE_SIZE - 1));

                //[Trap] Access to  page_start
                // IMPORTANT: Before getting RAM, we must check if we have room!
                manager.EvictOldestPage(page_start);

                // Now we have room -> get RAM
                if (VirtualAlloc(page_start, PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE))
                {

                    // If data was in backup -> Restore
                    if (manager.backing_store.count(page_start))
                    {
                        std::vector<char> &data = manager.backing_store[page_start];
                        LZ4_decompress_safe(data.data(), (char *)page_start, data.size(), PAGE_SIZE);
                        manager.backing_store.erase(page_start); // Remove from backup, it's live now
                    }

                    // Add to active list
                    manager.MarkPageAsActive(page_start);

                    return EXCEPTION_CONTINUE_EXECUTION;
                }
            }
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

#else
// Linux signal handler implementation
void GhostMemoryManager::InstallSignalHandler()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = SignalHandler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, nullptr);
}
        
        // Lock mutex for thread-safe access to shared data structures
        // Note: While mutexes aren't technically async-signal-safe,
        // this works in practice since the manager is initialized in main
        // and page faults are handled per-thread by the kernel.
        std::lock_guard<std::recursive_mutex> lock(manager.mutex_);

void GhostMemoryManager::SignalHandler(int sig, siginfo_t *info, void *context)
{
    if (sig == SIGSEGV)
    {
        void *fault_addr = info->si_addr;
        auto &manager = Instance();

        // Is it our address?
        for (const auto &block : manager.managed_blocks)
        {
            uintptr_t start = (uintptr_t)block.first;
            uintptr_t end = start + block.second;
            uintptr_t fault = (uintptr_t)fault_addr;

            if (fault >= start && fault < end)
            {
                void *page_start = (void *)(fault & ~(PAGE_SIZE - 1));

                //[Trap] Access to  page_start
                // IMPORTANT: Before getting RAM, we must check if we have room!
                manager.EvictOldestPage(page_start);

                // Now we have room -> get RAM (make page accessible)
                if (mprotect(page_start, PAGE_SIZE, PROT_READ | PROT_WRITE) == 0)
                {
                    // If data was in backup -> Restore
                    if (manager.backing_store.count(page_start))
                    {
                        std::vector<char> &data = manager.backing_store[page_start];
                        LZ4_decompress_safe(data.data(), (char *)page_start, data.size(), PAGE_SIZE);
                        manager.backing_store.erase(page_start); // Remove from backup, it's live now
                    }
                    else
                    {
                        // Zero out new pages
                        memset(page_start, 0, PAGE_SIZE);
                    }

                    // Add to active list
                    manager.MarkPageAsActive(page_start);

                    return; // Continue execution
                }
            }
        }
    }
    
    // Not our fault address - reraise signal for default handling
    signal(SIGSEGV, SIG_DFL);
    raise(SIGSEGV);
}
#endif