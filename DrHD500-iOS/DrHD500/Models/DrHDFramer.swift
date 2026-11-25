//
//  DrHDFramer.swift
//  DrHD500
//
//  Assembles and validates 104-byte packets from incoming byte stream
//

import Foundation

public final class DrHDFramer {
    private var buffer = [UInt8]()
    public var onPacketReceived: (([UInt8]) -> Void)?
    public var onChecksumError: (() -> Void)?
    
    public init() {}
    
    public func appendBytes(_ bytes: [UInt8]) {
        buffer.append(contentsOf: bytes)
        processBuffer()
    }
    
    private func processBuffer() {
        while buffer.count >= DrHDConstants.incomingPacketSize {
            // Find start byte 0xFE
            guard let startIndex = buffer.firstIndex(of: DrHDConstants.incomingHeaderByte) else {
                buffer.removeAll()
                return
            }
            
            if startIndex > 0 {
                buffer.removeFirst(startIndex)
            }
            
            if buffer.count < DrHDConstants.incomingPacketSize {
                return
            }
            
            let candidate = Array(buffer[0..<DrHDConstants.incomingPacketSize])
            if validateChecksum(candidate) {
                buffer.removeFirst(DrHDConstants.incomingPacketSize)
                onPacketReceived?(candidate)
            } else {
                // False sync or corrupted packet, skip header and re-search
                onChecksumError?()
                buffer.removeFirst()
            }
        }
    }
    
    private func validateChecksum(_ packet: [UInt8]) -> Bool {
        var sum: Int = 0
        for i in 0..<102 {
            sum += Int(packet[i]) & 0xFF
        }
        
        let receivedChecksum = ((Int(packet[102]) & 0xFF) << 8) | (Int(packet[103]) & 0xFF)
        return (sum & 0xFFFF) == (receivedChecksum & 0xFFFF)
    }
    
    public func reset() {
        buffer.removeAll()
    }
}
