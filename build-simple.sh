#!/bin/bash
# Quick build script for Linux without CMake (direct compilation)

set -e

echo "Building GhostMem (Direct Compilation)..."

# Compile
g++ -std=c++17 -O2 -pthread \
    src/main.cpp \
    src/ghostmem/GhostMemoryManager.cpp \
    src/3rdparty/lz4.c \
    -I src \
    -o ghostmem_demo

echo "Build complete! Run with: ./ghostmem_demo"
