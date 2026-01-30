#include "GhostMemoryManager.h"

void GhostMemoryManager::EvictOldestPage(void *ignore_page)
{
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
    // First check if it's already in the list (shouldn't be, but better safe than sorry)
    active_ram_pages.remove(page_start);

    // Insert at front (Most Recently Used)
    active_ram_pages.push_front(page_start);
}

void *GhostMemoryManager::AllocateGhost(size_t size)
{
    size_t aligned_size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    void *ptr = VirtualAlloc(NULL, aligned_size, MEM_RESERVE, PAGE_NOACCESS);
    if (ptr)
    {
        managed_blocks[ptr] = aligned_size;
        //[Alloc] Virtual region: ptr reserved
    }
    return ptr;
}

void GhostMemoryManager::FreezePage(void *page_start)
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
        VirtualFree(page_start, PAGE_SIZE, MEM_DECOMMIT);
    }
}

LONG WINAPI GhostMemoryManager::VectoredHandler(PEXCEPTION_POINTERS pExceptionInfo)
{
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