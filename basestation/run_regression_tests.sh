#!/bin/bash
#
# GuitarAcc Basestation Regression Test Suite
#
# Runs all verification tests in sequence to validate firmware functionality.
# Tests include: patch storage, export/import, and configuration management.
#
# Usage:
#   ./run_regression_tests.sh           # Run automated tests only
#   ./run_regression_tests.sh -i        # Run automated + interactive tests
#   ./run_regression_tests.sh --interactive
#

set -e  # Exit on first failure

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Parse command line arguments
RUN_INTERACTIVE=false
while [[ $# -gt 0 ]]; do
    case $1 in
        -i|--interactive)
            RUN_INTERACTIVE=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [-i|--interactive] [-h|--help]"
            echo ""
            echo "Options:"
            echo "  -i, --interactive  Include interactive tests (requires user presence)"
            echo "  -h, --help        Show this help message"
            echo ""
            echo "Automated tests:"
            echo "  - Patch Count & Persistence"
            echo "  - Configuration Export/Import"
            echo ""
            echo "Interactive tests (with -i flag):"
            echo "  - Accelerometer Deadzone (requires moving client device)"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use -h or --help for usage information"
            exit 1
            ;;
    esac
done

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test results
TESTS_PASSED=0
TESTS_FAILED=0
FAILED_TESTS=()

echo "========================================================================"
echo "  GuitarAcc Basestation - Regression Test Suite"
echo "========================================================================"
echo ""

# Function to run a test
run_test() {
    local test_name=$1
    local test_script=$2
    
    echo "------------------------------------------------------------------------"
    echo "Running: $test_name"
    echo "------------------------------------------------------------------------"
    
    if python3 "$test_script"; then
        echo -e "${GREEN}✓ $test_name PASSED${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        echo ""
        return 0
    else
        echo -e "${RED}✗ $test_name FAILED${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        FAILED_TESTS+=("$test_name")
        echo ""
        return 1
    fi
}

# Run all tests
echo "Starting test suite..."
echo ""

# Automated Tests
echo "========================================================================"
echo "  AUTOMATED TESTS"
echo "========================================================================"
echo ""

# Test 1: Patch Count and Persistence
run_test "Patch Count & Persistence" "./verify_patch_count.py" || true

# Test 2: Configuration Export/Import
run_test "Configuration Export/Import" "./verify_export_import.py" || true

# Interactive Tests (only if -i flag is set)
if [ "$RUN_INTERACTIVE" = true ]; then
    echo ""
    echo "========================================================================"
    echo "  INTERACTIVE TESTS"
    echo "========================================================================"
    echo ""
    echo -e "${YELLOW}NOTE: Interactive tests require user presence to move the client device${NC}"
    echo ""
    
    # Test 3: Accelerometer Deadzone
    run_test "Accelerometer Deadzone" "./test_accel_deadzone.py" || true
else
    echo ""
    echo "========================================================================"
    echo -e "${YELLOW}Skipping interactive tests (use -i flag to include them)${NC}"
    echo "========================================================================"
fi

# Summary
echo "========================================================================"
echo "  TEST SUITE SUMMARY"
echo "========================================================================"
echo ""
echo "Total Tests Run: $((TESTS_PASSED + TESTS_FAILED))"
echo -e "Tests Passed: ${GREEN}${TESTS_PASSED}${NC}"
echo -e "Tests Failed: ${RED}${TESTS_FAILED}${NC}"
echo ""

if [ $TESTS_FAILED -gt 0 ]; then
    echo -e "${RED}Failed Tests:${NC}"
    for test in "${FAILED_TESTS[@]}"; do
        echo "  - $test"
    done
    echo ""
    echo "========================================================================"
    echo -e "${RED}✗✗✗ REGRESSION TEST SUITE FAILED ✗✗✗${NC}"
    echo "========================================================================"
    exit 1
else
    echo "========================================================================"
    echo -e "${GREEN}✓✓✓ ALL REGRESSION TESTS PASSED ✓✓✓${NC}"
    echo "========================================================================"
    exit 0
fi
