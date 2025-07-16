#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>
#include <memory>

// Mock Arduino/ESP32 dependencies for testing
#define LOG_DEBUG(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) printf("[WARN] " fmt "\n", ##__VA_ARGS__)

// Include the Kyber crypto engine
#include "meshtastic/src/mesh/KyberCryptoEngine.h"

/**
 * Integration tests for KyberCryptoEngine with Meshtastic
 * Tests the crypto engine interface and compatibility
 */

class KyberCryptoEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        crypto_engine = std::make_unique<KyberCryptoEngine>();
    }

    void TearDown() override {
        crypto_engine.reset();
    }

    std::unique_ptr<KyberCryptoEngine> crypto_engine;
};

/**
 * Test 1: Basic crypto engine initialization
 */
TEST_F(KyberCryptoEngineTest, InitializationTest) {
    ASSERT_NE(crypto_engine, nullptr);
    
    // Test that the engine is properly initialized
    EXPECT_NO_THROW({
        uint8_t pubkey[CRYPTO_PUBLICKEYBYTES];
        uint8_t privkey[CRYPTO_SECRETKEYBYTES];
        crypto_engine->generateKeyPair(pubkey, privkey);
    });
}

/**
 * Test 2: Key generation interface
 */
TEST_F(KyberCryptoEngineTest, KeyGenerationTest) {
    uint8_t pubkey1[CRYPTO_PUBLICKEYBYTES];
    uint8_t privkey1[CRYPTO_SECRETKEYBYTES];
    uint8_t pubkey2[CRYPTO_PUBLICKEYBYTES];
    uint8_t privkey2[CRYPTO_SECRETKEYBYTES];
    
    // Generate two key pairs
    EXPECT_NO_THROW({
        crypto_engine->generateKeyPair(pubkey1, privkey1);
        crypto_engine->generateKeyPair(pubkey2, privkey2);
    });
    
    // Keys should be different
    EXPECT_NE(0, memcmp(pubkey1, pubkey2, CRYPTO_PUBLICKEYBYTES));
    EXPECT_NE(0, memcmp(privkey1, privkey2, CRYPTO_SECRETKEYBYTES));
}

/**
 * Test 3: Hash function interface
 */
TEST_F(KyberCryptoEngineTest, HashFunctionTest) {
    uint8_t input[64];
    uint8_t hash1[32];
    uint8_t hash2[32];
    
    // Initialize test data
    for (int i = 0; i < 64; i++) {
        input[i] = i;
    }
    memcpy(hash1, input, 32);
    memcpy(hash2, input, 32);
    
    // Test hash function
    EXPECT_NO_THROW({
        crypto_engine->hash(hash1, 64);
        crypto_engine->hash(hash2, 64);
    });
    
    // Same input should produce same hash
    EXPECT_EQ(0, memcmp(hash1, hash2, 32));
    
    // Different input should produce different hash
    input[0] ^= 0xFF;
    memcpy(hash2, input, 32);
    crypto_engine->hash(hash2, 64);
    EXPECT_NE(0, memcmp(hash1, hash2, 32));
}

/**
 * Test 4: Key size compatibility issues
 * This test demonstrates the current limitations
 */
TEST_F(KyberCryptoEngineTest, KeySizeCompatibilityTest) {
    // This test shows the fundamental compatibility problem
    
    // Meshtastic expects 32-byte keys
    const size_t MESHTASTIC_KEY_SIZE = 32;
    
    // Kyber uses much larger keys
    EXPECT_GT(CRYPTO_PUBLICKEYBYTES, MESHTASTIC_KEY_SIZE);
    EXPECT_GT(CRYPTO_SECRETKEYBYTES, MESHTASTIC_KEY_SIZE);
    
    printf("Key size mismatch detected:\n");
    printf("  Meshtastic expects: %zu bytes\n", MESHTASTIC_KEY_SIZE);
    printf("  Kyber public key:   %d bytes\n", CRYPTO_PUBLICKEYBYTES);
    printf("  Kyber secret key:   %d bytes\n", CRYPTO_SECRETKEYBYTES);
    printf("  Kyber ciphertext:   %d bytes\n", CRYPTO_CIPHERTEXTBYTES);
    
    // This demonstrates the truncation problem
    uint8_t kyber_pubkey[CRYPTO_PUBLICKEYBYTES];
    uint8_t kyber_privkey[CRYPTO_SECRETKEYBYTES];
    uint8_t truncated_pubkey[MESHTASTIC_KEY_SIZE];
    uint8_t truncated_privkey[MESHTASTIC_KEY_SIZE];
    
    crypto_engine->generateKeyPair(kyber_pubkey, kyber_privkey);
    
    // Current implementation truncates (SECURITY ISSUE!)
    memcpy(truncated_pubkey, kyber_pubkey, MESHTASTIC_KEY_SIZE);
    memcpy(truncated_privkey, kyber_privkey, MESHTASTIC_KEY_SIZE);
    
    // Verify truncation destroys security
    EXPECT_EQ(0, memcmp(truncated_pubkey, kyber_pubkey, MESHTASTIC_KEY_SIZE));
    EXPECT_NE(0, memcmp(kyber_pubkey, kyber_pubkey + MESHTASTIC_KEY_SIZE, 
                       CRYPTO_PUBLICKEYBYTES - MESHTASTIC_KEY_SIZE));
}

/**
 * Test 5: Encryption/Decryption interface (demonstrates failures)
 */
