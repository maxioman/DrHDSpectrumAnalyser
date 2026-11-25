//
//  ConnectionSettingsSheet.swift
//  DrHD500
//
//  Hardware connection, transport selection, live BLE scanner, and diagnostics console
//

import SwiftUI
#if os(iOS)
import UIKit
#endif

public struct ConnectionSettingsSheet: View {
    @ObservedObject var session: DeviceSession
    @Environment(\.dismiss) private var dismiss
    @State private var copiedAlert = false
    
    public var body: some View {
        NavigationView {
            Form {
                // MARK: - Transport Interface Picker
                Section(header: Text("Transport Interface")) {
                    Picker("Connection Type", selection: $session.currentTransportType) {
                        ForEach(TransportType.allCases) { type in
                            Text(type.rawValue).tag(type)
                        }
                    }
                    .pickerStyle(.segmented)
                    .onChange(of: session.currentTransportType) { newType in
                        session.switchTransport(to: newType)
                    }
                    
                    if session.currentTransportType == .wifi {
                        HStack {
                            Text("Bridge IP:")
                            Spacer()
                            TextField("Host IP", text: $session.tcpHost)
                                .multilineTextAlignment(.trailing)
                                #if os(iOS)
                                .autocapitalization(.none)
                                .disableAutocorrection(true)
                                #endif
                        }
                        HStack {
                            Text("Port:")
                            Spacer()
                            #if os(iOS)
                            TextField("Port", value: $session.tcpPort, format: .number)
                                .keyboardType(.numberPad)
                                .multilineTextAlignment(.trailing)
                            #else
                            TextField("Port", value: $session.tcpPort, format: .number)
                                .multilineTextAlignment(.trailing)
                            #endif
                        }
                        
                        // IP Quick Presets
                        ScrollView(.horizontal, showsIndicators: false) {
                            HStack(spacing: 8) {
                                Button("Mac Bridge (192.168.1.26)") {
                                    session.tcpHost = "192.168.1.26"
                                    session.tcpPort = 8080
                                }
                                .font(.caption2)
                                .buttonStyle(.bordered)
                                
                                Button("ESP32 AP (192.168.4.1)") {
                                    session.tcpHost = "192.168.4.1"
                                    session.tcpPort = 8080
                                }
                                .font(.caption2)
                                .buttonStyle(.bordered)
                                
                                Button("Simulator (127.0.0.1)") {
                                    session.tcpHost = "127.0.0.1"
                                    session.tcpPort = 8080
                                }
                                .font(.caption2)
                                .buttonStyle(.bordered)
                            }
                            .padding(.vertical, 2)
                        }
                    }
                    
                    if case .failed(let err) = session.connectionState {
                        Text("⚠️ Connection Error: \(err)")
                            .font(.caption)
                            .foregroundColor(.red)
                    }
                    
                    if case .connecting(let target) = session.connectionState {
                        HStack(spacing: 8) {
                            ProgressView()
                                .scaleEffect(0.8)
                            Text("Connecting to \(target)...")
                                .font(.caption)
                                .foregroundColor(.secondary)
                        }
                    }
                    
                    // Main Connect/Disconnect Button (for Simulator and Wi-Fi)
                    if session.currentTransportType != .ble {
                        HStack {
                            Spacer()
                            switch session.connectionState {
                            case .disconnected, .failed:
                                Button("Connect to Device") {
                                    session.connect()
                                }
                                .buttonStyle(.borderedProminent)
                            case .connecting:
                                Button("Cancel") {
                                    session.disconnect()
                                }
                                .buttonStyle(.bordered)
                            case .connected:
                                Button("Disconnect") {
                                    session.disconnect()
                                }
                                .buttonStyle(.borderedProminent)
                                .tint(.red)
                            case .scanning:
                                EmptyView()
                            }
                            Spacer()
                        }
                        .padding(.vertical, 4)
                    }
                }
                
                // MARK: - BLE Scanner & Device Discovery Section
                if session.currentTransportType == .ble {
                    BLEScannerSectionView(ble: session.bleTransport, session: session)
                    
                    BLEDiagnosticsConsoleView(ble: session.bleTransport)
                    
                    Section(header: Text("Bluetooth Compatibility Guide")) {
                        VStack(alignment: .leading, spacing: 6) {
                            Text("About Dr.HD 500 Bluetooth:")
                                .font(.footnote).bold()
                            Text("• The Dr.HD 500 sat finder hardware was designed with Bluetooth Classic SPP (RFCOMM).")
                                .font(.caption).foregroundColor(.secondary)
                            Text("• iOS allows Bluetooth Low Energy (BLE) natively, but restricts Classic SPP unless Apple MFi certified.")
                                .font(.caption).foregroundColor(.secondary)
                            Text("• If using an external BLE serial bridge (HM-10, JDY-30/31, ESP32 BLE), it will appear in the scanner above and connect seamlessly.")
                                .font(.caption).foregroundColor(.secondary)
                            Text("• You can also bridge Dr.HD serial to Wi-Fi and use the 'Wi-Fi / TCP' transport.")
                                .font(.caption).foregroundColor(.secondary)
                        }
                        .padding(.vertical, 4)
                    }
                }
                
                // MARK: - Hardware Diagnostics
                Section(header: Text("Hardware Diagnostics")) {
                    HStack {
                        Text("Hardware Version")
                        Spacer()
                        Text("v\(session.hardwareTelemetry.hardwareVersion)")
                            .foregroundColor(.secondary)
                    }
                    HStack {
                        Text("Firmware Version")
                        Spacer()
                        Text("v\(session.hardwareTelemetry.softwareVersion)")
                            .foregroundColor(.secondary)
                    }
                    HStack {
                        Text("Device Hardware ID")
                        Spacer()
                        Text(session.hardwareTelemetry.hardwareId.isEmpty ? "None" : session.hardwareTelemetry.hardwareId)
                            .font(.system(.body, design: .monospaced))
                            .foregroundColor(.secondary)
                    }
                    HStack {
                        Text("Battery Voltage")
                        Spacer()
                        Text(session.hardwareTelemetry.batteryVoltsString)
                            .foregroundColor(.secondary)
                    }
                    HStack {
                        Text("LNB Supply Voltage")
                        Spacer()
                        Text(String(format: "%.1f V", session.hardwareTelemetry.lnbVolts))
                            .foregroundColor(.secondary)
                    }
                    HStack {
                        Text("PSU Input Voltage")
                        Spacer()
                        Text(String(format: "%.1f V", session.hardwareTelemetry.psuVolts))
                            .foregroundColor(.secondary)
                    }
                }
                
                Section(header: Text("About")) {
                    Text("Dr.HD 500 Sat Finder Companion for iOS\nReverse-engineered from Android APK\nMulti-Transport Architecture (BLE, TCP & Simulator).")
                        .font(.footnote)
                        .foregroundColor(.secondary)
                }
            }
            .navigationTitle("Device Connection")
            #if os(iOS)
            .navigationBarTitleDisplayMode(.inline)
            #endif
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Done") { dismiss() }
                }
            }
        }
    }
}

