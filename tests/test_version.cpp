#include "test_framework.h"
#include "ghostmem/Version.h"

// Test version constants
TEST(VersionConstants) {
    ASSERT_EQ(GhostMem::GetVersionMajor(), 1);
    ASSERT_EQ(GhostMem::GetVersionMinor(), 1);
    ASSERT_EQ(GhostMem::GetVersionPatch(), 0);
}

// Test version string
TEST(VersionString) {
    std::string version = GhostMem::GetVersionString();
    ASSERT_TRUE(version == "1.1.0");
}

// Test version number encoding
TEST(VersionNumber) {
    int version = GhostMem::GetVersion();
    ASSERT_EQ(version, 10100); // 1 * 10000 + 1 * 100 + 0
}
