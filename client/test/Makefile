# Host-based unit tests for client motion logic
# Compiles and runs tests on Mac without requiring Zephyr

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I../src -g
LDFLAGS = -lm

# Source files
MOTION_LOGIC_SRC = ../src/motion_logic.c

# Test files
TEST_MOTION = test_motion
TEST_FILTERS = test_filters

# All tests
TESTS = $(TEST_MOTION) $(TEST_FILTERS)

.PHONY: all clean test

all: test

$(TEST_MOTION): test_motion.c $(MOTION_LOGIC_SRC)
	@echo "Compiling motion logic tests..."
	$(CC) $(CFLAGS) -o $@ test_motion.c $(MOTION_LOGIC_SRC) $(LDFLAGS)

$(TEST_FILTERS): test_filters.c $(MOTION_LOGIC_SRC)
	@echo "Compiling filter tests..."
	$(CC) $(CFLAGS) -o $@ test_filters.c $(LDFLAGS)

test: $(TESTS)
	@echo "\n========================================="
	@echo "Running all client tests..."
	@echo "=========================================\n"
	@echo "--- Motion Logic Tests ---"
	@./$(TEST_MOTION)
	@echo "\n--- Filter Tests ---"
	@./$(TEST_FILTERS)
	@echo "\n✓ All client tests completed successfully!"

clean:
	@rm -f $(TESTS)
	@rm -rf *.dSYM
	@echo "Cleaned client test artifacts"
