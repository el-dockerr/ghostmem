#pragma once

#include "GhostMemoryManager.h"

// A wrapper so std::vector & others use our manager
template <typename T>
struct GhostAllocator {
    using value_type = T;

    GhostAllocator() = default;
    template <typename U> GhostAllocator(const GhostAllocator<U>&) {}

    T* allocate(size_t n) {
        // We call our DLL/Manager
        // Note: AllocateGhost returns void*, we cast to T*
        size_t bytes = n * sizeof(T);
        return static_cast<T*>(GhostMemoryManager::Instance().AllocateGhost(bytes));
    }

    void deallocate(T* p, size_t n) {
        // Deallocate memory through our manager
        // This enables automatic cleanup when STL containers destroy elements
        if (p != nullptr) {
            GhostMemoryManager::Instance().DeallocateGhost(p, n * sizeof(T));
        }
    }
};

template <typename T, typename U>
bool operator==(const GhostAllocator<T>&, const GhostAllocator<U>&) { return true; }
template <typename T, typename U>
bool operator!=(const GhostAllocator<T>&, const GhostAllocator<U>&) { return false; }
