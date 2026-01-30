#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>

// Simple test framework for GhostMem
class TestRunner {
private:
    struct Test {
        std::string name;
        std::function<void()> func;
    };
    
    std::vector<Test> tests;
    int passed = 0;
    int failed = 0;
    
public:
    static TestRunner& Instance() {
        static TestRunner instance;
        return instance;
    }
    
    void AddTest(const std::string& name, std::function<void()> func) {
        tests.push_back({name, func});
    }
    
    int RunAll() {
        std::cout << "Running " << tests.size() << " tests...\n\n";
        
        for (const auto& test : tests) {
            std::cout << "[ RUN      ] " << test.name << "\n";
            try {
                test.func();
                std::cout << "[       OK ] " << test.name << "\n";
                passed++;
            } catch (const std::exception& e) {
                std::cout << "[  FAILED  ] " << test.name << "\n";
                std::cout << "           Error: " << e.what() << "\n";
                failed++;
            }
        }
        
        std::cout << "\n===================\n";
        std::cout << "Tests passed: " << passed << "/" << tests.size() << "\n";
        std::cout << "Tests failed: " << failed << "/" << tests.size() << "\n";
        
        return failed == 0 ? 0 : 1;
    }
};

// Helper macros
#define TEST(name) \
    static void Test_##name(); \
    static struct TestRegistrar_##name { \
        TestRegistrar_##name() { \
            TestRunner::Instance().AddTest(#name, Test_##name); \
        } \
    } testRegistrar_##name; \
    static void Test_##name()

#define ASSERT_TRUE(condition) \
    if (!(condition)) { \
        throw std::runtime_error("Assertion failed: " #condition); \
    }

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) { \
        throw std::runtime_error("Assertion failed: " #a " == " #b); \
    }

#define ASSERT_NE(a, b) \
    if ((a) == (b)) { \
        throw std::runtime_error("Assertion failed: " #a " != " #b); \
    }

#define ASSERT_NOT_NULL(ptr) \
    if ((ptr) == nullptr) { \
        throw std::runtime_error("Assertion failed: " #ptr " != nullptr"); \
    }
