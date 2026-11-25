//
//  BLETransport.swift
//  DrHD500
//
//  Advanced CoreBluetooth implementation with universal BLE-UART service discovery,
//  live peripheral scanning, diagnostic event logging, and manual/auto connection.
//

import Foundation
import CoreBluetooth
import Combine

public struct DiscoveredBLEDevice: Identifiable, Hashable {
    public let id: UUID
    public let peripheral: CBPeripheral
    public var name: String
    public var rssi: Int
    public var advertisedServices: [String]
    public var manufacturerDataHex: String?
    public var lastSeen: Date
    
    public var displayName: String {
        if !name.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty && name != "Unknown" {
            return name
        }
        return "Unnamed Device (\(id.uuidString.prefix(8)))"
    }
    
    public var isLikelyDrHD: Bool {
        let n = name.lowercased()
        return n.contains("dr.hd") ||
               n.contains("drhd") ||
               n.contains("500") ||
               n.contains("sat") ||
               n.contains("finder") ||
               n.contains("spp") ||
               n.contains("bt05") ||
               n.contains("hm-10") ||
               n.contains("jdy") ||
               n.contains("fsc") ||
               n.contains("uart")
    }
    
    public func hash(into hasher: inout Hasher) {
        hasher.combine(id)
    }
    
    public static func == (lhs: DiscoveredBLEDevice, rhs: DiscoveredBLEDevice) -> Bool {
        lhs.id == rhs.id
    }
}

public final class BLETransport: NSObject, ObservableObject, SatFinderTransport, CBCentralManagerDelegate, CBPeripheralDelegate {
    // Observable UI state
    @Published public var discoveredDevices: [DiscoveredBLEDevice] = []
    @Published public var diagnosticLogs: [String] = []
    @Published public var bluetoothStateString: String = "Initializing"
    @Published public var isScanning: Bool = false
    @Published public var autoConnectEnabled: Bool = true
    @Published public var connectedDeviceName: String? = nil
    
    // CoreBluetooth internals
    private var centralManager: CBCentralManager!
    private var connectedPeripheral: CBPeripheral?
    private var writeCharacteristic: CBCharacteristic?
    private var rxCharacteristic: CBCharacteristic?
    private var shouldScanWhenReady: Bool = false
    
    // SatFinderTransport protocol requirements
    private let stateSubject = CurrentValueSubject<TransportConnectionState, Never>(.disconnected)
    public var connectionStatePublisher: AnyPublisher<TransportConnectionState, Never> {
        stateSubject.eraseToAnyPublisher()
    }
    public var currentState: TransportConnectionState { stateSubject.value }
    public var onBytesReceived: (([UInt8]) -> Void)?
    
    // Well-known BLE UART Services to search for bonded/connected devices
    private let knownUARTServiceUUIDs: [CBUUID] = [
        CBUUID(string: "FFE0"),                                                 // HM-10 / CC2541
        CBUUID(string: "FFF0"),                                                 // JDY-08, JDY-30, JDY-31, BT-05
        CBUUID(string: "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"),                 // Nordic NUS
        CBUUID(string: "49535343-FE7D-4AE5-8FA9-9FAFD205E455"),                 // Microchip / ISSC
        CBUUID(string: "FFE5"),                                                 // Extended UART
        CBUUID(string: "FF00"),                                                 // Realtek / Actions
        CBUUID(string: "ABF0"),                                                 // Telink UART
        CBUUID(string: "00001101-0000-1000-8000-00805F9B34FB")                  // Serial Port Profile (SPP)
    ]
    
    public override init() {
        super.init()
        self.log("Initializing CoreBluetooth central manager...")
        self.centralManager = CBCentralManager(delegate: self, queue: .main)
    }
    
    // MARK: - Logging Helper
    public func log(_ message: String) {
        let formatter = DateFormatter()
        formatter.dateFormat = "HH:mm:ss.SSS"
        let timestamp = formatter.string(from: Date())
        let line = "[\(timestamp)] \(message)"
        
        print("[DrHD-BLE] \(line)")
        
        Task { @MainActor [weak self] in
            guard let self = self else { return }
            self.diagnosticLogs.append(line)
            if self.diagnosticLogs.count > 250 {
                self.diagnosticLogs.removeFirst(self.diagnosticLogs.count - 250)
            }
        }
    }
    
