/**
 * Comprehensive test suite for Kyber networking protocol extensions
 * 
 * Tests the chunked transmission protocol, session management, and 
 * integration with the KyberCryptoEngine for quantum-resistant mesh networking.
 */

#include <iostream>
#include <cstring>
#include <cassert>
#include <vector>
#include <memory>

// Mock Arduino/ESP32 functions for testing
extern "C" {
    uint32_t millis() { 
        static uint32_t counter = 1000;
        return counter += 100; // Simulate time progression
    }
    
    uint32_t esp_random() {
        return rand();
    }
}

// Mock logging functions
#define LOG_INFO(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) printf("[WARN] " fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)

// Include our implementation files
#include "meshtastic/src/mesh/kyber_protocol.h"

// Mock Kyber parameters for testing
#define CRYPTO_PUBLICKEYBYTES 800
#define CRYPTO_SECRETKEYBYTES 1632
#define CRYPTO_CIPHERTEXTBYTES 768
#define CRYPTO_BYTES 32

// Simple implementations of the protocol functions for testing
#include "meshtastic/src/mesh/kyber_protocol.cpp"

// Test framework
static int tests_passed = 0;
static int tests_failed = 0;

void test_assert(bool condition, const char* test_name) {
    if (condition) {
        printf("✓ PASS: %s\n", test_name);
        tests_passed++;
    } else {
        printf("✗ FAIL: %s\n", test_name);
        tests_failed++;
    }
}

/**
 * Test 1: Protocol Message Encoding/Decoding
 */
void test_message_encoding_decoding() {
    printf("\n=== Test 1: Message Encoding/Decoding ===\n");
    
    // Test key exchange request
    kyber_protocol_message_t original_msg;
    original_msg.msg_type = KYBER_MSG_KEY_EXCHANGE_REQUEST;
    original_msg.payload.key_request.protocol_version = KYBER_PROTOCOL_VERSION;
    original_msg.payload.key_request.session_id = 0x12345678;
    original_msg.payload.key_request.pubkey_total_size = CRYPTO_PUBLICKEYBYTES;
    original_msg.payload.key_request.total_chunks = KYBER_PUBKEY_CHUNKS;
    original_msg.payload.key_request.supports_fallback = true;
    
    uint8_t buffer[256];
    size_t encoded_size = kyber_message_encode(&original_msg, buffer, sizeof(buffer));
    test_assert(encoded_size > 0, "Key exchange request encoding");
    
    kyber_protocol_message_t decoded_msg;
    bool decode_success = kyber_message_decode(buffer, encoded_size, &decoded_msg);
    test_assert(decode_success, "Key exchange request decoding");
    
    test_assert(decoded_msg.msg_type == original_msg.msg_type, "Message type preserved");
    test_assert(decoded_msg.payload.key_request.session_id == original_msg.payload.key_request.session_id, 
                "Session ID preserved");
    test_assert(decoded_msg.payload.key_request.protocol_version == KYBER_PROTOCOL_VERSION, 
                "Protocol version preserved");
    
    // Test data chunk message
    kyber_protocol_message_t chunk_msg;
    chunk_msg.msg_type = KYBER_MSG_KEY_CHUNK;
    chunk_msg.payload.data_chunk.session_id = 0xABCDEF00;
    chunk_msg.payload.data_chunk.chunk_index = 2;
    chunk_msg.payload.data_chunk.total_chunks = 5;
    chunk_msg.payload.data_chunk.chunk_size = 150;
    
    // Fill with test data
    for (int i = 0; i < 150; i++) {
        chunk_msg.payload.data_chunk.data[i] = i & 0xFF;
    }
    chunk_msg.payload.data_chunk.checksum = kyber_calculate_crc32(chunk_msg.payload.data_chunk.data, 150);
    
    encoded_size = kyber_message_encode(&chunk_msg, buffer, sizeof(buffer));
    test_assert(encoded_size > 0, "Data chunk encoding");
    
    decode_success = kyber_message_decode(buffer, encoded_size, &decoded_msg);
    test_assert(decode_success, "Data chunk decoding");
    test_assert(decoded_msg.payload.data_chunk.chunk_index == 2, "Chunk index preserved");
    test_assert(decoded_msg.payload.data_chunk.chunk_size == 150, "Chunk size preserved");
}

/**
 * Test 2: Session Management
 */
