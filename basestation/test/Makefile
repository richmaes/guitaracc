# Makefile for host-based testing of basestation functions

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -O0 -I../src
TARGET = test_midi_cc
TEST_SRC = test_midi_cc.c
MIDI_LOGIC_SRC = ../src/midi_logic.c
SOURCES = $(TEST_SRC) $(MIDI_LOGIC_SRC)

.PHONY: all clean test run

all: $(TARGET)

$(TARGET): $(SOURCES)
	@echo "Building host-based test (with actual embedded source)..."
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)
	@echo "✓ Build complete: ./$(TARGET)"
	@echo "✓ Using source: $(MIDI_LOGIC_SRC)"

test: $(TARGET)
	@echo ""
	@echo "Running tests..."
	@./$(TARGET)

run: test

clean:
	@echo "Cleaning build artifacts..."
	rm -f $(TARGET)
	rm -rf $(TARGET).dSYM
	@echo "✓ Clean complete"

help:
	@echo "Host-based test targets:"
	@echo "  make        - Build the test executable"
	@echo "  make test   - Build and run tests"
	@echo "  make clean  - Remove build artifacts"
	@echo "  make help   - Show this help message"