    public func clearLogs() {
        diagnosticLogs.removeAll()
    }
    
    // MARK: - Scanning & Connecting
    public func connect() {
        startScanning()
    }
    
    public func startScanning() {
        guard centralManager != nil else { return }
        
        guard centralManager.state == .poweredOn else {
            shouldScanWhenReady = true
            isScanning = true
            stateSubject.send(.scanning)
            log("Bluetooth state is \(bluetoothStateString). Scan will begin once powered on.")
            return
        }
        
        shouldScanWhenReady = false
        isScanning = true
        stateSubject.send(.scanning)
        log("Scanning for nearby BLE devices (all services)...")
        
        // 1. Check if device is already connected via iOS Bluetooth Settings
        retrieveAlreadyConnectedPeripherals()
        
        // 2. Scan broadly with services = nil to discover all peripherals
        centralManager.scanForPeripherals(
            withServices: nil,
            options: [CBCentralManagerScanOptionAllowDuplicatesKey: false]
        )
    }
    
    public func stopScanning() {
        shouldScanWhenReady = false
        isScanning = false
        if centralManager.isScanning {
            centralManager.stopScan()
            log("Stopped scanning.")
        }
    }
    
    public func rescan() {
        stopScanning()
        discoveredDevices.removeAll()
        startScanning()
    }
    
    public func connectToDevice(_ device: DiscoveredBLEDevice) {
        connectToPeripheral(device.peripheral)
    }
    
    public func connectToPeripheral(_ peripheral: CBPeripheral) {
        stopScanning()
        
        if let current = connectedPeripheral, current.identifier != peripheral.identifier {
            centralManager.cancelPeripheralConnection(current)
        }
        
        self.connectedPeripheral = peripheral
        let name = peripheral.name ?? "Device (\(peripheral.identifier.uuidString.prefix(8)))"
        log("Connecting to: \(name)...")
        stateSubject.send(.connecting(name))
        
        centralManager.connect(peripheral, options: [
            CBConnectPeripheralOptionNotifyOnDisconnectionKey: true
        ])
    }
    
    public func disconnect() {
        stopScanning()
        if let p = connectedPeripheral {
            log("Disconnecting from: \(p.name ?? "device")...")
            centralManager.cancelPeripheralConnection(p)
        }
        connectedPeripheral = nil
        writeCharacteristic = nil
        rxCharacteristic = nil
        connectedDeviceName = nil
        stateSubject.send(.disconnected)
    }
    
    private func retrieveAlreadyConnectedPeripherals() {
        let connectedList = centralManager.retrieveConnectedPeripherals(withServices: knownUARTServiceUUIDs)
        for p in connectedList {
            let name = p.name ?? "Connected Accessory"
            log("Found already-paired peripheral: \(name)")
            let dev = DiscoveredBLEDevice(
                id: p.identifier,
                peripheral: p,
                name: name,
                rssi: 0,
                advertisedServices: knownUARTServiceUUIDs.map { $0.uuidString },
                manufacturerDataHex: nil,
                lastSeen: Date()
            )
            addOrUpdateDevice(dev)
            
            if autoConnectEnabled && dev.isLikelyDrHD && connectedPeripheral == nil {
                connectToPeripheral(p)
            }
        }
    }
    
    private func addOrUpdateDevice(_ device: DiscoveredBLEDevice) {
        if let idx = discoveredDevices.firstIndex(where: { $0.id == device.id }) {
            discoveredDevices[idx].name = device.name.isEmpty ? discoveredDevices[idx].name : device.name
            discoveredDevices[idx].rssi = device.rssi
            discoveredDevices[idx].lastSeen = device.lastSeen
            if !device.advertisedServices.isEmpty {
                discoveredDevices[idx].advertisedServices = device.advertisedServices
            }
        } else {
            discoveredDevices.append(device)
            discoveredDevices.sort {
                if $0.isLikelyDrHD != rhsIsDrHD($1) {
                    return $0.isLikelyDrHD
                }
                return $0.rssi > $1.rssi
            }
        }
    }
    
