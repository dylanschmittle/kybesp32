#!/usr/bin/env python3
"""
Helper script to flash firmware to T-Deck devices
"""

import subprocess
import sys

try:
    import serial.tools.list_ports
except ImportError:
    print("❌ pyserial not installed. Run: pip install pyserial")
    sys.exit(1)

def main():
    # Find T-Deck devices
    ports = serial.tools.list_ports.comports()
    t_deck_ports = []
    
    for port in ports:
        if any(kw in str(port.description).lower() for kw in ['cp210', 'esp32', 'serial', 'usb']):
            t_deck_ports.append(port.device)
            
    if len(t_deck_ports) < 2:
        print(f"❌ ERROR: Need 2 T-Decks, found {len(t_deck_ports)}")
        print("Make sure both devices are connected and in flashing mode")
        sys.exit(1)
        
    print(f"Found T-Deck ports: {t_deck_ports[:2]}")
    
    # Flash each device
    for i, port in enumerate(t_deck_ports[:2]):
        print(f"Flashing T-Deck {i+1} on {port}...")
        
        result = subprocess.run([
            'platformio', 'run', 
            '-e', 't-deck-tft', 
            '--target', 'upload', 
            '--upload-port', port
        ], cwd='meshtastic')
        
        if result.returncode == 0:
            print(f"✅ T-Deck {i+1} flashed successfully")
        else:
            print(f"❌ T-Deck {i+1} flash failed")
            
if __name__ == "__main__":
    main()