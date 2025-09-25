#!/bin/bash
# Deo Blockchain Test Cleanup Script
# Moves any XML files from root to test_results directory

echo "Cleaning up test result files..."

# Create test_results directory if it doesn't exist
mkdir -p test_results

# Move any XML files from root to test_results directory
if ls *.xml 1> /dev/null 2>&1; then
    echo "Moving XML files to test_results directory..."
    mv *.xml test_results/
    echo "XML files moved successfully."
else
    echo "No XML files found in root directory."
fi

# Remove any temporary test files
echo "Cleaning up temporary files..."
rm -f debug_*.cpp test_*.cpp *_test.cpp 2>/dev/null || true

echo "Cleanup completed."


