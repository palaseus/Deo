#!/bin/bash
# Deo Blockchain Test Runner
# Ensures test results are saved to the test_results directory

set -e

# Create test_results directory if it doesn't exist
mkdir -p test_results

# Change to build directory
cd build

# Run tests with XML output to test_results directory
echo "Running Deo Blockchain tests..."
echo "Test results will be saved to ../test_results/"

# Run all tests
./bin/DeoBlockchain_tests --gtest_output=xml:../test_results/test_results.xml

echo "Tests completed. Results saved to test_results/test_results.xml"


