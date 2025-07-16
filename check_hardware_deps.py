#!/usr/bin/env python3
"""
Check dependencies for T-Deck hardware testing
"""

import sys
import subprocess
import importlib

def check_python_package(package_name, install_cmd=None):
    """Check if a Python package is available"""
    try:
        importlib.import_module(package_name)
        print(f"✅ {package_name} is installed")
        return True
    except ImportError:
        print(f"❌ {package_name} is NOT installed")
        if install_cmd:
            print(f"   Install with: {install_cmd}")
        return False

def check_command(command, description):
    """Check if a command is available"""
    try:
        result = subprocess.run([command, '--version'], 
                              capture_output=True, text=True, timeout=10)
        if result.returncode == 0:
            version = result.stdout.strip().split('\n')[0]
            print(f"✅ {description}: {version}")
            return True
        else:
            print(f"❌ {description} not working properly")
            return False
    except (FileNotFoundError, subprocess.TimeoutExpired):
        print(f"❌ {description} not found")
        return False

def check_usb_devices():
    """Check for potential T-Deck devices"""
    try:
        import serial.tools.list_ports
        ports = serial.tools.list_ports.comports()
        
        potential_devices = []
        for port in ports:
            if any(keyword in str(port.description).lower() for keyword in 
                   ['cp210', 'esp32', 'serial', 'usb', 'uart']):
                potential_devices.append(port)
                
        print(f"\nUSB Serial Devices Found:")
        if potential_devices:
            for port in potential_devices:
                print(f"  {port.device} - {port.description}")
            print(f"\n✅ Found {len(potential_devices)} potential T-Deck device(s)")
            
            if len(potential_devices) >= 2:
                print("Ready for dual T-Deck testing!")
            else:
                print("⚠️  Need 2 T-Deck devices for full testing")
        else:
            print("  No USB serial devices found")
            print("❌ No T-Deck devices detected")
            
        return len(potential_devices) >= 2
        
    except ImportError:
        print("❌ Cannot check USB devices (pyserial not installed)")
        return False

def main():
    print("T-DECK HARDWARE TEST DEPENDENCY CHECK")
    print("=" * 40)
    
    checks_passed = 0
    total_checks = 4
    
    # Check Python packages
    print("\nPython Dependencies:")
    if check_python_package('serial', 'pip install pyserial'):
        checks_passed += 1
        
    # Check system commands
    print("\nSystem Commands:")
    if check_command('platformio', 'PlatformIO'):
        checks_passed += 1
        
    if check_command('python3', 'Python 3'):
        checks_passed += 1
        
    # Check USB devices
    print("\nHardware Detection:")
    if check_usb_devices():
        checks_passed += 1
        
    # Summary
    print(f"\n{'='*40}")
    print(f"DEPENDENCY CHECK SUMMARY")
    print(f"{'='*40}")
    print(f"Checks passed: {checks_passed}/{total_checks}")
    
    if checks_passed == total_checks:
        print("🎉 ALL DEPENDENCIES SATISFIED!")
        print("Ready to run: make test_hardware")
        return 0
    else:
        print("❌ Some dependencies missing")
        print("\nNext steps:")
        if checks_passed < 3:
            print("1. Install missing software dependencies")
        if checks_passed >= 3:
            print("1. Connect T-Deck devices via USB")
            print("2. Put devices in flashing mode (hold BOOT)")
        print("3. Run: make test_hardware")
        return 1

if __name__ == "__main__":
    sys.exit(main())