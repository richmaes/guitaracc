# Makefile for host-based testing of basestation functions

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -O0 -I../src
TARGET_MIDI = test_midi_cc
TARGET_MAPPING = test_accel_mapping
TEST_MIDI_SRC = test_midi_cc.c
TEST_MAPPING_SRC = test_accel_mapping.c
MIDI_LOGIC_SRC = ../src/midi_logic.c
ACCEL_MAPPING_SRC = ../src/accel_mapping.c
SOURCES_MIDI = $(TEST_MIDI_SRC) $(MIDI_LOGIC_SRC) $(ACCEL_MAPPING_SRC)
SOURCES_MAPPING = $(TEST_MAPPING_SRC) $(ACCEL_MAPPING_SRC)

.PHONY: all clean test run help

all: $(TARGET_MIDI) $(TARGET_MAPPING)

$(TARGET_MIDI): $(SOURCES_MIDI)
	@echo "Building MIDI test (with actual embedded source)..."
	$(CC) $(CFLAGS) -o $(TARGET_MIDI) $(SOURCES_MIDI)
	@echo "✓ Build complete: ./$(TARGET_MIDI)"

$(TARGET_MAPPING): $(SOURCES_MAPPING)
	@echo "Building Accelerometer Mapping test..."
	$(CC) $(CFLAGS) -o $(TARGET_MAPPING) $(SOURCES_MAPPING) -lm
	@echo "✓ Build complete: ./$(TARGET_MAPPING)"

test: $(TARGET_MIDI) $(TARGET_MAPPING)
	@echo ""
	@echo "Running MIDI tests..."
	@./$(TARGET_MIDI)
	@echo ""
	@echo "Running Accelerometer Mapping tests..."
	@./$(TARGET_MAPPING)

run: test

clean:
	@echo "Cleaning build artifacts..."
	rm -f $(TARGET_MIDI) $(TARGET_MAPPING)
	rm -rf $(TARGET_MIDI).dSYM $(TARGET_MAPPING).dSYM
	@echo "✓ Clean complete"

help:
	@echo "Host-based test targets:"
	@echo "  make        - Build all test executables"
	@echo "  make test   - Build and run all tests"
	@echo "  make clean  - Remove build artifacts"
	@echo "  make help   - Show this help message"
