/**
 * @file encryption_example.cpp
 * @brief Example demonstrating GhostMem disk encryption feature
 * 
 * This example shows how to enable encryption for disk-backed pages,
 * protecting sensitive data at rest in the swap file.
 */

#include "ghostmem/GhostMemoryManager.h"
#include "ghostmem/GhostAllocator.h"
#include <iostream>
#include <vector>
#include <string>
#include <cstring>

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  GhostMem Disk Encryption Example" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // Configure GhostMem with disk backing and encryption
    GhostConfig config;
    config.use_disk_backing = true;          // Enable disk-backed storage
    config.encrypt_disk_pages = true;        // Enable ChaCha20 encryption
    config.compress_before_disk = true;      // Compress before encrypting
    config.disk_file_path = "secure_swap.dat";
    config.max_memory_pages = 3;             // Small limit to force eviction
    config.enable_verbose_logging = true;    // Show operations
    
    // Initialize GhostMem with encryption enabled
    if (!GhostMemoryManager::Instance().Initialize(config)) {
        std::cerr << "Failed to initialize GhostMemoryManager!" << std::endl;
        return 1;
    }
    
    std::cout << "Configuration:" << std::endl;
    std::cout << "  - Disk backing: ENABLED" << std::endl;
    std::cout << "  - Encryption: ENABLED (ChaCha20-256)" << std::endl;
    std::cout << "  - Compression: ENABLED (LZ4)" << std::endl;
    std::cout << "  - Swap file: " << config.disk_file_path << std::endl;
    std::cout << "  - Max RAM pages: " << config.max_memory_pages << " (12 KB)" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Creating vectors with sensitive data..." << std::endl;
    std::cout << std::endl;
    
    // Create vectors using GhostAllocator
    // These will use encrypted disk storage when evicted from RAM
    std::vector<std::string, GhostAllocator<std::string>> sensitive_data;
    
    // Add sensitive information
    sensitive_data.push_back("Credit Card: 4532-1234-5678-9012");
    sensitive_data.push_back("SSN: 123-45-6789");
    sensitive_data.push_back("Password: MySecretPassword123!");
    sensitive_data.push_back("API Key: sk_live_51H4abc123xyz456def789");
    sensitive_data.push_back("PIN: 1234");
    
    std::cout << "Sensitive data stored (will be encrypted if evicted):" << std::endl;
    for (size_t i = 0; i < sensitive_data.size(); i++) {
        std::cout << "  [" << i << "] " << sensitive_data[i] << std::endl;
    }
    std::cout << std::endl;
    
    // Allocate more memory to force page eviction
    std::cout << "Allocating additional memory to trigger eviction..." << std::endl;
    std::vector<int, GhostAllocator<int>> numbers;
    for (int i = 0; i < 5000; i++) {
        numbers.push_back(i);
    }
    std::cout << "Created vector with " << numbers.size() << " integers" << std::endl;
    std::cout << std::endl;
    
    // Access the sensitive data again (will be decrypted if it was evicted)
    std::cout << "Accessing sensitive data (may trigger decryption)..." << std::endl;
    std::cout << "First entry: " << sensitive_data[0] << std::endl;
    std::cout << "Last entry:  " << sensitive_data[sensitive_data.size() - 1] << std::endl;
    std::cout << std::endl;
    
    // Demonstrate that data persists correctly through eviction/restoration
    std::cout << "Verifying all data integrity..." << std::endl;
    bool all_correct = true;
    std::vector<std::string> expected = {
        "Credit Card: 4532-1234-5678-9012",
        "SSN: 123-45-6789",
        "Password: MySecretPassword123!",
        "API Key: sk_live_51H4abc123xyz456def789",
        "PIN: 1234"
    };
    
    for (size_t i = 0; i < expected.size(); i++) {
        if (sensitive_data[i] != expected[i]) {
            std::cout << "  ERROR: Data mismatch at index " << i << std::endl;
            all_correct = false;
        }
    }
    
    if (all_correct) {
        std::cout << "  ✓ All sensitive data verified correctly!" << std::endl;
    }
    std::cout << std::endl;
    
    std::cout << "========================================" << std::endl;
    std::cout << "  Security Notes:" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "1. Encryption key (256-bit) is generated at startup" << std::endl;
    std::cout << "2. Key exists only in RAM, never written to disk" << std::endl;
    std::cout << "3. Each page uses a unique nonce (address-based)" << std::endl;
    std::cout << "4. Swap file contents are unreadable without the key" << std::endl;
    std::cout << "5. Key is destroyed when program exits" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Check '" << config.disk_file_path << "' - it contains encrypted" << std::endl;
    std::cout << "data (compressed + encrypted with ChaCha20)" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Example completed successfully!" << std::endl;
    
    return 0;
}