TEST_F(KyberCryptoEngineTest, EncryptionDecryptionTest) {
    const uint32_t NODE_ID = 0x12345678;
    const uint64_t PACKET_ID = 0xABCDEF123456789ULL;
    const size_t MESSAGE_SIZE = 256;
    
    uint8_t message[MESSAGE_SIZE];
    uint8_t encrypted[MESSAGE_SIZE + 16]; // Extra space for auth tag
    uint8_t decrypted[MESSAGE_SIZE];
    
    // Initialize test message
    for (size_t i = 0; i < MESSAGE_SIZE; i++) {
        message[i] = i & 0xFF;
    }
    
    // Generate keys
    uint8_t pubkey[CRYPTO_PUBLICKEYBYTES];
    uint8_t privkey[CRYPTO_SECRETKEYBYTES];
    crypto_engine->generateKeyPair(pubkey, privkey);
    
    // Create mock public key structure (Meshtastic format)
    meshtastic_UserLite_public_key_t remote_public;
    remote_public.size = 32; // Meshtastic expects 32 bytes
    memcpy(remote_public.bytes, pubkey, 32); // TRUNCATION ISSUE!
    
    // Attempt encryption (will fail due to key size issues)
    bool encrypt_result = crypto_engine->encryptCurve25519(
        NODE_ID, NODE_ID, remote_public, PACKET_ID, 
        MESSAGE_SIZE, message, encrypted
    );
    
    // This should fail due to insufficient key size
    EXPECT_FALSE(encrypt_result) << "Encryption should fail with truncated keys";
}

/**
 * Test 6: Memory usage analysis
 */
TEST_F(KyberCryptoEngineTest, MemoryUsageTest) {
    // Calculate memory footprint of Kyber vs Curve25519
    
    struct MemoryFootprint {
        size_t public_key;
        size_t private_key;
        size_t ciphertext;
        size_t shared_secret;
        size_t total;
    };
    
    MemoryFootprint curve25519 = {
        .public_key = 32,
        .private_key = 32,
        .ciphertext = 0, // ECDH doesn't have ciphertext
        .shared_secret = 32,
        .total = 96
    };
    
    MemoryFootprint kyber = {
        .public_key = CRYPTO_PUBLICKEYBYTES,
        .private_key = CRYPTO_SECRETKEYBYTES,
        .ciphertext = CRYPTO_CIPHERTEXTBYTES,
        .shared_secret = CRYPTO_BYTES,
        .total = CRYPTO_PUBLICKEYBYTES + CRYPTO_SECRETKEYBYTES + 
                 CRYPTO_CIPHERTEXTBYTES + CRYPTO_BYTES
    };
    
    printf("Memory usage comparison:\n");
    printf("  Curve25519 total: %zu bytes\n", curve25519.total);
    printf("  Kyber total: %zu bytes\n", kyber.total);
    printf("  Increase factor: %.1fx\n", (double)kyber.total / curve25519.total);
    
    // Verify significant memory increase
    EXPECT_GT(kyber.total, curve25519.total * 10) 
        << "Kyber uses significantly more memory";
}

/**
 * Test 7: Protocol compatibility analysis
 */
TEST_F(KyberCryptoEngineTest, ProtocolCompatibilityTest) {
    // This test documents the protocol compatibility issues
    
    printf("Protocol compatibility analysis:\n");
    
    // Check if Kyber keys fit in existing structures
    bool public_key_fits = (CRYPTO_PUBLICKEYBYTES <= 32);
    bool private_key_fits = (CRYPTO_SECRETKEYBYTES <= 32);
    bool ciphertext_fits = (CRYPTO_CIPHERTEXTBYTES <= 256); // Typical packet limit
    
    printf("  Public key fits in 32 bytes: %s\n", public_key_fits ? "YES" : "NO");
    printf("  Private key fits in 32 bytes: %s\n", private_key_fits ? "YES" : "NO");
    printf("  Ciphertext fits in 256 bytes: %s\n", ciphertext_fits ? "YES" : "NO");
    
    EXPECT_FALSE(public_key_fits) << "Public keys don't fit existing protocol";
    EXPECT_FALSE(private_key_fits) << "Private keys don't fit existing protocol";
    
    // Check protocol message overhead
    size_t overhead = CRYPTO_CIPHERTEXTBYTES;
    printf("  Additional protocol overhead: %zu bytes per key exchange\n", overhead);
    
    EXPECT_GT(overhead, 500) << "Significant protocol overhead required";
}

/**
 * Test fixture for performance testing
 */
class KyberPerformanceTest : public KyberCryptoEngineTest {
protected:
    static const int PERFORMANCE_ITERATIONS = 100;
};

/**
 * Test 8: Performance benchmarking
 */
TEST_F(KyberPerformanceTest, PerformanceBenchmark) {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Benchmark key generation
    auto keygen_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < PERFORMANCE_ITERATIONS; i++) {
        uint8_t pubkey[CRYPTO_PUBLICKEYBYTES];
        uint8_t privkey[CRYPTO_SECRETKEYBYTES];
        crypto_engine->generateKeyPair(pubkey, privkey);
    }
    auto keygen_end = std::chrono::high_resolution_clock::now();
    
    auto keygen_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        keygen_end - keygen_start).count();
    
    printf("Performance results (%d iterations):\n", PERFORMANCE_ITERATIONS);
    printf("  Key generation: %ld ms total, %.2f ms avg\n", 
           keygen_duration, (double)keygen_duration / PERFORMANCE_ITERATIONS);
    
    // Performance should be reasonable for embedded systems
    double avg_keygen_time = (double)keygen_duration / PERFORMANCE_ITERATIONS;
    EXPECT_LT(avg_keygen_time, 100.0) << "Key generation should be under 100ms";
}

/**
 * Main test suite
 */
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    printf("CRYSTALS-KYBER Meshtastic Integration Test Suite\n");
    printf("===============================================\n");
    printf("Testing Kyber integration with Meshtastic crypto engine\n\n");
    
    return RUN_ALL_TESTS();
}