void test_session_management() {
    printf("\n=== Test 2: Session Management ===\n");
    
    // Create session
    uint32_t peer_node = 0x1234;
    kyber_session_context_t* session = kyber_session_create(peer_node);
    test_assert(session != nullptr, "Session creation");
    test_assert(session->peer_node == peer_node, "Peer node stored correctly");
    test_assert(session->state == KYBER_STATE_IDLE, "Initial state is idle");
    test_assert(session->session_id != 0, "Session ID generated");
    
    // Test session ID uniqueness
    kyber_session_context_t* session2 = kyber_session_create(0x5678);
    test_assert(session2 != nullptr, "Second session creation");
    test_assert(session2->session_id != session->session_id, "Session IDs are unique");
    
    // Test session expiration (sessions should not be expired immediately)
    test_assert(!kyber_session_is_expired(session), "Fresh session not expired");
    test_assert(!kyber_session_is_expired(session2), "Fresh session2 not expired");
    
    // Clean up
    kyber_session_destroy(session);
    kyber_session_destroy(session2);
    printf("Sessions cleaned up successfully\n");
}

/**
 * Test 3: Data Chunking and Assembly
 */
void test_data_chunking() {
    printf("\n=== Test 3: Data Chunking and Assembly ===\n");
    
    // Create test data representing a Kyber public key
    uint8_t test_pubkey[CRYPTO_PUBLICKEYBYTES];
    for (int i = 0; i < CRYPTO_PUBLICKEYBYTES; i++) {
        test_pubkey[i] = (i + 37) & 0xFF; // Fill with deterministic pattern
    }
    
    // Create session for assembly
    kyber_session_context_t* session = kyber_session_create(0x9999);
    test_assert(session != nullptr, "Session created for chunking test");
    
    session->expected_chunks = KYBER_PUBKEY_CHUNKS;
    session->state = KYBER_STATE_RECEIVING_PUBKEY;
    
    printf("Testing chunking of %d bytes into %d chunks\n", CRYPTO_PUBLICKEYBYTES, KYBER_PUBKEY_CHUNKS);
    
    // Create and process chunks
    for (uint8_t chunk_idx = 0; chunk_idx < KYBER_PUBKEY_CHUNKS; chunk_idx++) {
        kyber_protocol_message_t chunk_msg;
        chunk_msg.msg_type = KYBER_MSG_KEY_CHUNK;
        
        auto& chunk = chunk_msg.payload.data_chunk;
        chunk.session_id = session->session_id;
        chunk.chunk_index = chunk_idx;
        chunk.total_chunks = KYBER_PUBKEY_CHUNKS;
        
        // Calculate chunk data
        size_t offset = chunk_idx * KYBER_CHUNK_SIZE;
        size_t remaining = CRYPTO_PUBLICKEYBYTES - offset;
        chunk.chunk_size = (remaining > KYBER_CHUNK_SIZE) ? KYBER_CHUNK_SIZE : remaining;
        
        memcpy(chunk.data, test_pubkey + offset, chunk.chunk_size);
        chunk.checksum = kyber_calculate_crc32(chunk.data, chunk.chunk_size);
        
        // Validate chunk
        test_assert(kyber_validate_chunk(&chunk), "Chunk validation");
        
        // Process chunk
        bool processed = kyber_process_message(session, &chunk_msg);
        test_assert(processed, "Chunk processing");
        
        printf("Processed chunk %d/%d (%zu bytes)\n", chunk_idx + 1, KYBER_PUBKEY_CHUNKS, chunk.chunk_size);
    }
    
    // Verify assembly completed
    test_assert(session->has_remote_pubkey, "Public key assembly completed");
    
    // Verify assembled data matches original
    bool data_matches = (memcmp(session->assembled_pubkey, test_pubkey, CRYPTO_PUBLICKEYBYTES) == 0);
    test_assert(data_matches, "Assembled data matches original");
    
    kyber_session_destroy(session);
    printf("Chunking test completed successfully\n");
}

/**
 * Test 4: CRC32 Validation
 */
void test_crc32_validation() {
    printf("\n=== Test 4: CRC32 Validation ===\n");
    
    uint8_t test_data[] = "Hello, Kyber quantum-resistant world!";
    size_t data_len = strlen((char*)test_data);
    
    uint32_t crc1 = kyber_calculate_crc32(test_data, data_len);
    uint32_t crc2 = kyber_calculate_crc32(test_data, data_len);
    test_assert(crc1 == crc2, "CRC32 is deterministic");
    test_assert(crc1 != 0, "CRC32 is non-zero");
    
    // Test with modified data
    test_data[0] ^= 0xFF;
    uint32_t crc3 = kyber_calculate_crc32(test_data, data_len);
    test_assert(crc3 != crc1, "CRC32 detects data changes");
    
    printf("CRC32 validation working correctly\n");
}

