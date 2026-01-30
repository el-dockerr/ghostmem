#pragma once

#ifdef _WIN32
    #include <windows.h>
#else
    #include <signal.h>
    #include <sys/mman.h>
    #include <unistd.h>
    #include <cstring>
#endif

#include <map>
#include <vector>
#include <list>
#include <algorithm>
#include "../3rdparty/lz4.h" // Make sure this file is included in the project!

const size_t PAGE_SIZE = 4096;
const size_t MAX_PHYSICAL_PAGES = 5;

class GhostMemoryManager
{
private:
    std::map<void *, size_t> managed_blocks;           // All virtual reservations
    std::map<void *, std::vector<char>> backing_store; // Compressed data

    // List of pages that are currently consuming REAL RAM
    std::list<void *> active_ram_pages;

    GhostMemoryManager()
    {
#ifdef _WIN32
        AddVectoredExceptionHandler(1, VectoredHandler);
#else
        InstallSignalHandler();
#endif
    }

    // Internal function: Make room!
    // Parameter 'ignore_page': The page we want to load (don't evict this one!)
    void EvictOldestPage(void *ignore_page);

    // Internal function: Mark page as "recently used"
    void MarkPageAsActive(void *page_start);

public:
    static GhostMemoryManager &Instance()
    {
        static GhostMemoryManager instance;
        return instance;
    }

    void *AllocateGhost(size_t size);

    void FreezePage(void *page_start);

#ifdef _WIN32
    static LONG WINAPI VectoredHandler(PEXCEPTION_POINTERS pExceptionInfo);
#else
    static void InstallSignalHandler();
    static void SignalHandler(int sig, siginfo_t *info, void *context);
#endif
};