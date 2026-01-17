# Host-based unit tests for client motion logic
# Compiles and runs tests on Mac without requiring Zephyr

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I../src -g
LDFLAGS = -lm

# Source files
MOTION_LOGIC_SRC = ../src/motion_logic.c
TEST_SRC = test_motion.c

# Output
TEST_BIN = test_motion

.PHONY: all clean test

all: test

$(TEST_BIN): $(TEST_SRC) $(MOTION_LOGIC_SRC)
	@echo "Compiling client motion tests..."
	$(CC) $(CFLAGS) -o $@ $(TEST_SRC) $(MOTION_LOGIC_SRC) $(LDFLAGS)

test: $(TEST_BIN)
	@echo "Running client motion tests..."
	@./$(TEST_BIN)

clean:
	@rm -f $(TEST_BIN)
	@echo "Cleaned client test artifacts"