    private func rhsIsDrHD(_ dev: DiscoveredBLEDevice) -> Bool {
        dev.isLikelyDrHD
    }
    
    // MARK: - Transmit Data
    public func sendBytes(_ bytes: [UInt8]) {
        guard let p = connectedPeripheral, let char = writeCharacteristic else {
            log("Cannot send \(bytes.count) bytes: TX characteristic not ready.")
            return
        }
        
        let data = Data(bytes)
        let writeType: CBCharacteristicWriteType = char.properties.contains(.writeWithoutResponse) ? .withoutResponse : .withResponse
        let maxChunk = max(20, p.maximumWriteValueLength(for: writeType))
        
        var offset = 0
        while offset < data.count {
            let chunk = data.subdata(in: offset..<min(offset + maxChunk, data.count))
            p.writeValue(chunk, for: char, type: writeType)
            offset += maxChunk
        }
        
        let hex = bytes.prefix(8).map { String(format: "%02X", $0) }.joined(separator: " ")
        log("TX (\(bytes.count) B): [\(hex)\(bytes.count > 8 ? "..." : "")]")
    }
    
    // MARK: - CBCentralManagerDelegate
    public func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            bluetoothStateString = "Powered On"
            log("Bluetooth powered on and ready.")
            if shouldScanWhenReady || isScanning {
                startScanning()
            }
        case .poweredOff:
            bluetoothStateString = "Powered Off"
            log("Bluetooth is powered off on this device.")
            stateSubject.send(.failed("Bluetooth is turned off"))
        case .unauthorized:
            bluetoothStateString = "Unauthorized"
            log("Bluetooth permission denied. Grant access in iOS Settings > Privacy > Bluetooth.")
            stateSubject.send(.failed("Bluetooth permission denied"))
        case .unsupported:
            bluetoothStateString = "Unsupported"
            log("Bluetooth Low Energy is unsupported on this platform (Simulator does not emulate BLE).")
            stateSubject.send(.failed("BLE unsupported (Simulator). Use real device or Wi-Fi."))
        case .resetting:
            bluetoothStateString = "Resetting"
            log("Bluetooth service is resetting...")
        case .unknown:
            bluetoothStateString = "Unknown"
            log("Bluetooth state unknown...")
        @unknown default:
            bluetoothStateString = "State: \(central.state.rawValue)"
        }
    }
    
    public func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        let advertisedName = advertisementData[CBAdvertisementDataLocalNameKey] as? String
        let name = (peripheral.name ?? advertisedName ?? "").trimmingCharacters(in: .whitespacesAndNewlines)
        let serviceUUIDs = (advertisementData[CBAdvertisementDataServiceUUIDsKey] as? [CBUUID])?.map { $0.uuidString } ?? []
        
        var mfgHex: String? = nil
        if let mfgData = advertisementData[CBAdvertisementDataManufacturerDataKey] as? Data {
            mfgHex = mfgData.map { String(format: "%02X", $0) }.joined()
        }
        
        let device = DiscoveredBLEDevice(
            id: peripheral.identifier,
            peripheral: peripheral,
            name: name,
            rssi: RSSI.intValue,
            advertisedServices: serviceUUIDs,
            manufacturerDataHex: mfgHex,
            lastSeen: Date()
        )
        
        addOrUpdateDevice(device)
        
        let desc = name.isEmpty ? "Unnamed (\(peripheral.identifier.uuidString.prefix(6)))" : name
        log("Discovered: '\(desc)' | RSSI: \(RSSI) dBm | Services: \(serviceUUIDs.joined(separator: ", "))")
        
        // Auto-connect if enabled and name matches DrHD signature
        if autoConnectEnabled && device.isLikelyDrHD && connectedPeripheral == nil {
            log("Auto-connecting to detected candidate: '\(desc)'...")
            connectToPeripheral(peripheral)
        }
    }
    
    public func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        let name = peripheral.name ?? "Device"
        log("Connected to \(name). Discovering all services...")
        peripheral.delegate = self
        // Discover ALL services on the device
        peripheral.discoverServices(nil)
    }
    
    public func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        let errDesc = error?.localizedDescription ?? "Connection failed"
        log("Failed to connect: \(errDesc)")
        stateSubject.send(.failed(errDesc))
    }
    
    public func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        let desc = error != nil ? "with error: \(error!.localizedDescription)" : "cleanly"
        log("Peripheral disconnected \(desc).")
        connectedPeripheral = nil
        writeCharacteristic = nil
        rxCharacteristic = nil
        connectedDeviceName = nil
        stateSubject.send(.disconnected)
    }
    
    // MARK: - CBPeripheralDelegate
    public func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        if let error = error {
            log("Service discovery error: \(error.localizedDescription)")
            return
        }
        
        guard let services = peripheral.services, !services.isEmpty else {
            log("Warning: No services found on \(peripheral.name ?? "device").")
            return
        }
        
        log("Discovered \(services.count) service(s):")
        for service in services {
            log("  └─ Service: \(service.uuid.uuidString)")
            peripheral.discoverCharacteristics(nil, for: service)
        }
    }
    
    public func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        if let error = error {
            log("Characteristic discovery error for \(service.uuid): \(error.localizedDescription)")
            return
        }
        
        guard let chars = service.characteristics, !chars.isEmpty else {
            log("  └─ Service \(service.uuid) has no characteristics.")
            return
        }
        
        for char in chars {
            let props = describeProperties(char.properties)
            log("     └─ Char: \(char.uuid.uuidString) [\(props)]")
            
            // Check for RX / Notify / Indicate
            if char.properties.contains(.notify) || char.properties.contains(.indicate) {
                if rxCharacteristic == nil || isStandardUARTChar(char.uuid) {
                    rxCharacteristic = char
                    log("        ★ Set as RX. Subscribing to notifications...")
                    peripheral.setNotifyValue(true, for: char)
                }
            }
            
            // Check for TX / Write
            if char.properties.contains(.write) || char.properties.contains(.writeWithoutResponse) {
                if writeCharacteristic == nil || isStandardUARTChar(char.uuid) {
                    writeCharacteristic = char
                    log("        ★ Set as TX characteristic.")
                }
            }
        }
        
        // If we found at least one functional characteristic, mark connected
        if writeCharacteristic != nil || rxCharacteristic != nil {
            let devName = peripheral.name ?? "Dr.HD Sat Finder"
            connectedDeviceName = devName
            stateSubject.send(.connected(devName))
            log("Transceiver link active on '\(devName)'. Ready to send/receive telemetry!")
        }
    }
    
    public func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: Error?) {
        if let error = error {
            log("Notification state error on \(characteristic.uuid): \(error.localizedDescription)")
        } else {
            log("Notification state on \(characteristic.uuid) is now: \(characteristic.isNotifying ? "ACTIVE" : "INACTIVE")")
        }
    }
    
    public func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        if let error = error {
            log("RX Error on \(characteristic.uuid): \(error.localizedDescription)")
            return
        }
        
        guard let data = characteristic.value, !data.isEmpty else { return }
        let bytes = [UInt8](data)
        
        let hex = bytes.prefix(8).map { String(format: "%02X", $0) }.joined(separator: " ")
        log("RX (\(bytes.count) B): [\(hex)\(bytes.count > 8 ? "..." : "")]")
        
        onBytesReceived?(bytes)
    }
    
    // MARK: - Utilities
    private func isStandardUARTChar(_ uuid: CBUUID) -> Bool {
        let s = uuid.uuidString.uppercased()
        return s == "FFE1" || s == "FFF1" || s == "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" || s == "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
    }
    
    private func describeProperties(_ p: CBCharacteristicProperties) -> String {
        var list: [String] = []
        if p.contains(.broadcast) { list.append("Broadcast") }
        if p.contains(.read) { list.append("Read") }
        if p.contains(.writeWithoutResponse) { list.append("WriteNoResp") }
        if p.contains(.write) { list.append("Write") }
        if p.contains(.notify) { list.append("Notify") }
        if p.contains(.indicate) { list.append("Indicate") }
        if p.contains(.authenticatedSignedWrites) { list.append("SignedWrite") }
        return list.isEmpty ? "None" : list.joined(separator: ", ")
    }
}
