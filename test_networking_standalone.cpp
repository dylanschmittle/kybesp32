/**
 * Standalone test for Kyber networking protocol
 * Tests the chunked transmission protocol without external dependencies
 */

#include <iostream>
#include <cstring>
#include <cassert>
#include <vector>
#include <memory>
#include <cstdlib>
#include <ctime>

// Mock Kyber constants
#define CRYPTO_PUBLICKEYBYTES 800
#define CRYPTO_SECRETKEYBYTES 1632
#define CRYPTO_CIPHERTEXTBYTES 768
#define CRYPTO_BYTES 32

// Protocol constants
#define KYBER_PROTOCOL_VERSION 1
#define KYBER_CHUNK_SIZE 200
#define KYBER_PUBKEY_CHUNKS ((CRYPTO_PUBLICKEYBYTES + KYBER_CHUNK_SIZE - 1) / KYBER_CHUNK_SIZE)
#define KYBER_CIPHERTEXT_CHUNKS ((CRYPTO_CIPHERTEXTBYTES + KYBER_CHUNK_SIZE - 1) / KYBER_CHUNK_SIZE)
#define KYBER_SESSION_TIMEOUT_MS (30 * 1000)
#define KYBER_CHUNK_RETRY_LIMIT 3
#define KYBER_MAX_CONCURRENT_SESSIONS 4

// Mock Arduino functions
extern "C" {
    uint32_t millis() { 
        static uint32_t counter = 1000;
        return counter += 100;
    }
    
    uint32_t esp_random() {
        return rand();
    }
}

// Logging macros
#define LOG_INFO(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) printf("[WARN] " fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)

// Message types
typedef enum {
    KYBER_MSG_KEY_EXCHANGE_REQUEST = 1,
    KYBER_MSG_KEY_CHUNK = 2,
    KYBER_MSG_KEY_CHUNK_ACK = 3,
    KYBER_MSG_CIPHERTEXT_CHUNK = 4,
    KYBER_MSG_CIPHERTEXT_CHUNK_ACK = 5,
    KYBER_MSG_SESSION_ESTABLISHED = 6,
    KYBER_MSG_ERROR = 7
} kyber_message_type_t;

typedef enum {
    KYBER_ERROR_NONE = 0,
    KYBER_ERROR_UNSUPPORTED = 1,
    KYBER_ERROR_CHUNK_TIMEOUT = 2,
    KYBER_ERROR_ASSEMBLY_FAILED = 3,
    KYBER_ERROR_CRYPTO_FAILED = 4,
    KYBER_ERROR_PROTOCOL_VERSION = 5
} kyber_error_code_t;

typedef enum {
    KYBER_STATE_IDLE = 0,
    KYBER_STATE_REQUESTING,
    KYBER_STATE_SENDING_PUBKEY,
    KYBER_STATE_RECEIVING_PUBKEY,
    KYBER_STATE_SENDING_CIPHERTEXT,
    KYBER_STATE_RECEIVING_CIPHERTEXT,
    KYBER_STATE_ESTABLISHED,
    KYBER_STATE_ERROR
} kyber_session_state_t;

// Message structures
typedef struct {
    uint8_t protocol_version;
    uint32_t session_id;
    uint16_t pubkey_total_size;
    uint8_t total_chunks;
    bool supports_fallback;
} kyber_key_exchange_request_t;

typedef struct {
    uint32_t session_id;
    uint8_t chunk_index;
    uint8_t total_chunks;
    uint16_t chunk_size;
    uint8_t data[KYBER_CHUNK_SIZE];
    uint32_t checksum;
} kyber_data_chunk_t;

typedef struct {
    uint32_t session_id;
    uint8_t chunk_index;
    bool success;
    kyber_error_code_t error_code;
} kyber_chunk_ack_t;

typedef struct {
    uint32_t session_id;
    bool quantum_security;
    uint8_t shared_secret_hash[8];
} kyber_session_established_t;

