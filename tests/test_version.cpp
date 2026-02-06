#include "test_framework.h"
#include "ghostmem/Version.h"

// Test version constants
TEST(VersionConstants) {
    ASSERT_EQ(GhostMem::GetVersionMajor(), 1);
    ASSERT_EQ(GhostMem::GetVersionMinor(), 0);
    ASSERT_EQ(GhostMem::GetVersionPatch(), 1);
}

// Test version string
TEST(VersionString) {
    std::string version = GhostMem::GetVersionString();
    ASSERT_TRUE(version == "1.0.1");
}

// Test version number encoding
TEST(VersionNumber) {
    int version = GhostMem::GetVersion();
    ASSERT_EQ(version, 10001); // 1 * 10000 + 0 * 100 + 1
}