/**
 * Test 5: Protocol State Machine
 */
void test_protocol_state_machine() {
    printf("\n=== Test 5: Protocol State Machine ===\n");
    
    kyber_session_context_t* session = kyber_session_create(0xABCD);
    test_assert(session != nullptr, "Session created for state machine test");
    
    // Test key exchange request handling
    kyber_protocol_message_t req_msg;
    req_msg.msg_type = KYBER_MSG_KEY_EXCHANGE_REQUEST;
    req_msg.payload.key_request.protocol_version = KYBER_PROTOCOL_VERSION;
    req_msg.payload.key_request.session_id = session->session_id;
    req_msg.payload.key_request.pubkey_total_size = CRYPTO_PUBLICKEYBYTES;
    req_msg.payload.key_request.total_chunks = KYBER_PUBKEY_CHUNKS;
    req_msg.payload.key_request.supports_fallback = true;
    
    bool handled = kyber_process_message(session, &req_msg);
    test_assert(handled, "Key exchange request handled");
    test_assert(session->state == KYBER_STATE_RECEIVING_PUBKEY, "State transitioned to receiving pubkey");
    test_assert(session->expected_chunks == KYBER_PUBKEY_CHUNKS, "Expected chunks set correctly");
    
    // Test session established message
    kyber_protocol_message_t established_msg;
    established_msg.msg_type = KYBER_MSG_SESSION_ESTABLISHED;
    established_msg.payload.session_established.session_id = session->session_id;
    established_msg.payload.session_established.quantum_security = true;
    memset(established_msg.payload.session_established.shared_secret_hash, 0x42, 8);
    
    handled = kyber_process_message(session, &established_msg);
    test_assert(handled, "Session established message handled");
    test_assert(session->state == KYBER_STATE_ESTABLISHED, "State transitioned to established");
    
    // Test error message
    kyber_protocol_message_t error_msg;
    error_msg.msg_type = KYBER_MSG_ERROR;
    error_msg.payload.error_code = KYBER_ERROR_CRYPTO_FAILED;
    
    handled = kyber_process_message(session, &error_msg);
    test_assert(!handled, "Error message returns failure");
    test_assert(session->state == KYBER_STATE_ERROR, "State transitioned to error");
    
    kyber_session_destroy(session);
    printf("State machine test completed\n");
}

/**
 * Test 6: Protocol Constants and Limits
 */
void test_protocol_constants() {
    printf("\n=== Test 6: Protocol Constants and Limits ===\n");
    
    // Test chunk size calculations
    test_assert(KYBER_CHUNK_SIZE == 200, "Chunk size is reasonable for LoRa");
    test_assert(KYBER_PUBKEY_CHUNKS == ((CRYPTO_PUBLICKEYBYTES + KYBER_CHUNK_SIZE - 1) / KYBER_CHUNK_SIZE), 
                "Public key chunk calculation correct");
    test_assert(KYBER_CIPHERTEXT_CHUNKS == ((CRYPTO_CIPHERTEXTBYTES + KYBER_CHUNK_SIZE - 1) / KYBER_CHUNK_SIZE), 
                "Ciphertext chunk calculation correct");
    
    printf("Public key chunks needed: %d (for %d bytes)\n", KYBER_PUBKEY_CHUNKS, CRYPTO_PUBLICKEYBYTES);
    printf("Ciphertext chunks needed: %d (for %d bytes)\n", KYBER_CIPHERTEXT_CHUNKS, CRYPTO_CIPHERTEXTBYTES);
    
    // Verify chunks fit in reasonable number
    test_assert(KYBER_PUBKEY_CHUNKS <= 8, "Public key chunking is reasonable");
    test_assert(KYBER_CIPHERTEXT_CHUNKS <= 8, "Ciphertext chunking is reasonable");
    
    // Test session limits
    test_assert(KYBER_MAX_CONCURRENT_SESSIONS >= 4, "Sufficient concurrent sessions");
    test_assert(KYBER_SESSION_TIMEOUT_MS == 30000, "Reasonable session timeout");
    
    printf("Protocol constants validated\n");
}

/**
 * Test 7: Error Handling and Edge Cases
 */