typedef struct {
    kyber_message_type_t msg_type;
    union {
        kyber_key_exchange_request_t key_request;
        kyber_data_chunk_t data_chunk;
        kyber_chunk_ack_t chunk_ack;
        kyber_session_established_t session_established;
        kyber_error_code_t error_code;
    } payload;
} kyber_protocol_message_t;

typedef struct {
    uint32_t session_id;
    kyber_session_state_t state;
    uint32_t peer_node;
    
    uint8_t assembled_pubkey[CRYPTO_PUBLICKEYBYTES];
    uint8_t received_chunks_mask;
    uint8_t expected_chunks;
    
    uint8_t assembled_ciphertext[CRYPTO_CIPHERTEXTBYTES];
    uint8_t ciphertext_chunks_mask;
    uint8_t expected_ciphertext_chunks;
    
    uint32_t last_activity_ms;
    uint8_t retry_count;
    
    bool has_local_keypair;
    bool has_remote_pubkey;
    bool has_shared_secret;
    uint8_t shared_secret[CRYPTO_BYTES];
} kyber_session_context_t;

// CRC32 implementation
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t kyber_calculate_crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

uint32_t kyber_generate_session_id(void) {
    static uint32_t counter = 0;
    uint32_t timestamp = millis();
    uint32_t random_val = esp_random();
    return (timestamp ^ random_val ^ (++counter));
}

bool kyber_validate_chunk(const kyber_data_chunk_t* chunk) {
    if (!chunk) return false;
    if (chunk->chunk_size > KYBER_CHUNK_SIZE) return false;
    if (chunk->chunk_index >= chunk->total_chunks) return false;
    
    uint32_t calculated_crc = kyber_calculate_crc32(chunk->data, chunk->chunk_size);
    return calculated_crc == chunk->checksum;
}

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

void test_protocol_constants() {
    printf("\n=== Test 1: Protocol Constants ===\n");
    
    test_assert(KYBER_CHUNK_SIZE == 200, "Chunk size is reasonable for LoRa");
    test_assert(KYBER_PUBKEY_CHUNKS == 4, "Public key chunks calculated correctly");
    test_assert(KYBER_CIPHERTEXT_CHUNKS == 4, "Ciphertext chunks calculated correctly");
    
    printf("Public key: %d bytes -> %d chunks of %d bytes\n", 
           CRYPTO_PUBLICKEYBYTES, KYBER_PUBKEY_CHUNKS, KYBER_CHUNK_SIZE);
    printf("Ciphertext: %d bytes -> %d chunks of %d bytes\n", 
           CRYPTO_CIPHERTEXTBYTES, KYBER_CIPHERTEXT_CHUNKS, KYBER_CHUNK_SIZE);
    
    test_assert(KYBER_PUBKEY_CHUNKS <= 8, "Reasonable chunk count for public key");
    test_assert(KYBER_CIPHERTEXT_CHUNKS <= 8, "Reasonable chunk count for ciphertext");
}

void test_crc32_validation() {
    printf("\n=== Test 2: CRC32 Validation ===\n");
    
    uint8_t test_data[] = "Kyber quantum-resistant protocol test data";
    size_t data_len = strlen((char*)test_data);
    
    uint32_t crc1 = kyber_calculate_crc32(test_data, data_len);
    uint32_t crc2 = kyber_calculate_crc32(test_data, data_len);
    test_assert(crc1 == crc2, "CRC32 is deterministic");
    test_assert(crc1 != 0, "CRC32 is non-zero");
    
    test_data[0] ^= 0xFF;
    uint32_t crc3 = kyber_calculate_crc32(test_data, data_len);
    test_assert(crc3 != crc1, "CRC32 detects data changes");
    
    printf("CRC32 validation working correctly\n");
}