// MARK: - Dedicated BLE Scanner Section
struct BLEScannerSectionView: View {
    @ObservedObject var ble: BLETransport
    @ObservedObject var session: DeviceSession
    
    var body: some View {
        Section(header: Text("Bluetooth Low Energy Scanner")) {
            // Scanner Status & Controls
            HStack {
                VStack(alignment: .leading, spacing: 2) {
                    Text("Status: \(ble.bluetoothStateString)")
                        .font(.subheadline)
                        .foregroundColor(ble.bluetoothStateString == "Powered On" ? .green : .primary)
                    if ble.isScanning {
                        HStack(spacing: 4) {
                            ProgressView()
                                .controlSize(.small)
                            Text("Scanning for peripherals...")
                                .font(.caption2)
                                .foregroundColor(.secondary)
                        }
                    }
                }
                
                Spacer()
                
                if ble.isScanning {
                    Button("Stop") {
                        ble.stopScanning()
                    }
                    .buttonStyle(.bordered)
                    .controlSize(.small)
                } else {
                    Button("Scan") {
                        ble.rescan()
                    }
                    .buttonStyle(.borderedProminent)
                    .controlSize(.small)
                }
            }
            
            Toggle("Auto-Connect to Dr.HD Device", isOn: $ble.autoConnectEnabled)
                .font(.subheadline)
            
            // Discovered Peripherals
            if ble.discoveredDevices.isEmpty {
                VStack(spacing: 8) {
                    if ble.isScanning {
                        ProgressView()
                            .padding(.top, 4)
                        Text("Scanning for nearby Bluetooth devices...")
                            .font(.caption)
                            .foregroundColor(.secondary)
                    } else {
                        Text("No Bluetooth devices found yet.\nEnsure your Dr.HD device or BLE adapter is powered on and tap 'Scan'.")
                            .font(.caption)
                            .foregroundColor(.secondary)
                            .multilineTextAlignment(.center)
                            .padding(.vertical, 6)
                    }
                }
                .frame(maxWidth: .infinity)
                .padding(.vertical, 8)
            } else {
                ForEach(ble.discoveredDevices) { device in
                    HStack(spacing: 12) {
                        Image(systemName: device.isLikelyDrHD ? "antenna.radiowaves.left.and.right" : "wave.3.forward")
                            .foregroundColor(device.isLikelyDrHD ? .green : .blue)
                            .font(.title3)
                        
                        VStack(alignment: .leading, spacing: 3) {
                            HStack {
                                Text(device.displayName)
                                    .font(.subheadline)
                                    .fontWeight(.semibold)
                                
                                if device.isLikelyDrHD {
                                    Text("Dr.HD Match")
                                        .font(.system(size: 9, weight: .bold))
                                        .padding(.horizontal, 5)
                                        .padding(.vertical, 2)
                                        .background(Color.green.opacity(0.18))
                                        .foregroundColor(.green)
                                        .cornerRadius(4)
                                }
                            }
                            
                            HStack(spacing: 8) {
                                Text("\(device.rssi) dBm")
                                    .font(.caption2)
                                    .fontWeight(.medium)
                                    .foregroundColor(device.rssi > -70 ? .green : (device.rssi > -85 ? .orange : .red))
                                
                                Text(device.id.uuidString.prefix(8))
                                    .font(.system(size: 10, design: .monospaced))
                                    .foregroundColor(.secondary)
                                
                                if !device.advertisedServices.isEmpty {
                                    Text(device.advertisedServices.first ?? "")
                                        .font(.system(size: 9, design: .monospaced))
                                        .foregroundColor(.secondary)
                                }
                            }
                        }
                        
                        Spacer()
                        
                        if ble.connectedDeviceName == device.name && session.connectionState != .disconnected {
                            Button("Connected") {
                                ble.disconnect()
                            }
                            .buttonStyle(.bordered)
                            .tint(.green)
                            .controlSize(.small)
                        } else {
                            Button("Connect") {
                                session.connectBLEDevice(device)
                            }
                            .buttonStyle(.borderedProminent)
                            .controlSize(.small)
                        }
                    }
                    .padding(.vertical, 2)
                }
            }
        }
    }
}

