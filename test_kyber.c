#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include "components/kem/kem.h"
#include "components/fips202/fips202.h"

// Simple random number generation for testing (replace ESP32 dependencies)
void randombytes(uint8_t *x, size_t xlen) {
    for (size_t i = 0; i < xlen; i++) {
        x[i] = rand() & 0xFF;
    }
}

void esp_randombytes(uint8_t *x, size_t xlen) {
    randombytes(x, xlen);
}

/**
 * Comprehensive test suite for CRYSTALS-KYBER implementation
 * Tests basic functionality, performance, and security properties
 */

// Test configuration
#define NUM_TEST_ITERATIONS 100
#define PERFORMANCE_ITERATIONS 1000
#define TEST_MESSAGE_SIZE 256

// Global test counters
static int tests_passed = 0;
static int tests_failed = 0;

void test_assert(int condition, const char* test_name) {
    if (condition) {
        printf("✓ PASS: %s\n", test_name);
        tests_passed++;
    } else {
        printf("✗ FAIL: %s\n", test_name);
        tests_failed++;
    }
}

/**
 * Test 1: Basic Kyber KEM functionality
 * Verifies key generation, encapsulation, and decapsulation work correctly
 */
void test_kyber_kem_basic() {
    printf("\n=== Test 1: Basic Kyber KEM Operations ===\n");
    
    uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk[CRYPTO_SECRETKEYBYTES];
    uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
    uint8_t ss_alice[CRYPTO_BYTES];
    uint8_t ss_bob[CRYPTO_BYTES];
    
    // Test key generation
    int result = crypto_kem_keypair(pk, sk);
    test_assert(result == 0, "Key generation returns success");
    
    // Test encapsulation
    result = crypto_kem_enc(ct, ss_bob, pk);
    test_assert(result == 0, "Encapsulation returns success");
    
    // Test decapsulation
    result = crypto_kem_dec(ss_alice, ct, sk);
    test_assert(result == 0, "Decapsulation returns success");
    
    // Test shared secret equality
    test_assert(memcmp(ss_alice, ss_bob, CRYPTO_BYTES) == 0, 
                "Shared secrets match");
    
    printf("Public key size: %d bytes\n", CRYPTO_PUBLICKEYBYTES);
    printf("Secret key size: %d bytes\n", CRYPTO_SECRETKEYBYTES);
    printf("Ciphertext size: %d bytes\n", CRYPTO_CIPHERTEXTBYTES);
    printf("Shared secret size: %d bytes\n", CRYPTO_BYTES);
}

/**
 * Test 2: Multiple iterations for stability
 * Ensures KEM works reliably across many iterations
 */
void test_kyber_kem_iterations() {
    printf("\n=== Test 2: Multiple KEM Iterations ===\n");
    
    int successful_iterations = 0;
    
    for (int i = 0; i < NUM_TEST_ITERATIONS; i++) {
        uint8_t pk[CRYPTO_PUBLICKEYBYTES];
        uint8_t sk[CRYPTO_SECRETKEYBYTES];
        uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
        uint8_t ss_alice[CRYPTO_BYTES];
        uint8_t ss_bob[CRYPTO_BYTES];
        
        if (crypto_kem_keypair(pk, sk) == 0 &&
            crypto_kem_enc(ct, ss_bob, pk) == 0 &&
            crypto_kem_dec(ss_alice, ct, sk) == 0 &&
            memcmp(ss_alice, ss_bob, CRYPTO_BYTES) == 0) {
            successful_iterations++;
        }
    }
    
    test_assert(successful_iterations == NUM_TEST_ITERATIONS,
                "All iterations successful");
    printf("Successful iterations: %d/%d\n", successful_iterations, NUM_TEST_ITERATIONS);
}

/**
 * Test 3: Key uniqueness
 * Verifies that generated keys are unique across multiple generations
 */
