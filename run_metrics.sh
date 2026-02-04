#!/bin/bash
# Run GhostMem performance metrics tests and save results

echo "============================================"
echo "GhostMem Performance Metrics Runner"
echo "============================================"
echo ""

# Check if build directory exists
if [ ! -f "build/ghostmem_tests" ]; then
    echo "ERROR: Tests not built. Please run build.sh first."
    exit 1
fi

# Create results directory
mkdir -p metrics_results

# Generate timestamp for filename
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_FILE="metrics_results/metrics_${TIMESTAMP}.txt"

echo "Running tests..."
echo "Results will be saved to: ${OUTPUT_FILE}"
echo ""

# Run tests and save output
./build/ghostmem_tests > "${OUTPUT_FILE}" 2>&1
TEST_RESULT=$?

# Check if tests passed
if [ $TEST_RESULT -eq 0 ]; then
    echo ""
    echo "============================================"
    echo "SUCCESS: All tests passed!"
    echo "============================================"
    echo ""
    echo "Key Metrics Summary:"
    echo "-------------------"
    grep -E "Compression:|Slowdown:|savings:|Physical RAM limit:" "${OUTPUT_FILE}"
    echo ""
    echo "Full results saved to: ${OUTPUT_FILE}"
else
    echo ""
    echo "============================================"
    echo "ERROR: Some tests failed!"
    echo "============================================"
    echo "Check ${OUTPUT_FILE} for details"
    exit 1
fi

echo ""
echo "To compare with previous results:"
echo "  diff metrics_results/metrics_PREVIOUS.txt ${OUTPUT_FILE}"
echo ""

exit 0
