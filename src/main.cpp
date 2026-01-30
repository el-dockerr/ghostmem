#ifdef _WIN32
    #include <windows.h>
#endif
#include <iostream>
#include "ghostmem/GhostMemoryManager.h"
#include "ghostmem/GhostAllocator.h"
#include "ghostmem/Version.h"


// --- Test Program ---
int main() {
    std::cout << "===========================================\n";
    std::cout << "GhostMem v" << GhostMem::GetVersionString() << "\n";
    std::cout << "Virtual RAM through Transparent Compression\n";
    std::cout << "===========================================\n\n";
    
    std::cout << "--- GhostRAM with C++ Containers ---\n\n";

    // We create a vector that uses our allocator.
    // This means: Every time the vector grows, it gets memory from US.
    std::vector<int, GhostAllocator<int>> vec;

    std::cout << "1. Filling vector with 10,000 numbers...\n";
    // This will request multiple pages.
    // Since our limit is MAX_PHYSICAL_PAGES = 5, it will
    // swap wildly in the background (compress/decompress).
    for (int i = 0; i < 10000; i++) {
        vec.push_back(i);
        // On each push_back a re-allocation could happen -> Trap -> Copy -> Trap
    }

    std::cout << "   Vector size: " << vec.size() << "\n";
    std::cout << "   (Check the logs above: It was constantly swapping!)\n";

    std::cout << "\n2. Accessing index 5000...\n";
    int val = vec[5000]; // Access -> Trap -> Restore
    std::cout << "   Value: " << val << "\n";

    // --- Test with Strings (text compresses really well) ---

    using GhostString = std::basic_string<char, std::char_traits<char>, GhostAllocator<char>>;

    std::cout << "\n3. Testing with Ghost-Strings...\n";
    // A long string that consists mostly of 'A' (perfect for LZ4)
    GhostString s(4000, 'A');
    s += "END";

    std::cout << "   String created. Last words: " << (s.c_str() + 3995) << "\n";

    // We force the manager to make room (by accessing the vector)
    // This should evict the string (freeze it).
    int force_swap = vec[0];

    std::cout << "   Reading string again (should trigger decompress)...\n";
    std::cout << "   Content check: " << (s.c_str() + 3995) << "\n";

    std::cout << "\nPress Enter to exit...\n";
    std::cin.get();
    return 0;
}