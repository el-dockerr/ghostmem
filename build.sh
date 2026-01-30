#!/bin/bash
# Build script for GhostMem on Linux

set -e  # Exit on error

echo "================================================"
echo "  GhostMem Linux Build Script"
echo "================================================"
echo

# Check if CMake is available
if ! command -v cmake &> /dev/null; then
    echo "[ERROR] CMake not found! Please install CMake."
    echo "On Ubuntu/Debian: sudo apt-get install cmake"
    echo "On Fedora/RHEL:   sudo dnf install cmake"
    echo "On Arch:          sudo pacman -S cmake"
    exit 1
fi

# Check for C++ compiler
if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
    echo "[ERROR] No C++ compiler found! Please install g++ or clang++."
    echo "On Ubuntu/Debian: sudo apt-get install build-essential"
    echo "On Fedora/RHEL:   sudo dnf groupinstall 'Development Tools'"
    echo "On Arch:          sudo pacman -S base-devel"
    exit 1
fi

# Create build directory
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build

echo
echo "[1/3] Configuring project with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

echo
echo "[2/3] Building project..."
cmake --build . -- -j$(nproc)

echo
echo "[3/3] Build complete!"
echo
echo "Output files:"
echo "  - Static library: build/libghostmem.a"
echo "  - Shared library: build/libghostmem.so"
echo "  - Demo program:   build/ghostmem_demo"
echo
echo "To run the demo:"
echo "  cd build"
echo "  ./ghostmem_demo"
echo

cd ..
echo "================================================"
echo "  Build completed successfully!"
echo "================================================"
