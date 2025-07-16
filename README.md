# Kybesp32: CRYSTALS-KYBER for ESP32 + Quantum-Resistant Meshtastic

**Authors:** Fabian Segatz & Muhammad Ihsan Al Hafiz  
**Extended by:** Post-quantum mesh networking integration

This project provides an efficient implementation of the **CRYSTALS-KYBER** Key Encapsulation Mechanism (KEM) on ESP32 microcontrollers, along with an integration into **Meshtastic** firmware for quantum-resistant mesh networking.

## 🛡️ **What is CRYSTALS-KYBER?**

**CRYSTALS-KYBER** is a **NIST-standardized** post-quantum cryptographic algorithm designed to be secure against attacks by quantum computers. As quantum computing advances, current cryptographic methods like RSA and elliptic curve cryptography will become vulnerable. Kyber provides a quantum-resistant alternative for secure key exchange.

## 🚀 **Project Components**

### 1. **ESP32 Kyber Implementation** (Original Research)
Based on the research project from [II2202, Research Methodology and Scientific Writing](https://www.kth.se/student/kurser/kurs/II2202?l=en) at KTH Stockholm, this implementation optimizes CRYSTALS-Kyber for ESP32 constraints.

**Credits:** Bos et al. [official reference implementation](https://github.com/pq-crystals/kyber/blob/master/README.md) of [Kyber](https://www.pq-crystals.org/kyber/).

### 2. **Quantum-Resistant Meshtastic Integration** (New)
Integration of Kyber into [Meshtastic](https://meshtastic.org) firmware, replacing Curve25519 ECDH with post-quantum key exchange for secure mesh networking.

**⚠️ IMPORTANT:** This integration is currently a **proof-of-concept** and has significant limitations (see below).

## 📋 **Quick Start**

### **Testing Kyber Implementation**
```bash
# Build and run comprehensive test suite
make test_kyber
./test_kyber

# Performance testing
make test_performance
./test_performance

# Memory safety testing  
make test_memory
./test_memory
```

### **Building Meshtastic with Kyber**
```bash
# Navigate to Meshtastic submodule
cd meshtastic

# Build for T-Deck (basic)
platformio run -e t-deck

# Build for T-Deck with TFT (recommended)
platformio run -e t-deck-tft

# Flash to device
platformio run -e t-deck-tft --target upload
```

## 🔧 **Build Instructions**

### **ESP32 Development Setup**
1. Install [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)
2. Use [VSCode ESP-IDF extension](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/vscode-setup.html) (recommended)
3. Configure for your ESP32 development board

### **Meshtastic Development**
1. Install [PlatformIO](https://platformio.org/)
2. Clone this repository with submodules:
   ```bash
   git clone --recursive https://github.com/your-username/kybesp32.git
   ```
3. Build targets available:
   - `t-deck`: Basic T-Deck support
   - `t-deck-tft`: Full TFT display support
   - Compatible with T-Deck Plus

## Folder contents

Below is short overview of the files in the project folder.

```
├── components
│   ├── indcpa
│   ├── kem
│   ├── ...
├── main
│   ├── CMakeLists.txt
│   └── main.c
├── .gitignore
├── CMakeLists.txt
└── README.md           This is the file you are currently reading
```

The project **kybesp32** contains a source file in C language [main.c](main/main.c). The file is located in folder [main](main). This file contains the program entry point `app_main()`.

The components folder holds every primitive, that is required for the Kyber algorithm. We want to highlight the component `indcpa` which holds all the functionality for the Public Key Encryption (PKE) and the component `kem`, that defines the Key Encapsulation Mechanism (KEM).

ESP-IDF projects are built using CMake. The project build configuration is contained in `CMakeLists.txt`
files that provide set of directives and instructions describing the project's source files and targets
(executable, library, or both).

## ⚠️ **Critical Limitations & Current Status**

### **🚨 Meshtastic Integration Status: PROOF-OF-CONCEPT ONLY**

**The Meshtastic integration COMPILES but DOES NOT WORK for actual communication.** Here's why:

#### **1. Key Size Incompatibility**
```
Meshtastic Protocol:  32-byte public keys
CRYSTALS-Kyber:      800-byte public keys (25x larger!)
Current Implementation: TRUNCATES keys to 32 bytes (DESTROYS SECURITY!)
```

#### **2. Protocol Incompatibility**
- **Missing ciphertext transmission**: Kyber requires 768-byte ciphertext exchange
- **No protocol fields**: Meshtastic has no message fields for Kyber data
- **Different key exchange**: KEM vs ECDH requires protocol redesign
- **Storage issues**: Node database can't store large keys

#### **3. What Actually Works**
✅ **Kyber crypto libraries** compile and run on ESP32  
✅ **Build system** integrates Kyber components  
✅ **Performance** is acceptable (26K ops/sec key generation)  
✅ **Memory usage** fits in ESP32 constraints  

❌ **Network communication** is broken  
❌ **Key exchange** fails due to truncation  
❌ **Protocol compatibility** with existing nodes  
❌ **Backward compatibility** (requires full network migration)  

### **🎯 What This Project Demonstrates**

1. **Feasibility**: CRYSTALS-Kyber CAN run efficiently on ESP32
2. **Performance**: Suitable for embedded post-quantum crypto
3. **Integration challenges**: Protocol changes needed for practical deployment
4. **Future roadmap**: Foundation for quantum-resistant mesh networking

## 📊 **Performance Results**

### **Original ESP32 Benchmarks** (from research paper)

We used ESP-IDF v5.0 with GCC 8.4.0 on ESP32-S3-DevKitC-1 (160 MHz):

| Implementation| Algorithm         | Cycle count   | Speedup ratio |
| -----------   | -----------       | -----------   | ----------- |
| Scenario 1    | Key Generation    | 2.439.083     | 1x |
|               | Encapsulation     | 2.736.256     | 1x |
|               | Decapsulation     | 2.736.256     | 1x |
| Scenario 2    | Key Generation    | 2.007.689     | 1.21x |
|               | Encapsulation     | 2.243.652     | 1.22x |
|               | Decapsulation     | 2.471.286     | 1.20x |
| Scenario 3    | Key Generation    | 1.414.389     | 1.72x |
|               | Encapsulation     | 1.490.784     | 1.84x |
|               | Decapsulation     | 1.756.638     | 1.69x |

### **Current Test Results** (Kyber512-90s on modern hardware)
```
Performance results (1000 iterations):
  Key generation: 0.04 ms avg (26,636 ops/sec)
  Encapsulation:  0.04 ms avg (22,896 ops/sec)  
  Decapsulation:  0.05 ms avg (21,879 ops/sec)
```

### **Meshtastic Build Results**
| Target | Status | RAM Usage | Flash Usage | Firmware Size |
|--------|--------|-----------|-------------|---------------|
| t-deck | ✅ SUCCESS | 31.9% | 31.9% | 2.09 MB |
| t-deck-tft | ✅ SUCCESS | 39.0% | 51.8% | 3.39 MB |

## Test results

We used the ESP-IDF development framework in version 5.0. The compilation is done by the framework’s default compiler, which is GCC in version 8.4.0. We did not activate any compiler optimization. The execution was performed on a [ESP32-S3-DevKitC-1 development board](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/hw-reference/esp32s3/user-guide-devkitc-1.html) with a clock frequency of 160 MHz. The compiler settings were kept default as they were specified when setting up the project with the ESP-IDF.

In the file `CMakeLists.txt` there are a number of preprocessor defines that can be used to activate either the dual-core optimizations or the SHA and ACC accelerators.

After building the firmware and flashing it to the device, the clock tick cylcle count measurments for the Kyber KEM keypair generation, encapsulation and decapsulation get reported via the serial interface of the dev board.

We measured 3 scenarios:

1. Scenario 1: Single-core
```
add_compile_definitions("KYBER_90S")
add_compile_definitions("KYBER_K=2")
add_compile_definitions("SHA_ACC=0")
add_compile_definitions("AES_ACC=0")
add_compile_definitions("INDCPA_KEYPAIR_DUAL=0")
add_compile_definitions("INDCPA_ENC_DUAL=0")
add_compile_definitions("INDCPA_DEC_DUAL=0")
```
2. Scenario 2: Dual-core
```
add_compile_definitions("KYBER_90S")
add_compile_definitions("KYBER_K=2")
add_compile_definitions("SHA_ACC=0")
add_compile_definitions("AES_ACC=0")
add_compile_definitions("INDCPA_KEYPAIR_DUAL=1")
add_compile_definitions("INDCPA_ENC_DUAL=1")
add_compile_definitions("INDCPA_DEC_DUAL=0")
```
3. Scenario 3: Dual-core and accelerator
```
add_compile_definitions("KYBER_90S")
add_compile_definitions("KYBER_K=2")
add_compile_definitions("SHA_ACC=1")
add_compile_definitions("AES_ACC=1")
add_compile_definitions("INDCPA_KEYPAIR_DUAL=1")
add_compile_definitions("INDCPA_ENC_DUAL=1")
add_compile_definitions("INDCPA_DEC_DUAL=0")
```

## 🚀 **Future Work & Roadmap**

### **Phase 1: Protocol Design (Required for Production)**
- [ ] **Design new Kyber-compatible protocol** for key exchange
- [ ] **Update Meshtastic protobuf definitions** for large keys
- [ ] **Create migration strategy** from Curve25519 to Kyber
- [ ] **Implement proper key storage** for 800-byte keys

### **Phase 2: Implementation (Major Rewrite)**
- [ ] **Rewrite CryptoEngine** without key truncation
- [ ] **Update NodeDB** for large key storage
- [ ] **Add ciphertext transmission** in mesh messages
- [ ] **Implement compatibility layer** for mixed networks

### **Phase 3: Deployment (Breaking Changes)**
- [ ] **Coordinated network migration** (no backward compatibility)
- [ ] **Key regeneration** on all nodes
- [ ] **Protocol version negotiation**
- [ ] **Extensive testing** with real hardware

### **Research Opportunities**
- [ ] **Hybrid schemes**: Combine Kyber with existing crypto
- [ ] **Compression techniques**: Reduce key/ciphertext sizes
- [ ] **Protocol optimization**: Minimize post-quantum overhead
- [ ] **Performance optimization**: Further ESP32 optimizations

## 🛠️ **Usage & Testing**

### **Testing the Kyber Implementation**
```bash
# Run comprehensive tests
make run_tests

# Test results should show:
# ✓ All basic KEM operations work
# ✓ Key uniqueness and security properties
# ✓ Performance within acceptable limits
# ✓ Memory safety verified
```

### **Analyzing the Integration**
```bash
# Build Meshtastic with Kyber (compiles but limited functionality)
cd meshtastic
platformio run -e t-deck-tft

# View memory usage
platformio run -e t-deck-tft --verbose | grep -E "RAM:|Flash:"
```

### **Development & Contributions**
- **File issues**: Report bugs or suggest improvements
- **Protocol design**: Help design quantum-resistant mesh protocols
- **Testing**: Test on different ESP32 variants
- **Documentation**: Improve setup and usage docs

## 📚 **Technical References**

- **CRYSTALS-Kyber**: [Official specification](https://pq-crystals.org/kyber/data/kyber-specification-round3-20210804.pdf)
- **NIST Post-Quantum**: [Standardization process](https://csrc.nist.gov/projects/post-quantum-cryptography)
- **Meshtastic**: [Protocol documentation](https://meshtastic.org/docs/overview)
- **ESP32**: [Technical reference](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/index.html)

## 🏆 **Original Performance Results**

The following table shows our results for Kyber512 in the 90s variant:

| Implementation| Algorithm         | Cycle count   | Speedup ratio |
| -----------   | -----------       | -----------   | ----------- |
| Scenario 1    | Key Generation    | 2.439.083     | 1x |
|               | Encapsulation     | 2.736.256     | 1x |
|               | Decapsulation     | 2.736.256     | 1x |
| Scenario 2    | Key Generation    | 2.007.689     | 1.21x |
|               | Encapsulation     | 2.243.652     | 1.22x |
|               | Decapsulation     | 2.471.286     | 1.20x |
| Scenario 3    | Key Generation    | 1.414.389     | 1.72x |
|               | Encapsulation     | 1.490.784     | 1.84x |
|               | Decapsulation     | 1.756.638     | 1.69x |

## 🙏 **Acknowledgments**

- **Original research**: Fabian Segatz & Muhammad Ihsan Al Hafiz (KTH Stockholm)
- **Supervision**: [Assoc. Prof. Masoumeh (Azin) Ebrahimi](https://people.kth.se/~mebr/)
- **Kyber reference**: Bos et al. [official implementation](https://github.com/pq-crystals/kyber)
- **Meshtastic project**: [Meshtastic community](https://meshtastic.org)

## ⚖️ **License & Disclaimer**

This project is provided for research and educational purposes. The Meshtastic integration is a proof-of-concept and should not be used in production environments without significant additional development work.

**Security Note**: The current integration has known security vulnerabilities due to key truncation. Do not use for actual secure communications.

