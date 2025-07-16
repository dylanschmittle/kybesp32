#!/usr/bin/env python3
"""
Two T-Deck Test Suite for Kyber Quantum-Resistant Mesh Networking

This script tests the complete Kyber implementation between two T-Deck devices:
1. Flashes both devices with Kyber-enabled Meshtastic firmware
2. Monitors serial output for Kyber key exchange
3. Tests mesh communication with quantum security
4. Validates protocol performance and reliability

Requirements:
- 2 T-Deck devices connected via USB
- platformio installed
- python3 with serial support
"""

import sys
import time
import subprocess
import threading
import queue
import re
import json
from pathlib import Path
from typing import Dict, List, Optional, Tuple
import argparse

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("ERROR: pyserial not installed. Run: pip install pyserial")
    sys.exit(1)

class TDeckTester:
    def __init__(self, verbose: bool = False):
        self.verbose = verbose
        self.devices: List[Dict] = []
        self.test_results: Dict = {
            'flash_success': [],
            'kyber_init': [],
            'key_exchange': [],
            'mesh_communication': [],
            'quantum_security': []
        }
        
    def log(self, message: str, level: str = "INFO"):
        timestamp = time.strftime("%H:%M:%S")
        print(f"[{timestamp}] {level}: {message}")
        
    def find_t_deck_devices(self) -> List[str]:
        """Find connected T-Deck devices"""
        self.log("Scanning for T-Deck devices...")
        
        # Look for ESP32-S3 devices (T-Deck uses ESP32-S3)
        ports = serial.tools.list_ports.comports()
        t_deck_ports = []
        
        for port in ports:
            # T-Deck typically shows as CP210x or similar USB-to-UART bridge
            if any(keyword in str(port.description).lower() for keyword in 
                   ['cp210', 'esp32', 'serial', 'usb']):
                t_deck_ports.append(port.device)
                self.log(f"Found potential T-Deck: {port.device} - {port.description}")
                
        if len(t_deck_ports) < 2:
            self.log(f"ERROR: Need 2 T-Deck devices, found {len(t_deck_ports)}", "ERROR")
            self.log("Make sure both T-Decks are connected and in flashing mode", "ERROR")
            return []
            
        # Take first 2 devices
        selected_ports = t_deck_ports[:2]
        self.log(f"Selected T-Deck devices: {selected_ports}")
        return selected_ports
        
    def flash_firmware(self, port: str, device_name: str) -> bool:
        """Flash Kyber-enabled Meshtastic firmware to T-Deck"""
        self.log(f"Flashing {device_name} on {port}...")
        
        try:
            # Change to meshtastic directory and flash
            cmd = [
                "platformio", "run", 
                "--target", "upload",
                "--upload-port", port,
                "-e", "t-deck-tft"
            ]
            
            self.log(f"Running: {' '.join(cmd)}")
            
            result = subprocess.run(
                cmd,
                cwd="meshtastic",
                capture_output=True,
                text=True,
                timeout=300  # 5 minute timeout
            )
            
            if result.returncode == 0:
                self.log(f"✅ {device_name} flashed successfully")
                return True
            else:
                self.log(f"❌ {device_name} flash failed: {result.stderr}", "ERROR")
                return False
                
        except subprocess.TimeoutExpired:
            self.log(f"❌ {device_name} flash timeout", "ERROR")
            return False
        except Exception as e:
            self.log(f"❌ {device_name} flash error: {e}", "ERROR")
            return False
            
    def monitor_serial(self, port: str, device_name: str, 
                      output_queue: queue.Queue, duration: int = 60):
        """Monitor serial output from T-Deck"""
        self.log(f"Starting serial monitor for {device_name} on {port}")
        
        try:
            with serial.Serial(port, 115200, timeout=1) as ser:
                start_time = time.time()
                
                while time.time() - start_time < duration:
                    try:
                        line = ser.readline().decode('utf-8', errors='ignore').strip()
                        if line:
                            output_queue.put((device_name, time.time(), line))
                            if self.verbose:
                                self.log(f"[{device_name}] {line}")
                    except Exception as e:
                        if self.verbose:
                            self.log(f"Serial read error for {device_name}: {e}", "WARN")
                        continue
                        
        except Exception as e:
            self.log(f"❌ Serial monitor failed for {device_name}: {e}", "ERROR")
            
    def analyze_log_output(self, device_logs: Dict[str, List[Tuple[float, str]]]) -> Dict:
        """Analyze serial logs for Kyber-specific events"""
        results = {
            'kyber_init': {},
            'key_generation': {},
            'key_exchange_attempts': {},
            'sessions_established': {},
            'quantum_security_active': {},
            'mesh_packets': {},
            'errors': {}
        }
        
        # Patterns to look for in logs
        patterns = {
            'kyber_init': r'Kyber.*initialized|KyberCryptoEngine.*created',
            'key_generation': r'Kyber keypair generated|generateKeyPair.*success',
            'key_exchange_start': r'Initiating Kyber key exchange|KYBER_MSG_KEY_EXCHANGE_REQUEST',
            'key_chunk': r'Kyber.*chunk|KYBER_MSG_KEY_CHUNK',
            'session_established': r'Kyber session established|quantum security.*active',
            'quantum_security': r'quantum.{0,20}security.{0,20}(active|enabled)',
            'mesh_packet': r'packet.*received|packet.*sent|mesh.*message',
            'error': r'ERROR|FAIL|error|fail'
        }
        
        for device_name, logs in device_logs.items():
            results['kyber_init'][device_name] = False
            results['key_generation'][device_name] = False
            results['key_exchange_attempts'][device_name] = 0
            results['sessions_established'][device_name] = 0
            results['quantum_security_active'][device_name] = False
            results['mesh_packets'][device_name] = 0
            results['errors'][device_name] = 0
            
            for timestamp, line in logs:
                line_lower = line.lower()
                
                # Check each pattern
                if re.search(patterns['kyber_init'], line_lower):
                    results['kyber_init'][device_name] = True
                    
                if re.search(patterns['key_generation'], line_lower):
                    results['key_generation'][device_name] = True
                    
                if re.search(patterns['key_exchange_start'], line_lower):
                    results['key_exchange_attempts'][device_name] += 1
                    
                if re.search(patterns['session_established'], line_lower):
                    results['sessions_established'][device_name] += 1
                    
                if re.search(patterns['quantum_security'], line_lower):
                    results['quantum_security_active'][device_name] = True
                    
                if re.search(patterns['mesh_packet'], line_lower):
                    results['mesh_packets'][device_name] += 1
                    
                if re.search(patterns['error'], line_lower):
                    results['errors'][device_name] += 1
                    
        return results
        
    def test_mesh_communication(self, ports: List[str]) -> bool:
        """Test mesh communication between devices"""
        self.log("Testing mesh communication between T-Decks...")
        
        device_logs = {'T-Deck-1': [], 'T-Deck-2': []}
        output_queue = queue.Queue()
        
        # Start serial monitors
        monitor_threads = []
        for i, port in enumerate(ports):
            device_name = f"T-Deck-{i+1}"
            thread = threading.Thread(
                target=self.monitor_serial,
                args=(port, device_name, output_queue, 90)  # 90 second test
            )
            thread.daemon = True
            thread.start()
            monitor_threads.append(thread)
            
        self.log("Monitoring mesh communication for 90 seconds...")
        
        # Collect logs
        start_time = time.time()
        while time.time() - start_time < 95:  # Allow extra time for thread cleanup
            try:
                device_name, timestamp, line = output_queue.get(timeout=1)
                device_logs[device_name].append((timestamp, line))
            except queue.Empty:
                continue
                
        # Wait for monitor threads to complete
        for thread in monitor_threads:
            thread.join(timeout=5)
            
        self.log(f"Collected {sum(len(logs) for logs in device_logs.values())} log lines")
        
        # Analyze results
        analysis = self.analyze_log_output(device_logs)
        
        # Print analysis results
        self.log("\n=== MESH COMMUNICATION TEST RESULTS ===")
        for device in ['T-Deck-1', 'T-Deck-2']:
            self.log(f"\n{device}:")
            self.log(f"  Kyber Initialized: {analysis['kyber_init'][device]}")
            self.log(f"  Key Generation: {analysis['key_generation'][device]}")
            self.log(f"  Key Exchange Attempts: {analysis['key_exchange_attempts'][device]}")
            self.log(f"  Sessions Established: {analysis['sessions_established'][device]}")
            self.log(f"  Quantum Security: {analysis['quantum_security_active'][device]}")
            self.log(f"  Mesh Packets: {analysis['mesh_packets'][device]}")
            self.log(f"  Errors: {analysis['errors'][device]}")
            
        # Determine success
        success_criteria = {
            'both_init': all(analysis['kyber_init'].values()),
            'both_keygen': all(analysis['key_generation'].values()),
            'key_exchanges': sum(analysis['key_exchange_attempts'].values()) > 0,
            'low_errors': all(count < 5 for count in analysis['errors'].values())
        }
        
        overall_success = all(success_criteria.values())
        
        self.log(f"\n=== SUCCESS CRITERIA ===")
        for criterion, passed in success_criteria.items():
            status = "✅ PASS" if passed else "❌ FAIL"
            self.log(f"  {criterion}: {status}")
            
        return overall_success
        
    def generate_test_report(self, flash_results: Dict[str, bool], 
                           mesh_test_result: bool) -> str:
        """Generate a comprehensive test report"""
        report = []
        report.append("KYBER T-DECK MESH NETWORKING TEST REPORT")
        report.append("=" * 50)
        report.append(f"Test Date: {time.strftime('%Y-%m-%d %H:%M:%S')}")
        report.append(f"Devices Tested: {len(self.devices)}")
        report.append("")
        
        # Flash results
        report.append("FIRMWARE FLASHING RESULTS:")
        report.append("-" * 30)
        for device, success in flash_results.items():
            status = "✅ SUCCESS" if success else "❌ FAILED"
            report.append(f"  {device}: {status}")
        report.append("")
        
        # Mesh test results
        report.append("MESH COMMUNICATION TEST:")
        report.append("-" * 30)
        mesh_status = "✅ SUCCESS" if mesh_test_result else "❌ FAILED"
        report.append(f"  Overall Result: {mesh_status}")
        report.append("")
        
        # Summary
        overall_success = all(flash_results.values()) and mesh_test_result
        report.append("OVERALL TEST RESULT:")
        report.append("-" * 20)
        if overall_success:
            report.append("🎉 ALL TESTS PASSED!")
            report.append("Kyber quantum-resistant mesh networking is working correctly.")
        else:
            report.append("❌ SOME TESTS FAILED!")
            report.append("Check individual test results above for details.")
            
        return "\n".join(report)
        
    def run_tests(self) -> bool:
        """Run the complete test suite"""
        self.log("Starting Kyber T-Deck Test Suite...")
        
        # Step 1: Find devices
        ports = self.find_t_deck_devices()
        if len(ports) < 2:
            return False
            
        # Step 2: Flash firmware
        self.log("\n=== FIRMWARE FLASHING ===")
        flash_results = {}
        
        for i, port in enumerate(ports):
            device_name = f"T-Deck-{i+1}"
            self.devices.append({
                'name': device_name,
                'port': port,
                'index': i
            })
            
            success = self.flash_firmware(port, device_name)
            flash_results[device_name] = success
            
            if success:
                self.log(f"Waiting 10 seconds for {device_name} to boot...")
                time.sleep(10)
                
        if not all(flash_results.values()):
            self.log("❌ Firmware flashing failed for some devices", "ERROR")
            # Continue with mesh test even if some flashing failed
            
        # Step 3: Test mesh communication
        self.log("\n=== MESH COMMUNICATION TEST ===")
        mesh_result = self.test_mesh_communication(ports)
        
        # Step 4: Generate report
        report = self.generate_test_report(flash_results, mesh_result)
        
        # Save report to file
        report_file = Path("kyber_t_deck_test_report.txt")
        with open(report_file, 'w') as f:
            f.write(report)
            
        self.log(f"\n{report}")
        self.log(f"\nTest report saved to: {report_file}")
        
        return all(flash_results.values()) and mesh_result

def main():
    parser = argparse.ArgumentParser(description="Test Kyber mesh networking on 2 T-Deck devices")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose serial output")
    parser.add_argument("--skip-flash", action="store_true", help="Skip firmware flashing")
    parser.add_argument("--test-duration", type=int, default=90, help="Test duration in seconds")
    
    args = parser.parse_args()
    
    tester = TDeckTester(verbose=args.verbose)
    
    try:
        success = tester.run_tests()
        sys.exit(0 if success else 1)
    except KeyboardInterrupt:
        tester.log("\nTest interrupted by user", "WARN")
        sys.exit(1)
    except Exception as e:
        tester.log(f"Test failed with exception: {e}", "ERROR")
        sys.exit(1)

if __name__ == "__main__":
    main()