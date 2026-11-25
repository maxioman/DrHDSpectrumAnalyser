//
//  MockTransport.swift
//  DrHD500
//
//  Hardware simulator generating realistic Dr.HD 500 telemetry and RF packets
//

import Foundation
import Combine

public final class MockTransport: SatFinderTransport {
    private let stateSubject = CurrentValueSubject<TransportConnectionState, Never>(.disconnected)
    public var connectionStatePublisher: AnyPublisher<TransportConnectionState, Never> {
        stateSubject.eraseToAnyPublisher()
    }
    public var currentState: TransportConnectionState { stateSubject.value }
    public var onBytesReceived: (([UInt8]) -> Void)?
    
    private var timer: Timer?
    private var tick: Int = 0
    private var targetFreq: Int = 11727
    private var isS2: Bool = false
    
    public init() {}
    
    public func connect() {
        stateSubject.send(.connecting("Dr.HD 500 (Simulator)"))
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.6) { [weak self] in
            guard let self = self else { return }
            self.stateSubject.send(.connected("Dr.HD 500 (Simulator)"))
            self.startSimulation()
        }
    }
    
    public func disconnect() {
        timer?.invalidate()
        timer = nil
        stateSubject.send(.disconnected)
    }
    
    public func sendBytes(_ bytes: [UInt8]) {
        // Inspect commands sent by app
        guard bytes.count >= 2, bytes[0] == DrHDConstants.packetHeaderByte else { return }
        let mode = Int(bytes[1] >> 4)
        if mode == DrHDConstants.packetModeSatSNR && bytes.count >= 7 {
            targetFreq = ((Int(bytes[3]) & 0xFF) << 8) | (Int(bytes[4]) & 0xFF)
            isS2 = (bytes[2] & 0x10) != 0
        }
    }
    
    private func startSimulation() {
        timer?.invalidate()
        timer = Timer.scheduledTimer(withTimeInterval: 0.13, repeats: true) { [weak self] _ in
            self?.generateSimulatedPacket()
        }
    }
    
    private func generateSimulatedPacket() {
        tick += 1
        var packet = [UInt8](repeating: 0, count: 104)
        packet[0] = DrHDConstants.incomingHeaderByte // 0xFE
        
        let cycle = tick % 10
        if cycle == 0 {
            // Packet 13: Hardware Telemetry
            packet[1] = 13
            let battMV = 7650 + Int(sin(Double(tick) * 0.05) * 50)
            packet[2] = UInt8((battMV >> 8) & 0xFF)
            packet[3] = UInt8(battMV & 0xFF)
            
            let lnb10xV = 138 // 13.8V
            packet[4] = UInt8((lnb10xV >> 8) & 0xFF)
            packet[5] = UInt8(lnb10xV & 0xFF)
            
            let psu10xV = 120 // 12.0V
            packet[6] = UInt8((psu10xV >> 8) & 0xFF)
            packet[7] = UInt8(psu10xV & 0xFF)
            
            let lnbA = 145 + Int(sin(Double(tick) * 0.1) * 5) // 145 mA
            packet[8] = UInt8((lnbA >> 8) & 0xFF)
            packet[9] = UInt8(lnbA & 0xFF)
            
            packet[10] = 0x04 // LNB power enabled
            packet[11] = 134  // Hardware version 134
            packet[12] = 0    // Production FW
            packet[13] = 49   // Firmware version 49
            
            let mockHid: [UInt8] = [0x01, 0xAC, 0x78, 0xB5, 0x50, 0x00, 0xAA, 0x11]
            for i in 0..<8 { packet[14 + i] = mockHid[i] }
            
        } else if cycle == 1 || cycle == 2 || cycle == 3 {
            // Packet 0, 1, 2: Spectrum sweep chunk (100 points each)
            let chunkIdx = cycle - 1
            packet[1] = UInt8(chunkIdx) // 0, 1, or 2
            
            for i in 0..<100 {
                let globalIdx = chunkIdx * 100 + i
                // Generate simulated satellite spectrum with transponder peaks
                let baseNoise = 35.0 + sin(Double(globalIdx) * 0.1) * 5.0
                let peak1 = 140.0 * exp(-pow(Double(globalIdx - 65) / 10.0, 2))
                let peak2 = 160.0 * exp(-pow(Double(globalIdx - 150) / 14.0, 2))
                let peak3 = 130.0 * exp(-pow(Double(globalIdx - 220) / 12.0, 2))
                let level = min(250.0, max(20.0, baseNoise + peak1 + peak2 + peak3 + Double.random(in: -3...3)))
                packet[i + 2] = UInt8(level)
            }
            
        } else if cycle == 4 || cycle == 7 {
            // Packet 7: Constellation data
            packet[1] = 7
            packet[2] = 148 // SNR 14.8 dB
            packet[13] = 0x01 // Carrier locked!
            
            // Generate 40 QPSK constellation points with noise
            for i in 0..<40 {
                let offset = (i * 2) + 22
                let quadrant = i % 4
                let idealX = (quadrant == 0 || quadrant == 1) ? 75.0 : -75.0
                let idealY = (quadrant == 0 || quadrant == 2) ? 75.0 : -75.0
                let noisyX = idealX + Double.random(in: -9...9)
                let noisyY = idealY + Double.random(in: -9...9)
                packet[offset + 1] = UInt8(min(255, max(0, Int(128.0 + noisyX))))
                packet[offset] = UInt8(min(255, max(0, Int(128.0 + noisyY))))
            }
            
        } else {
            // Packet 6: Live Sat Signal Measurements + DVB SI
            packet[1] = 6
            packet[2] = 120 // raw SNR
            packet[3] = 15  // i2 high bits
            packet[4] = UInt8(Int.random(in: 0...5))   // i2 low bits (yields ~14.8 dB)
            packet[5] = 0x0C // Power raw byte 1
            packet[6] = 0x12 // Power raw byte 2 (~82 dBuV)
            packet[13] = 0x01 // Demod Carrier Lock: TRUE
            
            // Inject NIT and SDT DVB SI tables into Si payload
            // 0x40 (NIT): "SES ASTRA"
            var siOffset = 14
            packet[siOffset] = 0x40
            let nitName = "SES ASTRA".data(using: .isoLatin1)!
            packet[siOffset + 1] = UInt8(nitName.count)
            for (idx, b) in nitName.enumerated() {
                packet[siOffset + 2 + idx] = b
            }
            siOffset += 2 + nitName.count
            
            // 0x43 (SDS): 19.2°E
            packet[siOffset] = 0x43
            packet[siOffset + 1] = 11
            packet[siOffset + 10] = 0x01 // 0192
            packet[siOffset + 11] = 0x92
            packet[siOffset + 12] = 0x80 // East
            siOffset += 13
            
            // 0x48 (SDT): Channel Name "BBC News HD"
            packet[siOffset] = 0x48
            let chName = "BBC News HD".data(using: .isoLatin1)!
            packet[siOffset + 1] = UInt8(chName.count + 5)
            packet[siOffset + 5] = 0 // Provider length 0
            packet[siOffset + 6] = UInt8(chName.count)
            for (idx, b) in chName.enumerated() {
                packet[siOffset + 7 + idx] = b
            }
        }
        
        // Compute 16-bit checksum
        var sum: Int = 0
        for i in 0..<102 {
            sum += Int(packet[i]) & 0xFF
        }
        packet[102] = UInt8((sum >> 8) & 0xFF)
        packet[103] = UInt8(sum & 0xFF)
        
        onBytesReceived?(packet)
    }
}
