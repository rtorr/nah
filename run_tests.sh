#!/bin/bash
# Run all NAH tests

set -e

echo "Building and running all NAH tests..."

# Create build directory if it doesn't exist
mkdir -p build

# Configure with tests enabled
cmake -B build -DNAH_ENABLE_TESTS=ON

# Build everything
cmake --build build

# Run tests
echo ""
echo "Running tests..."
ctest --test-dir build --output-on-failure

echo ""
echo "All tests completed!"