# Makefile for CRYSTALS-KYBER tests
# Builds and runs comprehensive test suite

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
INCLUDES = -Icomponents/common -Icomponents/kem -Icomponents/indcpa -Icomponents/fips202 \
           -Icomponents/poly -Icomponents/polyvec -Icomponents/ntt -Icomponents/reduce \
           -Icomponents/cbd -Icomponents/verify -Icomponents/randombytes -Icomponents/symmetric \
           -Icomponents/sha2 -Icomponents/aes256ctr

DEFINES = -DKYBER_90S -DKYBER_K=2

# Source files for Kyber implementation
KYBER_SOURCES = components/kem/kem.c \
                components/indcpa/indcpa.c \
                components/fips202/fips202.c \
                components/poly/poly.c \
                components/polyvec/polyvec.c \
                components/ntt/ntt.c \
                components/reduce/reduce.c \
                components/cbd/cbd.c \
                components/verify/verify.c \
                components/symmetric/symmetric-aes.c \
                components/symmetric/symmetric-shake.c \
                components/sha2/sha256.c \
                components/sha2/sha512.c \
                components/aes256ctr/aes256ctr.c

# Test files
TEST_SOURCES = test_kyber.c

# Build targets
all: test_kyber run_tests

test_kyber: $(TEST_SOURCES) $(KYBER_SOURCES)
	$(CC) $(CFLAGS) $(INCLUDES) $(DEFINES) -o $@ $^

run_tests: test_kyber
	@echo "Running CRYSTALS-KYBER test suite..."
	./test_kyber

# Performance test with optimizations
test_performance: $(TEST_SOURCES) $(KYBER_SOURCES)
	$(CC) $(CFLAGS) -O3 -DPERFORMANCE_ITERATIONS=10000 $(INCLUDES) $(DEFINES) -o $@ $^
	@echo "Running performance tests..."
	./test_performance

# Memory test with debugging
test_memory: $(TEST_SOURCES) $(KYBER_SOURCES)
	$(CC) $(CFLAGS) -g -fsanitize=address $(INCLUDES) $(DEFINES) -o $@ $^
	@echo "Running memory safety tests..."
	./test_memory

# Clean build artifacts
clean:
	rm -f test_kyber test_performance test_memory *.o

# Install test dependencies (for CI)
install_deps:
	@echo "Installing test dependencies..."
	# Add any required packages here

# Continuous integration target
ci: clean test_kyber run_tests test_performance test_memory
	@echo "All CI tests completed successfully!"

.PHONY: all run_tests test_performance test_memory clean install_deps ci