# DX7 Synthesizer Makefile

# Compiler and flags
CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2
INCLUDES = -I/Users/MWOLAK/homebrew/include
LIBS = -L/Users/MWOLAK/homebrew/lib -lsndfile -lm

# Target executable
TARGET = dx7synth

# Source files
SOURCES = main.c envelope.c oscillators.c algorithms.c
OBJECTS = $(SOURCES:.c=.o)
HEADERS = dx7.h

# Default target
all: $(TARGET)

# Build the main executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBS)

# Compile source files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)

# Install (copy to /usr/local/bin)
install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/

# Uninstall
uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)

# Create test patch and run example
test: $(TARGET)
	./$(TARGET) -n 60 -v 100 -d 2.0 -o test_epiano.wav epiano.patch
	@echo "Test complete. Generated test_epiano.wav"

# Test perfect loop functionality
test-loop: $(TARGET)
	@echo "Testing perfect loop with zero crossings..."
	./$(TARGET) -l 2 -n 28 -v 127 -o test_wobble_loop.wav wobble_bass.patch
	./$(TARGET) -l 4 -n 64 -v 110 -o test_lead_loop.wav huge_lead.patch
	@echo "Loop test complete. Check console output for zero crossing detection!"
	@echo "Generated: test_wobble_loop.wav and test_lead_loop.wav"

# Test different sample rates
test-rates: $(TARGET)
	./$(TARGET) -s 44100 -d 1.5 -n 60 -v 100 -o test_44k.wav epiano.patch
	./$(TARGET) -s 96000 -d 1.5 -n 60 -v 100 -o test_96k.wav epiano.patch
	@echo "Sample rate test complete. Generated test_44k.wav and test_96k.wav"

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

# Check for required dependencies
check-deps:
	@echo "Checking for required dependencies..."
	@pkg-config --exists sndfile && echo "✓ libsndfile found" || echo "✗ libsndfile not found"
	@test -f /Users/MWOLAK/homebrew/include/sndfile.h && echo "✓ sndfile.h found" || echo "✗ sndfile.h not found"
	@test -f /Users/MWOLAK/homebrew/lib/libsndfile.dylib && echo "✓ libsndfile.dylib found" || echo "✗ libsndfile.dylib not found (checking for .a)" 
	@test -f /Users/MWOLAK/homebrew/lib/libsndfile.a && echo "✓ libsndfile.a found" || echo "✗ libsndfile library not found"

# Help target
help:
	@echo "DX7 Synthesizer Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  all        - Build the synthesizer (default)"
	@echo "  clean      - Remove build artifacts"
	@echo "  debug      - Build with debug symbols"
	@echo "  install    - Install to /usr/local/bin"
	@echo "  uninstall  - Remove from /usr/local/bin"
	@echo "  test       - Build and run basic test"
	@echo "  test-loop  - Test perfect loop functionality"
	@echo "  test-rates - Test different sample rates"
	@echo "  check-deps - Check for required dependencies"
	@echo "  help       - Show this help message"
	@echo ""
	@echo "Usage examples:"
	@echo "  make && ./dx7synth -n 64 -o mytest.wav epiano.patch"
	@echo "  ./dx7synth -s 44100 -d 2.0 -o brass.wav brass1.patch"
	@echo "  ./dx7synth -l 4 -o perfect_wobble.wav wobble_bass.patch"

.PHONY: all clean install uninstall test test-loop test-rates debug check-deps help
