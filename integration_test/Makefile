# Integration Test Makefile
# Software-in-the-Loop (SIL) Testing Framework
# Compiles and runs integration tests with actual client/basestation logic

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -I. -I../client/src -I../basestation/src
LDFLAGS = -lm

# Source files
BLE_HAL_SRC = ble_hal.c
CLIENT_EMULATOR_SRC = client_emulator.c
BASESTATION_EMULATOR_SRC = basestation_emulator.c
TEST_SRC = test_integration.c

# Production source files (actual business logic)
CLIENT_LOGIC_SRC = ../client/src/motion_logic.c
BASESTATION_LOGIC_SRC = ../basestation/src/midi_logic.c

# Object files
OBJS = $(BLE_HAL_SRC:.c=.o) \
       $(CLIENT_EMULATOR_SRC:.c=.o) \
       $(BASESTATION_EMULATOR_SRC:.c=.o) \
       $(TEST_SRC:.c=.o) \
       motion_logic.o \
       midi_logic.o

# Target executable
TARGET = test_integration

# Default target
all: $(TARGET)

# Link test executable
$(TARGET): $(OBJS)
	@echo "Linking $@..."
	$(CC) $(OBJS) $(LDFLAGS) -o $@

# Compile source files
%.o: %.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Special rule for motion_logic (from client)
motion_logic.o: $(CLIENT_LOGIC_SRC)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Special rule for midi_logic (from basestation)
midi_logic.o: $(BASESTATION_LOGIC_SRC)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Run tests
run: $(TARGET)
	@echo ""
	@echo "Running integration tests..."
	@./$(TARGET)

# Clean build artifacts
clean:
	rm -f $(OBJS) $(TARGET)

# Help target
help:
	@echo "Integration Test Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all     - Build test executable (default)"
	@echo "  run     - Build and run tests"
	@echo "  clean   - Remove build artifacts"
	@echo "  help    - Show this help message"
	@echo ""
	@echo "Usage:"
	@echo "  make         # Build tests"
	@echo "  make run     # Build and run tests"
	@echo "  make clean   # Clean build"

.PHONY: all run clean help
