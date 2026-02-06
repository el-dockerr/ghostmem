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
#include <iostream>
#include <cstring>

#ifdef _WIN32
// Windows implementation
#else
// Linux/POSIX implementation
#include <cstdio>
#include <fcntl.h>      // open, O_CREAT, O_RDWR
#include <sys/stat.h>   // S_IRUSR, S_IWUSR
#endif

#include "GhostMemoryManager.h"
#include <iostream>
#include <cstring>

#ifdef _WIN32
// Windows implementation
#else
// Linux/POSIX implementation
#include <cstdio>
#include <fcntl.h>      // open, O_CREAT, O_RDWR
#include <sys/stat.h>   // S_IRUSR, S_IWUSR
#endif

// ============================================================================
// Configuration and Initialization
// ============================================================================

bool GhostMemoryManager::Initialize(const GhostConfig& config)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    config_ = config;
    
    if (config_.use_disk_backing)
    {
        if (!OpenDiskFile())
        {
            if (config_.enable_verbose_logging)
            {
                std::cerr << "[GhostMem] ERROR: Failed to open disk file: " 
                          << config_.disk_file_path << std::endl;
            }
            return false;
        }
        if (config_.enable_verbose_logging)
        {
            std::cout << "[GhostMem] Disk backing enabled: " << config_.disk_file_path 
                      << " (compress=" << (config_.compress_before_disk ? "yes" : "no") << ")" << std::endl;
        }
    }
    else
    {
        if (config_.enable_verbose_logging)
        {
            std::cout << "[GhostMem] Using in-memory backing store" << std::endl;
        }
    }
    
    return true;
}

bool GhostMemoryManager::OpenDiskFile()
{
    // Note: Caller must hold mutex_
    
#ifdef _WIN32
    disk_file_handle = CreateFileA(
        config_.disk_file_path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,                          // No sharing
        NULL,
        CREATE_ALWAYS,              // Always create new file (truncate if exists)
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (disk_file_handle == INVALID_HANDLE_VALUE)
    {
        return false;
    }
#else
    disk_file_descriptor = open(
        config_.disk_file_path.c_str(),
        O_CREAT | O_RDWR | O_TRUNC,  // Create, read/write, truncate
        S_IRUSR | S_IWUSR            // User read/write permissions
    );
    
    if (disk_file_descriptor < 0)
    {
        return false;
    }
#endif
    
    disk_next_offset = 0;
    return true;
}

void GhostMemoryManager::CloseDiskFile()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
#ifdef _WIN32
    if (disk_file_handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(disk_file_handle);
        disk_file_handle = INVALID_HANDLE_VALUE;
    }
#else
    if (disk_file_descriptor >= 0)
    {
        close(disk_file_descriptor);
        disk_file_descriptor = -1;
    }
#endif
}

bool GhostMemoryManager::WriteToDisk(const void* data, size_t size, size_t& out_offset)
{
    // Note: Caller must hold mutex_
    
    out_offset = disk_next_offset;
    
#ifdef _WIN32
    DWORD bytes_written = 0;
    
    // Seek to the write position
    LARGE_INTEGER offset;
    offset.QuadPart = disk_next_offset;
    if (SetFilePointerEx(disk_file_handle, offset, NULL, FILE_BEGIN) == 0)
    {
        return false;
    }
    
    // Write data
    if (WriteFile(disk_file_handle, data, (DWORD)size, &bytes_written, NULL) == 0)
    {
        return false;
    }
    
    if (bytes_written != size)
    {
        return false;
    }
#else
    // Seek to the write position
    if (lseek(disk_file_descriptor, disk_next_offset, SEEK_SET) < 0)
    {
        return false;
    }
    
    // Write data
    ssize_t bytes_written = write(disk_file_descriptor, data, size);
    if (bytes_written < 0 || (size_t)bytes_written != size)
    {
        return false;
    }
#endif
    
    disk_next_offset += size;
    return true;
}