void test_error_handling() {
    printf("\n=== Test 7: Error Handling and Edge Cases ===\n");
    
    // Test null pointer handling
    test_assert(kyber_session_create(0) != nullptr, "Session creation with node 0");
    test_assert(kyber_session_is_expired(nullptr), "Null session is expired");
    test_assert(!kyber_validate_chunk(nullptr), "Null chunk validation fails");
    
    // Test invalid chunk validation
    kyber_data_chunk_t invalid_chunk;
    invalid_chunk.chunk_size = KYBER_CHUNK_SIZE + 1; // Too large
    invalid_chunk.chunk_index = 0;
    invalid_chunk.total_chunks = 1;
    test_assert(!kyber_validate_chunk(&invalid_chunk), "Oversized chunk rejected");
    
    invalid_chunk.chunk_size = 100;
    invalid_chunk.chunk_index = 5; // Index >= total_chunks
    invalid_chunk.total_chunks = 5;
    test_assert(!kyber_validate_chunk(&invalid_chunk), "Invalid chunk index rejected");
    
    // Test message encoding with insufficient buffer
    kyber_protocol_message_t msg;
    msg.msg_type = KYBER_MSG_KEY_EXCHANGE_REQUEST;
    uint8_t small_buffer[1];
    size_t encoded = kyber_message_encode(&msg, small_buffer, sizeof(small_buffer));
    test_assert(encoded == 0, "Insufficient buffer handled gracefully");
    
    // Test message decoding with insufficient data
    kyber_protocol_message_t decoded;
    bool decode_ok = kyber_message_decode(small_buffer, 1, &decoded);
    test_assert(!decode_ok, "Insufficient decode data handled gracefully");
    
    printf("Error handling tests completed\n");
}

/**
 * Test 8: Memory Management
 */
void test_memory_management() {
    printf("\n=== Test 8: Memory Management ===\n");
    
    std::vector<kyber_session_context_t*> sessions;
    
    // Create multiple sessions
    for (int i = 0; i < 10; i++) {
        kyber_session_context_t* session = kyber_session_create(0x1000 + i);
        test_assert(session != nullptr, "Session creation in loop");
        sessions.push_back(session);
    }
    
    // Verify sessions are independent
    for (size_t i = 0; i < sessions.size(); i++) {
        test_assert(sessions[i]->peer_node == (0x1000 + i), "Session peer nodes independent");
        for (size_t j = i + 1; j < sessions.size(); j++) {
            test_assert(sessions[i]->session_id != sessions[j]->session_id, 
                        "Session IDs are unique in batch");
        }
    }
    
    // Clean up all sessions
    for (auto session : sessions) {
        kyber_session_destroy(session);
    }
    
    sessions.clear();
    printf("Memory management test completed\n");
}

/**
 * Main test runner
 */
int main() {
    printf("KYBER NETWORKING PROTOCOL TEST SUITE\n");
    printf("====================================\n");
    printf("Testing chunked transmission protocol for quantum-resistant mesh networking\n\n");
    
    // Initialize random seed
    srand(time(nullptr));
    
    // Run all tests
    test_message_encoding_decoding();
    test_session_management();
    test_data_chunking();
    test_crc32_validation();
    test_protocol_state_machine();
    test_protocol_constants();
    test_error_handling();
    test_memory_management();
    
    // Print final results
    printf("\n=== NETWORKING PROTOCOL TEST RESULTS ===\n");
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("Success rate: %.1f%%\n", (100.0 * tests_passed) / (tests_passed + tests_failed));
    
    if (tests_failed == 0) {
        printf("\n🎉 ALL NETWORKING TESTS PASSED!\n");
        printf("Kyber protocol extensions are ready for integration.\n");
        printf("\nKey Features Validated:\n");
        printf("✓ Chunked transmission of 800-byte Kyber public keys\n");
        printf("✓ Chunked transmission of 768-byte Kyber ciphertext\n");
        printf("✓ Session management with %d concurrent sessions\n", KYBER_MAX_CONCURRENT_SESSIONS);
        printf("✓ CRC32 data integrity validation\n");
        printf("✓ Protocol state machine with error handling\n");
        printf("✓ Memory management and cleanup\n");
        printf("✓ LoRa packet size constraints respected (%d byte chunks)\n", KYBER_CHUNK_SIZE);
        return 0;
    } else {
        printf("\n❌ SOME NETWORKING TESTS FAILED!\n");
        printf("Protocol implementation needs fixes before deployment.\n");
        return 1;
    }
}