// MARK: - Dedicated BLE Diagnostic Console
struct BLEDiagnosticsConsoleView: View {
    @ObservedObject var ble: BLETransport
    @State private var showCopiedAlert = false
    
    var body: some View {
        Section(header: Text("Live Bluetooth Diagnostics")) {
            VStack(alignment: .leading, spacing: 6) {
                ScrollViewReader { proxy in
                    ScrollView {
                        LazyVStack(alignment: .leading, spacing: 3) {
                            if ble.diagnosticLogs.isEmpty {
                                Text("No logs recorded yet. Start scanning to view live events.")
                                    .font(.system(size: 11, design: .monospaced))
                                    .foregroundColor(.gray)
                                    .padding(8)
                            } else {
                                ForEach(Array(ble.diagnosticLogs.enumerated()), id: \.offset) { index, log in
                                    Text(log)
                                        .font(.system(size: 10, weight: .regular, design: .monospaced))
                                        .foregroundColor(logColor(log))
                                        .id(index)
                                }
                                .padding(6)
                            }
                        }
                    }
                    .frame(height: 150)
                    .background(Color.black.opacity(0.88))
                    .cornerRadius(8)
                    .onChange(of: ble.diagnosticLogs.count) { _ in
                        if !ble.diagnosticLogs.isEmpty {
                            proxy.scrollTo(ble.diagnosticLogs.count - 1)
                        }
                    }
                }
                
                HStack {
                    Button(action: { ble.clearLogs() }) {
                        Label("Clear", systemImage: "trash")
                            .font(.caption2)
                    }
                    .buttonStyle(.bordered)
                    .controlSize(.small)
                    
                    Spacer()
                    
                    Button(action: {
                        #if os(iOS)
                        UIPasteboard.general.string = ble.diagnosticLogs.joined(separator: "\n")
                        showCopiedAlert = true
                        #endif
                    }) {
                        Label(showCopiedAlert ? "Copied!" : "Copy Logs", systemImage: showCopiedAlert ? "checkmark" : "doc.on.doc")
                            .font(.caption2)
                    }
                    .buttonStyle(.bordered)
                    .controlSize(.small)
                }
            }
            .padding(.vertical, 4)
        }
    }
    
    private func logColor(_ log: String) -> Color {
        if log.contains("TX") {
            return .cyan
        } else if log.contains("RX") {
            return .green
        } else if log.contains("Error") || log.contains("failed") || log.contains("Failed") {
            return .red
        } else if log.contains("★") || log.contains("Link active") {
            return .yellow
        } else if log.contains("Discovered") {
            return Color(red: 0.8, green: 0.9, blue: 1.0)
        }
        return .white
    }
}
