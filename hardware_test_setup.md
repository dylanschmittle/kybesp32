# T-Deck Hardware Test Setup Guide

## Prerequisites

### Hardware Required
- 2x LilyGO T-Deck devices (ESP32-S3 based)
- 2x USB-C cables
- Computer with 2+ USB ports
- Optional: External antennas for better range testing

### Software Requirements
```bash
# Install required Python packages
pip install pyserial

# Ensure PlatformIO is installed
pip install platformio

# Or use existing PlatformIO installation
```

## Hardware Setup Instructions

### 1. T-Deck Device Preparation

**Device 1 (Alice):**
1. Connect T-Deck #1 to USB port
2. Hold BOOT button while connecting to enter flash mode
3. Verify device appears in system (check `ls /dev/cu.*` on macOS)

**Device 2 (Bob):**
1. Connect T-Deck #2 to different USB port  
2. Hold BOOT button while connecting to enter flash mode
3. Verify device appears in system

### 2. Verify Device Detection
```bash
# Check for connected devices
python3 test_two_t_decks.py --help

# Test device detection (dry run)
python3 -c "
import serial.tools.list_ports
ports = serial.tools.list_ports.comports()
for port in ports:
    if any(kw in str(port.description).lower() for kw in ['cp210', 'esp32', 'serial']):
        print(f'Found: {port.device} - {port.description}')
"
```

### 3. Expected Device Names
- **macOS**: `/dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART*`
- **Linux**: `/dev/ttyUSB*` or `/dev/ttyACM*`
- **Windows**: `COM*`

## Test Execution

### Quick Test (Automated)
```bash
# Run complete automated test
make test_hardware

# Or run Python script directly
python3 test_two_t_decks.py
```

### Manual Step-by-Step Test
```bash
# 1. Flash both devices
make flash_t_decks

# 2. Monitor both devices
make monitor_t_decks

# 3. Run communication test
python3 test_two_t_decks.py --skip-flash
```

### Advanced Testing Options
```bash
# Verbose output (see all serial data)
python3 test_two_t_decks.py --verbose

# Extended test duration
python3 test_two_t_decks.py --test-duration 180

# Skip flashing (devices already have firmware)
python3 test_two_t_decks.py --skip-flash
```

## Expected Test Results

### Successful Test Output
```
[10:30:15] INFO: Starting Kyber T-Deck Test Suite...
[10:30:15] INFO: Found potential T-Deck: /dev/cu.usbserial-1 - CP2102 USB to UART Bridge
[10:30:15] INFO: Found potential T-Deck: /dev/cu.usbserial-2 - CP2102 USB to UART Bridge
[10:30:15] INFO: Selected T-Deck devices: ['/dev/cu.usbserial-1', '/dev/cu.usbserial-2']

=== FIRMWARE FLASHING ===
[10:30:16] INFO: Flashing T-Deck-1 on /dev/cu.usbserial-1...
[10:31:45] INFO: ✅ T-Deck-1 flashed successfully
[10:31:55] INFO: Flashing T-Deck-2 on /dev/cu.usbserial-2...
[10:33:24] INFO: ✅ T-Deck-2 flashed successfully

=== MESH COMMUNICATION TEST ===
[10:33:34] INFO: Testing mesh communication between T-Decks...
[10:33:34] INFO: Monitoring mesh communication for 90 seconds...

=== MESH COMMUNICATION TEST RESULTS ===

T-Deck-1:
  Kyber Initialized: True
  Key Generation: True
  Key Exchange Attempts: 2
  Sessions Established: 1
  Quantum Security: True
  Mesh Packets: 15
  Errors: 0

T-Deck-2:
  Kyber Initialized: True
  Key Generation: True
  Key Exchange Attempts: 1
  Sessions Established: 1
  Quantum Security: True
  Mesh Packets: 12
  Errors: 0

=== SUCCESS CRITERIA ===
  both_init: ✅ PASS
  both_keygen: ✅ PASS
  key_exchanges: ✅ PASS
  low_errors: ✅ PASS

🎉 ALL TESTS PASSED!
Kyber quantum-resistant mesh networking is working correctly.
```

### Key Log Messages to Look For

**Kyber Initialization:**
```
[INFO] KyberCryptoEngine created
[INFO] Kyber keypair generated successfully
```

**Key Exchange:**
```
[INFO] Initiating Kyber key exchange with node 1234
[DEBUG] Sent Kyber public key chunk 1/4 to node 1234
[DEBUG] Received Kyber key chunk 1/4
[INFO] Kyber session established with node 1234 - quantum security active
```

**Mesh Communication:**
```
[INFO] Packet sent to node 1234 with quantum encryption
[INFO] Packet received from node 1234 with quantum decryption
```

## Troubleshooting

### Device Not Found
```bash
# Check USB connections
lsusb  # Linux
system_profiler SPUSBDataType  # macOS

# Check permissions (Linux)
sudo usermod -a -G dialout $USER
# Log out and back in

# Check device manager (Windows)
# Look for COM ports or USB Serial devices
```

### Flash Failures
```bash
# Put device in flash mode manually:
# 1. Hold BOOT button
# 2. Press and release RESET button
# 3. Release BOOT button
# 4. Try flashing again

# Check PlatformIO installation
platformio --version

# Manual flash
cd meshtastic
platformio run -e t-deck-tft --target upload --upload-port /dev/YOUR_PORT
```

### No Mesh Communication
1. **Check LoRa frequency**: Ensure both devices use same region
2. **Check distance**: Keep devices close (< 10m) for initial testing
3. **Check serial output**: Look for Kyber initialization messages
4. **Check node IDs**: Devices should detect each other

### Serial Monitor Issues
```bash
# Test serial connection manually
screen /dev/YOUR_PORT 115200
# or
minicom -D /dev/YOUR_PORT -b 115200

# Check permissions
ls -la /dev/YOUR_PORT
```

## Success Criteria

The test passes when:
1. ✅ Both devices flash successfully
2. ✅ Both devices initialize Kyber crypto
3. ✅ Both devices generate Kyber keypairs
4. ✅ Key exchange attempts are made
5. ✅ Quantum security is activated
6. ✅ Mesh packets are transmitted
7. ✅ Error count stays low (< 5 per device)

## Test Report

After completion, check `kyber_t_deck_test_report.txt` for detailed results including:
- Flash success/failure for each device
- Kyber initialization status  
- Key exchange statistics
- Mesh communication metrics
- Overall test verdict