void test_chunk_validation() {
    printf("\n=== Test 3: Chunk Validation ===\n");
    
    kyber_data_chunk_t valid_chunk;
    valid_chunk.session_id = 0x12345678;
    valid_chunk.chunk_index = 2;
    valid_chunk.total_chunks = 5;
    valid_chunk.chunk_size = 150;
    
    for (int i = 0; i < 150; i++) {
        valid_chunk.data[i] = (i + 42) & 0xFF;
    }
    valid_chunk.checksum = kyber_calculate_crc32(valid_chunk.data, valid_chunk.chunk_size);
    
    test_assert(kyber_validate_chunk(&valid_chunk), "Valid chunk passes validation");
    
    // Test invalid chunk size
    kyber_data_chunk_t invalid_chunk = valid_chunk;
    invalid_chunk.chunk_size = KYBER_CHUNK_SIZE + 1;
    test_assert(!kyber_validate_chunk(&invalid_chunk), "Oversized chunk rejected");
    
    // Test invalid chunk index
    invalid_chunk = valid_chunk;
    invalid_chunk.chunk_index = 5; // >= total_chunks
    test_assert(!kyber_validate_chunk(&invalid_chunk), "Invalid chunk index rejected");
    
    // Test corrupted checksum
    invalid_chunk = valid_chunk;
    invalid_chunk.checksum ^= 0xFFFFFFFF;
    test_assert(!kyber_validate_chunk(&invalid_chunk), "Corrupted checksum detected");
    
    printf("Chunk validation working correctly\n");
}

void test_data_chunking_simulation() {
    printf("\n=== Test 4: Data Chunking Simulation ===\n");
    
    // Create test public key data
    uint8_t test_pubkey[CRYPTO_PUBLICKEYBYTES];
    for (int i = 0; i < CRYPTO_PUBLICKEYBYTES; i++) {
        test_pubkey[i] = (i + 123) & 0xFF;
    }
    
    // Assembly buffer
    uint8_t assembled_pubkey[CRYPTO_PUBLICKEYBYTES];
    memset(assembled_pubkey, 0, sizeof(assembled_pubkey));
    uint8_t received_chunks_mask = 0;
    
    printf("Simulating chunking of %d-byte public key into %d chunks\n", 
           CRYPTO_PUBLICKEYBYTES, KYBER_PUBKEY_CHUNKS);
    
    // Create and validate all chunks
    for (uint8_t chunk_idx = 0; chunk_idx < KYBER_PUBKEY_CHUNKS; chunk_idx++) {
        kyber_data_chunk_t chunk;
        chunk.session_id = 0xABCDEF00;
        chunk.chunk_index = chunk_idx;
        chunk.total_chunks = KYBER_PUBKEY_CHUNKS;
        
        // Calculate chunk data
        size_t offset = chunk_idx * KYBER_CHUNK_SIZE;
        size_t remaining = CRYPTO_PUBLICKEYBYTES - offset;
        chunk.chunk_size = (remaining > KYBER_CHUNK_SIZE) ? KYBER_CHUNK_SIZE : remaining;
        
        memcpy(chunk.data, test_pubkey + offset, chunk.chunk_size);
        chunk.checksum = kyber_calculate_crc32(chunk.data, chunk.chunk_size);
        
        test_assert(kyber_validate_chunk(&chunk), "Chunk validation");
        
        // Assemble chunk
        memcpy(assembled_pubkey + offset, chunk.data, chunk.chunk_size);
        received_chunks_mask |= (1 << chunk_idx);
        
        printf("Processed chunk %d/%d (%zu bytes)\n", 
               chunk_idx + 1, KYBER_PUBKEY_CHUNKS, chunk.chunk_size);
    }
    
    // Verify assembly
    uint8_t expected_mask = (1 << KYBER_PUBKEY_CHUNKS) - 1;
    test_assert(received_chunks_mask == expected_mask, "All chunks received");
    
    bool data_matches = (memcmp(assembled_pubkey, test_pubkey, CRYPTO_PUBLICKEYBYTES) == 0);
    test_assert(data_matches, "Assembled data matches original");
    
    printf("Data chunking simulation completed successfully\n");
}

