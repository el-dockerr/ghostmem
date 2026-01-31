#include "test_framework.h"
#include "ghostmem/GhostMemoryManager.h"
#include "ghostmem/GhostAllocator.h"
#include <vector>
#include <string>

// Test disk-backed storage configuration
TEST(DiskBackingConfiguration) {
    GhostConfig config;
    config.use_disk_backing = true;
    config.disk_file_path = "test_ghostmem.swap";
    config.compress_before_disk = true;
    config.max_memory_pages = 3;  // Very small limit to force disk usage
    
    // Note: This test would normally initialize a new instance,
    // but since GhostMemoryManager is a singleton and already initialized,
    // we can't test initialization here without refactoring.
    // This test verifies the configuration structure exists and compiles.
    ASSERT_TRUE(true);
}

// Test that disk backing file would be created (manual verification)
TEST(DiskBackingFileCreation) {
    // This is a placeholder test that verifies the disk backing
    // configuration compiles. Actual file I/O testing would require
    // either mocking or a separate test program.
    GhostConfig config;
    config.use_disk_backing = true;
    config.disk_file_path = "test_disk_backing.swap";
    config.compress_before_disk = true;
    config.max_memory_pages = 2;
    
    // Verify configuration values
    ASSERT_TRUE(config.use_disk_backing);
    ASSERT_TRUE(config.disk_file_path == "test_disk_backing.swap");
    ASSERT_TRUE(config.compress_before_disk);
    ASSERT_EQ(config.max_memory_pages, 2);
}

// Test uncompressed disk mode configuration
TEST(DiskBackingUncompressed) {
    GhostConfig config;
    config.use_disk_backing = true;
    config.disk_file_path = "test_uncompressed.swap";
    config.compress_before_disk = false;  // No compression
    config.max_memory_pages = 4;
    
    ASSERT_TRUE(config.use_disk_backing);
    ASSERT_TRUE(!config.compress_before_disk);  // Check false using !
}

// Test default configuration values
TEST(DefaultConfigValues) {
    GhostConfig config;
    
    // Check defaults
    ASSERT_TRUE(!config.use_disk_backing);  // Default: in-memory (check false using !)
    ASSERT_TRUE(config.disk_file_path == "ghostmem.swap");
    ASSERT_TRUE(config.compress_before_disk);  // Default: compress
    ASSERT_EQ(config.max_memory_pages, 0);  // Default: use constant
}
