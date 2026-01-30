#include "test_framework.h"
#include <iostream>

int main() {
    std::cout << "===========================================\n";
    std::cout << "GhostMem Test Suite\n";
    std::cout << "===========================================\n\n";
    
    int result = TestRunner::Instance().RunAll();
    
    if (result == 0) {
        std::cout << "\n✓ All tests passed!\n";
    } else {
        std::cout << "\n✗ Some tests failed!\n";
    }
    
    return result;
}