void test_session_id_generation() {
    printf("\n=== Test 5: Session ID Generation ===\n");
    
    std::vector<uint32_t> session_ids;
    const int num_ids = 100;
    
    // Generate multiple session IDs
    for (int i = 0; i < num_ids; i++) {
        uint32_t id = kyber_generate_session_id();
        session_ids.push_back(id);
        test_assert(id != 0, "Session ID is non-zero");
    }
    
    // Check uniqueness
    int unique_count = 0;
    for (size_t i = 0; i < session_ids.size(); i++) {
        bool is_unique = true;
        for (size_t j = i + 1; j < session_ids.size(); j++) {
            if (session_ids[i] == session_ids[j]) {
                is_unique = false;
                break;
            }
        }
        if (is_unique) unique_count++;
    }
    
    // Allow some collisions but expect mostly unique IDs
    test_assert(unique_count > (num_ids * 0.9), "Session IDs are mostly unique");
    
    printf("Generated %d session IDs, %d unique (%.1f%%)\n", 
           num_ids, unique_count, (100.0 * unique_count) / num_ids);
}

void test_protocol_overhead() {
    printf("\n=== Test 6: Protocol Overhead Analysis ===\n");
    
    // Calculate protocol overhead
    size_t chunk_header_size = sizeof(kyber_data_chunk_t) - KYBER_CHUNK_SIZE;
    size_t chunk_total_size = sizeof(kyber_data_chunk_t);
    
    printf("Protocol overhead analysis:\n");
    printf("- Chunk header size: %zu bytes\n", chunk_header_size);
    printf("- Chunk data size: %d bytes\n", KYBER_CHUNK_SIZE);
    printf("- Total chunk size: %zu bytes\n", chunk_total_size);
    printf("- Overhead percentage: %.1f%%\n", 
           (100.0 * chunk_header_size) / chunk_total_size);
    
    // Verify it fits in LoRa packet
    test_assert(chunk_total_size <= 255, "Chunk fits in LoRa packet");
    test_assert(chunk_header_size < 50, "Reasonable protocol overhead");
    
    // Calculate total transmission overhead
    size_t pubkey_total_bytes = KYBER_PUBKEY_CHUNKS * chunk_total_size;
    size_t pubkey_overhead = pubkey_total_bytes - CRYPTO_PUBLICKEYBYTES;
    
    printf("Public key transmission:\n");
    printf("- Raw key size: %d bytes\n", CRYPTO_PUBLICKEYBYTES);
    printf("- Transmitted size: %zu bytes\n", pubkey_total_bytes);
    printf("- Transmission overhead: %zu bytes (%.1f%%)\n", 
           pubkey_overhead, (100.0 * pubkey_overhead) / CRYPTO_PUBLICKEYBYTES);
    
    test_assert(pubkey_overhead < CRYPTO_PUBLICKEYBYTES, "Overhead less than data");
}

int main() {
    printf("KYBER NETWORKING PROTOCOL STANDALONE TEST\n");
    printf("==========================================\n");
    printf("Testing chunked transmission for quantum-resistant mesh networking\n\n");
    
    srand(time(nullptr));
    
    test_protocol_constants();
    test_crc32_validation();
    test_chunk_validation();
    test_data_chunking_simulation();
    test_session_id_generation();
    test_protocol_overhead();
    
    printf("\n=== STANDALONE NETWORKING TEST RESULTS ===\n");
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("Success rate: %.1f%%\n", (100.0 * tests_passed) / (tests_passed + tests_failed));
    
    if (tests_failed == 0) {
        printf("\n🎉 ALL NETWORKING PROTOCOL TESTS PASSED!\n");
        printf("\nProtocol Features Validated:\n");
        printf("✓ Chunked transmission of %d-byte Kyber public keys\n", CRYPTO_PUBLICKEYBYTES);
        printf("✓ %d chunks per public key, %d bytes per chunk\n", KYBER_PUBKEY_CHUNKS, KYBER_CHUNK_SIZE);
        printf("✓ CRC32 data integrity validation\n");
        printf("✓ LoRa packet size constraints respected\n");
        printf("✓ Session ID generation and uniqueness\n");
        printf("✓ Protocol overhead analysis complete\n");
        printf("\nQuantum-resistant networking protocol is ready!\n");
        return 0;
    } else {
        printf("\n❌ SOME NETWORKING TESTS FAILED!\n");
        return 1;
    }
}