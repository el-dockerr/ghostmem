#include "test_framework.h"
#include "ghostmem/Version.h"

// Test version constants
TEST(VersionConstants) {
    ASSERT_EQ(GhostMem::GetVersionMajor(), 0);
    ASSERT_EQ(GhostMem::GetVersionMinor(), 11);
    ASSERT_EQ(GhostMem::GetVersionPatch(), 0);
}

// Test version string
TEST(VersionString) {
    std::string version = GhostMem::GetVersionString();
    ASSERT_TRUE(version == "0.11.0");
}

// Test version number encoding
TEST(VersionNumber) {
    int version = GhostMem::GetVersion();
    ASSERT_EQ(version, 1100); // 0 * 10000 + 11 * 100 + 0
}
