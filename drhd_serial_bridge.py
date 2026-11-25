#!/usr/bin/env python3
"""
Dr.HD 500 Combo Bluetooth-to-TCP Bridge for iOS

Connects to the Dr.HD 500 Satellite Finder via macOS Bluetooth Classic Serial (/dev/cu.*)
and exposes a high-speed TCP socket on port 8080 for the DrHD500 iOS App.

Usage:
  1. Pair your Dr.HD 500 in macOS System Settings > Bluetooth (PIN: 0000).
  2. Run this script:
       python3 drhd_serial_bridge.py
  3. In the iOS App, go to Settings -> Select "Wi-Fi / TCP" -> Connect!
"""

import sys
import glob
import time
import socket
import select
import argparse
import subprocess

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("Error: pyserial is required. Install with: pip3 install pyserial")
    sys.exit(1)


def find_drhd_ports():
    ports = serial.tools.list_ports.comports()
    candidates = []
    for p in ports:
        desc = f"{p.device} - {p.description}"
        dev_lower = p.device.lower() + " " + (p.description or "").lower()
        if any(k in dev_lower for k in ["drhd", "dr.hd", "500", "01-ac-78", "01:ac:78", "bluetooth", "spp"]):
            candidates.append(p.device)
    
    # Also check /dev/cu.*
    cu_ports = glob.glob("/dev/cu.*")
    for cp in cu_ports:
        cp_lower = cp.lower()
        if any(k in cp_lower for k in ["drhd", "dr.hd", "500", "bluetooth"]):
            if cp not in candidates:
                candidates.append(cp)
                
    return candidates, [p.device for p in ports]


def run_bridge(serial_port, baud_rate=115200, tcp_port=8080):
    print(f"\n=======================================================")
    print(f"🛰  Dr.HD 500 Satellite Finder -> iOS TCP Bridge")
    print(f"=======================================================")
    print(f"📡 Serial Port : {serial_port} (Baud: {baud_rate})")
    print(f"🌐 TCP Server  : 0.0.0.0:{tcp_port}")
    
    # Open Serial connection
    try:
        ser = serial.Serial(serial_port, baudrate=baud_rate, timeout=0.1)
        print(f"✅ Connected to Dr.HD Serial Port: {serial_port}")
    except Exception as e:
        print(f"❌ Failed to open serial port '{serial_port}': {e}")
        print("\nTip: Make sure the Dr.HD 500 is powered on and paired in macOS Bluetooth Settings (PIN: 0000).")
        return

    # Start TCP Server
    server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_sock.bind(("0.0.0.0", tcp_port))
    server_sock.listen(1)
    
    # Get local IP for convenience
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        local_ip = s.getsockname()[0]
        s.close()
    except Exception:
        local_ip = "127.0.0.1"

    print(f"📱 Ready for iOS App connection!")
    print(f"   • On iPhone: Enter Bridge IP: {local_ip}  Port: {tcp_port}")
    print(f"   • On Mac Simulator: Enter Bridge IP: 127.0.0.1  Port: {tcp_port}\n")

    def ensure_bluetooth():
        try:
            res = subprocess.run(["blueutil", "--is-connected", "01-ac-78-00-04-55"], capture_output=True, text=True, timeout=2)
            if res.stdout.strip() != "1":
                print("🔄 Reconnecting Bluetooth device 01-ac-78-00-04-55 via blueutil...")
                subprocess.run(["blueutil", "--connect", "01-ac-78-00-04-55"], timeout=3)
        except Exception:
            pass

    while True:
        try:
            ensure_bluetooth()
            print("⏳ Waiting for DrHD500 iOS app to connect...")
            client_sock, client_addr = server_sock.accept()
            print(f"🚀 iOS App connected from {client_addr}!")
            
            client_sock.setblocking(False)
            ser.timeout = 0.01

            rx_count = 0
            tx_count = 0
            last_stats = time.time()

            while True:
                # 1. Read from iOS App -> Forward to Dr.HD Serial
                r, _, _ = select.select([client_sock], [], [], 0.005)
                if r:
                    data = client_sock.recv(4096)
                    if not data:
                        print("ℹ️ iOS App disconnected.")
                        break
                    # Send to DrHD (using 2-byte chunking as required by firmware)
                    try:
                        for i in range(0, len(data), 2):
                            chunk = data[i:i+2]
                            ser.write(chunk)
                        tx_count += len(data)
                    except Exception as e:
                        print(f"⚠️ Serial write error: {e}")
                        break

                # 2. Read from Dr.HD Serial -> Forward to iOS App
                try:
                    ser_bytes = ser.read(ser.in_waiting or 1)
                    if ser_bytes:
                        client_sock.sendall(ser_bytes)
                        rx_count += len(ser_bytes)
                except Exception as e:
                    print(f"⚠️ Serial read error: {e}")
                    break

                # Periodic throughput readout
                now = time.time()
                if now - last_stats > 5.0 and (rx_count > 0 or tx_count > 0):
                    print(f"📊 Relayed: TX {tx_count} B (to DrHD) | RX {rx_count} B (from DrHD)")
                    last_stats = now

        except (socket.error, ConnectionResetError) as e:
            print(f"⚠️ Connection interrupted: {e}")
            time.sleep(1)
        except KeyboardInterrupt:
            print("\nShutting down bridge.")
            break
        finally:
            if 'client_sock' in locals():
                client_sock.close()
            # Try reopening serial if it failed
            try:
                if not ser.is_open:
                    ser.open()
            except Exception:
                pass

    ser.close()
    server_sock.close()


def main():
    parser = argparse.ArgumentParser(description="Dr.HD 500 Bluetooth Serial to TCP Bridge for iOS")
    parser.add_argument("-p", "--port", help="Serial port of Dr.HD 500 (e.g. /dev/cu.DrHD500-SerialPort)")
    parser.add_argument("-b", "--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("-t", "--tcp", type=int, default=8080, help="TCP port to listen on (default: 8080)")
    args = parser.parse_args()

    port = args.port
    if not port:
        candidates, all_ports = find_drhd_ports()
        if candidates:
            port = candidates[0]
            print(f"🔍 Auto-detected candidate port: {port}")
        elif all_ports:
            print("Available serial ports on this Mac:")
            for i, p in enumerate(all_ports):
                print(f"  [{i+1}] {p}")
            print("\nIf your Dr.HD 500 is paired, select its port with: python3 drhd_serial_bridge.py -p <port>")
            return
        else:
            print("❌ No Bluetooth serial ports found.")
            print("Please pair the Dr.HD 500 in macOS System Settings > Bluetooth (PIN: 0000).")
            return

    run_bridge(port, args.baud, args.tcp)


if __name__ == "__main__":
    main()