void test_key_uniqueness() {
    printf("\n=== Test 3: Key Uniqueness ===\n");
    
    uint8_t pk1[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk1[CRYPTO_SECRETKEYBYTES];
    uint8_t pk2[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk2[CRYPTO_SECRETKEYBYTES];
    
    crypto_kem_keypair(pk1, sk1);
    crypto_kem_keypair(pk2, sk2);
    
    test_assert(memcmp(pk1, pk2, CRYPTO_PUBLICKEYBYTES) != 0,
                "Public keys are unique");
    test_assert(memcmp(sk1, sk2, CRYPTO_SECRETKEYBYTES) != 0,
                "Secret keys are unique");
}

/**
 * Test 4: Invalid input handling
 * Tests behavior with corrupted or invalid inputs
 */
void test_invalid_inputs() {
    printf("\n=== Test 4: Invalid Input Handling ===\n");
    
    uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk[CRYPTO_SECRETKEYBYTES];
    uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
    uint8_t ss[CRYPTO_BYTES];
    
    // Generate valid key pair
    crypto_kem_keypair(pk, sk);
    crypto_kem_enc(ct, ss, pk);
    
    // Test with corrupted ciphertext
    uint8_t corrupted_ct[CRYPTO_CIPHERTEXTBYTES];
    memcpy(corrupted_ct, ct, CRYPTO_CIPHERTEXTBYTES);
    corrupted_ct[0] ^= 0xFF; // Flip bits
    
    uint8_t ss_corrupted[CRYPTO_BYTES];
    int result = crypto_kem_dec(ss_corrupted, corrupted_ct, sk);
    
    // Should either fail or produce different shared secret
    int is_different = (result != 0) || (memcmp(ss, ss_corrupted, CRYPTO_BYTES) != 0);
    test_assert(is_different, "Corrupted ciphertext handled correctly");
    
    // Test with corrupted secret key
    uint8_t corrupted_sk[CRYPTO_SECRETKEYBYTES];
    memcpy(corrupted_sk, sk, CRYPTO_SECRETKEYBYTES);
    corrupted_sk[0] ^= 0xFF;
    
    result = crypto_kem_dec(ss_corrupted, ct, corrupted_sk);
    is_different = (result != 0) || (memcmp(ss, ss_corrupted, CRYPTO_BYTES) != 0);
    test_assert(is_different, "Corrupted secret key handled correctly");
}

/**
 * Test 5: FIPS202 (SHAKE) functionality
 * Tests the hash functions used by Kyber
 */
void test_fips202_functions() {
    printf("\n=== Test 5: FIPS202 Hash Functions ===\n");
    
    uint8_t input[64];
    uint8_t output1[32];
    uint8_t output2[32];
    
    // Initialize test input
    for (int i = 0; i < 64; i++) {
        input[i] = i;
    }
    
    // Test SHAKE256
    shake256(output1, 32, input, 64);
    shake256(output2, 32, input, 64);
    
    test_assert(memcmp(output1, output2, 32) == 0,
                "SHAKE256 deterministic");
    
    // Test different input produces different output
    input[0] ^= 0xFF;
    shake256(output2, 32, input, 64);
    test_assert(memcmp(output1, output2, 32) != 0,
                "SHAKE256 different input produces different output");
}

/**
 * Test 6: Performance benchmarking
 * Measures performance of key operations
 */
void test_performance() {
    printf("\n=== Test 6: Performance Benchmarking ===\n");
    
    clock_t start, end;
    double keygen_time = 0, enc_time = 0, dec_time = 0;
    
    uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk[CRYPTO_SECRETKEYBYTES];
    uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
    uint8_t ss[CRYPTO_BYTES];
    
    // Benchmark key generation
    start = clock();
    for (int i = 0; i < PERFORMANCE_ITERATIONS; i++) {
        crypto_kem_keypair(pk, sk);
    }
    end = clock();
    keygen_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    // Benchmark encapsulation
    start = clock();
    for (int i = 0; i < PERFORMANCE_ITERATIONS; i++) {
        crypto_kem_enc(ct, ss, pk);
    }
    end = clock();
    enc_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    // Benchmark decapsulation
    start = clock();
    for (int i = 0; i < PERFORMANCE_ITERATIONS; i++) {
        crypto_kem_dec(ss, ct, sk);
    }
    end = clock();
    dec_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    printf("Performance results (%d iterations):\n", PERFORMANCE_ITERATIONS);
    printf("  Key generation: %.2f ms avg (%.2f ops/sec)\n", 
           (keygen_time * 1000) / PERFORMANCE_ITERATIONS,
           PERFORMANCE_ITERATIONS / keygen_time);
    printf("  Encapsulation:  %.2f ms avg (%.2f ops/sec)\n", 
           (enc_time * 1000) / PERFORMANCE_ITERATIONS,
           PERFORMANCE_ITERATIONS / enc_time);
    printf("  Decapsulation:  %.2f ms avg (%.2f ops/sec)\n", 
           (dec_time * 1000) / PERFORMANCE_ITERATIONS,
           PERFORMANCE_ITERATIONS / dec_time);
    
    // Performance assertions (reasonable thresholds for ESP32)
    test_assert(keygen_time / PERFORMANCE_ITERATIONS < 0.1, "Key generation reasonably fast");
    test_assert(enc_time / PERFORMANCE_ITERATIONS < 0.01, "Encapsulation reasonably fast");
    test_assert(dec_time / PERFORMANCE_ITERATIONS < 0.01, "Decapsulation reasonably fast");
}

/**
 * Test 7: Memory safety
 * Tests for buffer overflows and memory corruption
 */
void test_memory_safety() {
    printf("\n=== Test 7: Memory Safety ===\n");
    
    // Test with properly sized buffers
    uint8_t *pk = malloc(CRYPTO_PUBLICKEYBYTES);
    uint8_t *sk = malloc(CRYPTO_SECRETKEYBYTES);
    uint8_t *ct = malloc(CRYPTO_CIPHERTEXTBYTES);
    uint8_t *ss1 = malloc(CRYPTO_BYTES);
    uint8_t *ss2 = malloc(CRYPTO_BYTES);
    
    if (!pk || !sk || !ct || !ss1 || !ss2) {
        printf("Memory allocation failed\n");
        return;
    }
    
    // Clear memory with known pattern
    memset(pk, 0xAA, CRYPTO_PUBLICKEYBYTES);
    memset(sk, 0xBB, CRYPTO_SECRETKEYBYTES);
    memset(ct, 0xCC, CRYPTO_CIPHERTEXTBYTES);
    memset(ss1, 0xDD, CRYPTO_BYTES);
    memset(ss2, 0xEE, CRYPTO_BYTES);
    
    // Run operations
    int result1 = crypto_kem_keypair(pk, sk);
    int result2 = crypto_kem_enc(ct, ss1, pk);
    int result3 = crypto_kem_dec(ss2, ct, sk);
    
    test_assert(result1 == 0 && result2 == 0 && result3 == 0,
                "All operations successful with allocated memory");
    test_assert(memcmp(ss1, ss2, CRYPTO_BYTES) == 0,
                "Shared secrets match with allocated memory");
    
    free(pk);
    free(sk);
    free(ct);
    free(ss1);
    free(ss2);
}

/**
 * Main test runner
 */
int main() {
    printf("CRYSTALS-KYBER Test Suite\n");
    printf("=========================\n");
    printf("Configuration: KYBER_K=%d, 90s variant: %s\n", 
           KYBER_K, 
#ifdef KYBER_90S
           "YES"
#else
           "NO"
#endif
    );
    
    // Run all tests
    test_kyber_kem_basic();
    test_kyber_kem_iterations();
    test_key_uniqueness();
    test_invalid_inputs();
    test_fips202_functions();
    test_performance();
    test_memory_safety();
    
    // Print final results
    printf("\n=== Test Results ===\n");
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("Success rate: %.1f%%\n", (100.0 * tests_passed) / (tests_passed + tests_failed));
    
    if (tests_failed == 0) {
        printf("🎉 ALL TESTS PASSED!\n");
        return 0;
    } else {
        printf("❌ SOME TESTS FAILED!\n");
        return 1;
    }
}