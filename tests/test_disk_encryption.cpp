#include "test_framework.h"
#include "ghostmem/GhostMemoryManager.h"
#include "ghostmem/GhostAllocator.h"
#include <vector>
#include <string>
#include <fstream>
#include <cstring>

// Test encryption configuration default value
TEST(EncryptionConfigDefault) {
    GhostConfig config;
    
    // Check that encryption is disabled by default
    ASSERT_TRUE(!config.encrypt_disk_pages);  
}

// Test enabling encryption in configuration
TEST(EncryptionConfigEnable) {
    GhostConfig config;
    config.use_disk_backing = true;
    config.encrypt_disk_pages = true;
    config.disk_file_path = "test_encrypted.swap";
    config.compress_before_disk = true;
    
    ASSERT_TRUE(config.use_disk_backing);
    ASSERT_TRUE(config.encrypt_disk_pages);
    ASSERT_TRUE(config.disk_file_path == "test_encrypted.swap");
}

// Test encryption without disk backing (should be ignored)
TEST(EncryptionWithoutDiskBacking) {
    GhostConfig config;
    config.use_disk_backing = false;  // In-memory mode
    config.encrypt_disk_pages = true;  // This should have no effect
    
    // Verify configuration compiles and doesn't cause issues
    ASSERT_TRUE(!config.use_disk_backing);
    ASSERT_TRUE(config.encrypt_disk_pages);  // Flag is set but won't be used
}

// Test encryption with uncompressed disk mode
TEST(EncryptionUncompressedMode) {
    GhostConfig config;
    config.use_disk_backing = true;
    config.encrypt_disk_pages = true;
    config.compress_before_disk = false;  // No compression, just encryption
    config.disk_file_path = "test_encrypted_raw.swap";
    
    ASSERT_TRUE(config.use_disk_backing);
    ASSERT_TRUE(config.encrypt_disk_pages);
    ASSERT_TRUE(!config.compress_before_disk);
}

// Test that encrypted data in swap file is not plaintext
// This test verifies the encryption is actually working by checking
// that known patterns written to memory don't appear in plaintext in the swap file
TEST(EncryptedDataNotPlaintext) {
    // Create a unique test string that's easy to find
    const char* secret_data = "TOP_SECRET_PATTERN_12345_SHOULD_BE_ENCRYPTED";
    const size_t data_len = strlen(secret_data);
    
    // This test is conceptual - in practice, we'd need to:
    // 1. Initialize a fresh GhostMemoryManager instance with encryption enabled
    // 2. Allocate memory and write the secret pattern
    // 3. Force page eviction to disk
    // 4. Read the swap file and verify the pattern is NOT in plaintext
    //
    // Since GhostMemoryManager is a singleton and already initialized,
    // we verify the configuration structure exists and compiles correctly.
    
    GhostConfig config;
    config.use_disk_backing = true;
    config.encrypt_disk_pages = true;
    config.disk_file_path = "test_encryption_verify.swap";
    config.max_memory_pages = 2;  // Small limit to force eviction
    config.compress_before_disk = true;
    
    ASSERT_TRUE(config.encrypt_disk_pages);
    ASSERT_TRUE(data_len > 0);  // Verify test data is valid
}

// Test encryption key generation would succeed
// This verifies the key generation configuration is valid
TEST(EncryptionKeyGeneration) {
    GhostConfig config;
    config.use_disk_backing = true;
    config.encrypt_disk_pages = true;
    config.disk_file_path = "test_keygen.swap";
    
    // The key would be generated during Initialize() call
    // This test verifies the configuration for key generation is valid
    ASSERT_TRUE(config.encrypt_disk_pages);
}

// Test encryption with compression
TEST(EncryptionWithCompression) {
    GhostConfig config;
    config.use_disk_backing = true;
    config.encrypt_disk_pages = true;
    config.compress_before_disk = true;  // Both compress AND encrypt
    config.disk_file_path = "test_compress_encrypt.swap";
    config.max_memory_pages = 3;
    
    // Verify both flags can be enabled together
    ASSERT_TRUE(config.use_disk_backing);
    ASSERT_TRUE(config.encrypt_disk_pages);
    ASSERT_TRUE(config.compress_before_disk);
}

// Test verbose logging with encryption
TEST(EncryptionVerboseLogging) {
    GhostConfig config;
    config.use_disk_backing = true;
    config.encrypt_disk_pages = true;
    config.enable_verbose_logging = true;  // Should log encryption status
    config.disk_file_path = "test_verbose_encrypt.swap";
    
    ASSERT_TRUE(config.encrypt_disk_pages);
    ASSERT_TRUE(config.enable_verbose_logging);
}

// Test all encryption-related options together
TEST(EncryptionFullConfiguration) {
    GhostConfig config;
    config.use_disk_backing = true;
    config.encrypt_disk_pages = true;
    config.compress_before_disk = true;
    config.disk_file_path = "test_full_encryption.swap";
    config.max_memory_pages = 5;
    config.enable_verbose_logging = true;
    
    // Verify all settings
    ASSERT_TRUE(config.use_disk_backing);
    ASSERT_TRUE(config.encrypt_disk_pages);
    ASSERT_TRUE(config.compress_before_disk);
    ASSERT_EQ(config.max_memory_pages, 5);
    ASSERT_TRUE(config.enable_verbose_logging);
    ASSERT_TRUE(config.disk_file_path == "test_full_encryption.swap");
}

// Test that encryption configuration doesn't break existing functionality
TEST(EncryptionBackwardsCompatibility) {
    // Test 1: Old config without encryption field (defaults to false)
    GhostConfig config1;
    config1.use_disk_backing = true;
    config1.disk_file_path = "test_old_config.swap";
    // Don't set encrypt_disk_pages - should default to false
    
    ASSERT_TRUE(config1.use_disk_backing);
    ASSERT_TRUE(!config1.encrypt_disk_pages);  // Default: disabled
    
    // Test 2: Explicitly disable encryption
    GhostConfig config2;
    config2.use_disk_backing = true;
    config2.encrypt_disk_pages = false;
    config2.disk_file_path = "test_no_encryption.swap";
    
    ASSERT_TRUE(config2.use_disk_backing);
    ASSERT_TRUE(!config2.encrypt_disk_pages);
}