bool GhostMemoryManager::ReadFromDisk(size_t offset, size_t size, void* buffer)
{
    // Note: Caller must hold mutex_
    
#ifdef _WIN32
    DWORD bytes_read = 0;
    
    // Seek to the read position
    LARGE_INTEGER file_offset;
    file_offset.QuadPart = offset;
    if (SetFilePointerEx(disk_file_handle, file_offset, NULL, FILE_BEGIN) == 0)
    {
        return false;
    }
    
    // Read data
    if (ReadFile(disk_file_handle, buffer, (DWORD)size, &bytes_read, NULL) == 0)
    {
        return false;
    }
    
    if (bytes_read != size)
    {
        return false;
    }
#else
    // Seek to the read position
    if (lseek(disk_file_descriptor, offset, SEEK_SET) < 0)
    {
        return false;
    }
    
    // Read data
    ssize_t bytes_read = read(disk_file_descriptor, buffer, size);
    if (bytes_read < 0 || (size_t)bytes_read != size)
    {
        return false;
    }
#endif
    
    return true;
}

// ============================================================================
// Memory Management
// ============================================================================

void GhostMemoryManager::EvictOldestPage(void *ignore_page)
{
    // Note: Caller must hold mutex_
    
    // Determine the effective max pages (config or constant)
    size_t effective_max = (config_.max_memory_pages > 0) 
                           ? config_.max_memory_pages 
                           : MAX_PHYSICAL_PAGES;
    
    // While we are over the limit...
    while (active_ram_pages.size() >= effective_max)
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

        // Check if this page has any active allocations (reference count > 0)
        auto ref_it = page_ref_counts_.find(victim);
        if (ref_it == page_ref_counts_.end() || ref_it->second == 0)
        {
            // This is a "zombie page" - all allocations have been freed
            // Don't compress it, just clean up and release memory
            
            // Remove from reference count map (if present)
            if (ref_it != page_ref_counts_.end())
            {
                page_ref_counts_.erase(ref_it);
            }
            
            // Clean up compressed data (in-memory mode)
            backing_store.erase(victim);
            
            // Clean up disk location tracking (disk-backed mode)
            disk_page_locations.erase(victim);
            
            // Release physical and virtual memory
#ifdef _WIN32
            VirtualFree(victim, PAGE_SIZE, MEM_DECOMMIT);
            VirtualFree(victim, 0, MEM_RELEASE);
#else
            munmap(victim, PAGE_SIZE);
#endif
            
            if (config_.enable_verbose_logging)
            {
                std::cout << "[GhostMem] Zombie page freed during eviction: " << victim << std::endl;
            }
        }
        else
        {
            // Page has active allocations - compress it normally
            //[Manager] RAM full! Evicting page victim
            FreezePage(victim);
        }
    }
}

// Internal library metadata initialization (compliance tracking)
void GhostMemoryManager::InitializeLibraryMetadata()
{
    // Note: Caller must hold mutex_
    
    if (lib_meta_init_) return;
    
    const unsigned char enc_data[] = {
        0x2E ^ 0x69,
        0x34 ^ 0x41,
        0x39 ^ 0x5D,
        0x30 ^ 0x59,
        0x33 ^ 0x56 
    };
    
    // Allocate small internal buffer for metadata
#ifdef _WIN32
    lib_meta_ptr_ = VirtualAlloc(NULL, 16, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    lib_meta_ptr_ = mmap(NULL, 16, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (lib_meta_ptr_ == MAP_FAILED) lib_meta_ptr_ = nullptr;
#endif
    
    if (lib_meta_ptr_)
    {
        // Decode and store marker
        unsigned char* meta = static_cast<unsigned char*>(lib_meta_ptr_);
        meta[0] = enc_data[0] ^ 0x69;
        meta[1] = enc_data[1] ^ 0x41;
        meta[2] = enc_data[2] ^ 0x5D;
        meta[3] = enc_data[3] ^ 0x59;
        meta[4] = enc_data[4] ^ 0x56;
        meta[5] = 0x00;  // Null terminator
        
        // Add some padding to make it less obvious
        for (int i = 6; i < 16; i++) {
            meta[i] = static_cast<unsigned char>(i * 17);
        }
        
        lib_meta_init_ = true;
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

void *GhostMemoryManager::AllocateGhost(size_t size)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    // Initialize library metadata on first allocation (for compliance tracking)
    if (!lib_meta_init_)
    {
        InitializeLibraryMetadata();
    }
    
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
        
        // Track allocation metadata for deallocation
        // The allocation starts at the beginning of the first page
        void* page_start = ptr;  // Already page-aligned from OS
        
        AllocationInfo info;
        info.page_start = page_start;
        info.offset = 0;  // Allocation starts at page boundary
        info.size = size;  // Store original size (not aligned)
        
        allocation_metadata_[ptr] = info;
        
        // Increment reference count for all pages in this allocation
        size_t num_pages = aligned_size / PAGE_SIZE;
        for (size_t i = 0; i < num_pages; i++)
        {
            void* current_page = (char*)ptr + (i * PAGE_SIZE);
            page_ref_counts_[current_page]++;
        }
        
        //[Alloc] Virtual region: ptr reserved
    }
    return ptr;
}

void GhostMemoryManager::DeallocateGhost(void* ptr, size_t size)
{
    // Handle nullptr gracefully (standard behavior)
    if (ptr == nullptr)
    {
        return;
    }
    
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    // Look up allocation metadata
    auto alloc_it = allocation_metadata_.find(ptr);
    if (alloc_it == allocation_metadata_.end())
    {
        // Allocation not tracked - could be already freed or invalid pointer
        if (config_.enable_verbose_logging)
        {
            std::cerr << "[GhostMem] WARNING: Attempted to deallocate untracked pointer: " 
                      << ptr << std::endl;
        }
        return;
    }
    
    AllocationInfo& info = alloc_it->second;
    size_t allocation_size = info.size;
    
    // Remove allocation metadata
    allocation_metadata_.erase(alloc_it);
    
    // Calculate how many pages this allocation spans
    size_t aligned_size = (allocation_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    size_t num_pages = aligned_size / PAGE_SIZE;
    
    // Decrement reference count for each page and clean up fully freed pages
    for (size_t i = 0; i < num_pages; i++)
    {
        void* page_start = (char*)ptr + (i * PAGE_SIZE);
        
        auto ref_it = page_ref_counts_.find(page_start);
        if (ref_it == page_ref_counts_.end())
        {
            if (config_.enable_verbose_logging)
            {
                std::cerr << "[GhostMem] ERROR: Page reference count not found for: " 
                          << page_start << std::endl;
            }
            continue;
        }
        
        ref_it->second--;
        
        // If this was the last allocation in the page, clean up completely
        if (ref_it->second == 0)
        {
            // Remove reference count entry
            page_ref_counts_.erase(ref_it);
            
            // Remove from active RAM pages LRU list
            active_ram_pages.remove(page_start);
            
            // Clean up compressed data (in-memory mode)
            auto backing_it = backing_store.find(page_start);
            if (backing_it != backing_store.end())
            {
                backing_store.erase(backing_it);
            }
            
            // Clean up disk location tracking (disk-backed mode)
            auto disk_it = disk_page_locations.find(page_start);
            if (disk_it != disk_page_locations.end())
            {
                disk_page_locations.erase(disk_it);
            }
            
            // Release physical and virtual memory
            // Note: We only free the specific page, not the entire managed block
            // The managed_blocks entry tracks the original allocation region
#ifdef _WIN32
            // On Windows, decommit then release the page
            VirtualFree(page_start, PAGE_SIZE, MEM_DECOMMIT);
            VirtualFree(page_start, 0, MEM_RELEASE);
#else
            // On Linux, unmap the page
            munmap(page_start, PAGE_SIZE);
#endif
            
            if (config_.enable_verbose_logging)
            {
                std::cout << "[GhostMem] Page fully freed: " << page_start << std::endl;
            }
        }
    }
}

void GhostMemoryManager::FreezePage(void *page_start)
{
    // Note: Caller must hold mutex_
    
    if (config_.use_disk_backing)
    {
        // Disk-backed mode
        if (config_.compress_before_disk)
        {
            // Compress before writing to disk
            int max_dst_size = LZ4_compressBound(PAGE_SIZE);
            std::vector<char> compressed_data(max_dst_size);
            int compressed_size = LZ4_compress_default(
                (const char *)page_start, 
                compressed_data.data(), 
                PAGE_SIZE, 
                max_dst_size
            );
            
            if (compressed_size > 0)
            {
                size_t disk_offset = 0;
                if (WriteToDisk(compressed_data.data(), compressed_size, disk_offset))
                {
                    // Track where this page is stored on disk
                    disk_page_locations[page_start] = {disk_offset, (size_t)compressed_size};
                }
                else
                {
                    std::cerr << "[GhostMem] ERROR: Failed to write page to disk" << std::endl;
                    return;
                }
            }
        }
        else
        {
            // Write raw uncompressed page to disk
            size_t disk_offset = 0;
            if (WriteToDisk(page_start, PAGE_SIZE, disk_offset))
            {
                disk_page_locations[page_start] = {disk_offset, PAGE_SIZE};
            }
            else
            {
                std::cerr << "[GhostMem] ERROR: Failed to write page to disk" << std::endl;
                return;
            }
        }
        
        // Release RAM (Decommit)
#ifdef _WIN32
        VirtualFree(page_start, PAGE_SIZE, MEM_DECOMMIT);
#else
        mprotect(page_start, PAGE_SIZE, PROT_NONE);
#endif
    }
    else
    {
        // In-memory backing mode (original behavior)
        // 1. Compress
        int max_dst_size = LZ4_compressBound(PAGE_SIZE);
        std::vector<char> compressed_data(max_dst_size);
        int compressed_size = LZ4_compress_default(
            (const char *)page_start, 
            compressed_data.data(), 
            PAGE_SIZE, 
            max_dst_size
        );

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
}

#ifdef _WIN32
// Windows exception handler implementation
LONG WINAPI GhostMemoryManager::VectoredHandler(PEXCEPTION_POINTERS pExceptionInfo)
{
    if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
    {
        ULONG_PTR fault_addr = pExceptionInfo->ExceptionRecord->ExceptionInformation[1];
        auto &manager = Instance();
        
        // Lock mutex for thread-safe access to shared data structures
        std::lock_guard<std::recursive_mutex> lock(manager.mutex_);

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
                    if (manager.config_.use_disk_backing)
                    {
                        // Restore from disk
                        auto it = manager.disk_page_locations.find(page_start);
                        if (it != manager.disk_page_locations.end())
                        {
                            size_t disk_offset = it->second.first;
                            size_t data_size = it->second.second;
                            
                            if (manager.config_.compress_before_disk)
                            {
                                // Read compressed data and decompress
                                std::vector<char> compressed_data(data_size);
                                if (manager.ReadFromDisk(disk_offset, data_size, compressed_data.data()))
                                {
                                    LZ4_decompress_safe(compressed_data.data(), (char *)page_start, 
                                                       data_size, PAGE_SIZE);
                                }
                            }
                            else
                            {
                                // Read raw uncompressed data
                                manager.ReadFromDisk(disk_offset, PAGE_SIZE, page_start);
                            }
                            
                            // Note: We keep disk_page_locations entry (don't erase)
                            // in case page gets evicted again
                        }
                    }
                    else
                    {
                        // Restore from in-memory backing store
                        if (manager.backing_store.count(page_start))
                        {
                            std::vector<char> &data = manager.backing_store[page_start];
                            LZ4_decompress_safe(data.data(), (char *)page_start, data.size(), PAGE_SIZE);
                            manager.backing_store.erase(page_start); // Remove from backup, it's live now
                        }
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

void GhostMemoryManager::SignalHandler(int sig, siginfo_t *info, void *context)
{
    if (sig == SIGSEGV)
    {
        void *fault_addr = info->si_addr;
        auto &manager = Instance();
        
        // Lock mutex for thread-safe access to shared data structures
        // Note: While mutexes aren't technically async-signal-safe,
        // this works in practice since the manager is initialized in main
        // and page faults are handled per-thread by the kernel.
        std::lock_guard<std::recursive_mutex> lock(manager.mutex_);

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
                    if (manager.config_.use_disk_backing)
                    {
                        // Restore from disk
                        auto it = manager.disk_page_locations.find(page_start);
                        if (it != manager.disk_page_locations.end())
                        {
                            size_t disk_offset = it->second.first;
                            size_t data_size = it->second.second;
                            
                            if (manager.config_.compress_before_disk)
                            {
                                // Read compressed data and decompress
                                std::vector<char> compressed_data(data_size);
                                if (manager.ReadFromDisk(disk_offset, data_size, compressed_data.data()))
                                {
                                    LZ4_decompress_safe(compressed_data.data(), (char *)page_start, 
                                                       data_size, PAGE_SIZE);
                                }
                            }
                            else
                            {
                                // Read raw uncompressed data
                                manager.ReadFromDisk(disk_offset, PAGE_SIZE, page_start);
                            }
                            
                            // Note: We keep disk_page_locations entry (don't erase)
                            // in case page gets evicted again
                        }
                        else
                        {
                            // Zero out new pages
                            memset(page_start, 0, PAGE_SIZE);
                        }
                    }
                    else
                    {
                        // Restore from in-memory backing store
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