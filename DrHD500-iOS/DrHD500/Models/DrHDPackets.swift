//
//  DrHDPackets.swift
//  DrHD500
//
//  Reversed packet structures & command generators for Dr.HD 500 Sat Finder
//

import Foundation

public enum DrHDConstants {
    public static let packetHeaderByte: UInt8 = 0x3D // ASCII '=' (61)
    public static let incomingHeaderByte: UInt8 = 0xFE // 254
    public static let incomingPacketSize: Int = 104
    public static let timerPeriodMs: Int = 130
    
    // Packet Modes (commands sent to hardware)
    public static let packetModeSatSNR: Int = 0
    public static let packetModeTerSNR: Int = 1
    public static let packetModeCabSNR: Int = 2
    public static let packetModeService: Int = 3
    public static let packetModeBlindScan: Int = 4
    public static let packetModeDiseqcCommand: Int = 5
    public static let packetModeSpectrumRDA: Int = 6
    public static let packetModeSpectrumRDACal: Int = 7
    public static let packetModeSpectrumMXL: Int = 8
    public static let packetModeSpectrumMXLCal: Int = 9
    
    // UI Program Modes
    public static let programModeSpectrum: Int = 16
    public static let programModeConstellation: Int = 17
    public static let programModeMeasures: Int = 18
    public static let programModeBlindScan: Int = 19
    public static let programModeService: Int = 20
    public static let programModeDiseqc: Int = 21
    public static let programModeStartMenu: Int = 22
}

public enum LNBVoltageMode: Int, CaseIterable, Identifiable {
    case volt0 = 0    // 0V
    case volt5 = 1    // 5V
    case volt13 = 2   // 13V (Vertical / Right)
    case volt13_22k = 3 // 13V + 22KHz
    case volt18 = 4   // 18V (Horizontal / Left)
    case volt18_22k = 5 // 18V + 22KHz
    case volt22 = 6   // 22V
    case volt28 = 7   // 28V
    
    public var id: Int { rawValue }
    
    public var label: String {
        switch self {
        case .volt0: return "0V (Off)"
        case .volt5: return "5V"
        case .volt13: return "13V (V / R)"
        case .volt13_22k: return "13V + 22kHz"
        case .volt18: return "18V (H / L)"
        case .volt18_22k: return "18V + 22kHz"
        case .volt22: return "22V"
        case .volt28: return "28V"
        }
    }
}

public struct DrHDPacketBuilder {
    
    /// Builds 8-byte satellite measurement tuning command
    public static func makeMeasuresSatPacket(
        mode: Int = DrHDConstants.packetModeSatSNR,
        intermediateFreq: Int,
        symbolRate: Int,
        isS2: Bool,
        powerMode: Int
    ) -> [UInt8] {
        var packet = [UInt8](repeating: 0, count: 8)
        packet[0] = DrHDConstants.packetHeaderByte // 61
        packet[1] = UInt8(((mode & 0x0F) << 4) | 0x08)
        packet[2] = UInt8((powerMode << 5) | (isS2 ? 0x10 : 0x00))
        packet[3] = UInt8((intermediateFreq >> 8) & 0xFF)
        packet[4] = UInt8(intermediateFreq & 0xFF)
        packet[5] = UInt8((symbolRate >> 8) & 0xFF)
        packet[6] = UInt8(symbolRate & 0xFF)
        packet[7] = CRC8.compute(packet, offset: 0, length: 7)
        return packet
    }
    
    /// Builds 6-byte terrestrial/cable measurement tuning command (UHF/VHF DVB-T/T2, DVB-C)
    public static func makeMeasuresTerPacket(
        mode: Int = DrHDConstants.packetModeTerSNR,
        frequencyMHz: Int,
        isT2: Bool,
        bandwidthCode: Int = 0, // 0 = 8 MHz, 1 = 7 MHz, 2 = 6 MHz
        powerMode: Int = 0
    ) -> [UInt8] {
        var packet = [UInt8](repeating: 0, count: 6)
        packet[0] = DrHDConstants.packetHeaderByte // 61 (0x3D)
        packet[1] = UInt8(((mode & 0x0F) << 4) | 0x04)
        packet[2] = UInt8(powerMode << 5)
        if isT2 {
            packet[2] |= 0x10 // DVB-T2 bit
        }
        let bw = bandwidthCode >= 0 ? bandwidthCode : 0
        packet[1] += UInt8(bw & 0x03)
        packet[2] += UInt8((bw & 0x04) << 5)
        packet[3] = UInt8((frequencyMHz >> 8) & 0xFF)
        packet[4] = UInt8(frequencyMHz & 0xFF)
        packet[5] = CRC8.compute(packet, offset: 0, length: 5)
        return packet
    }
    
    /// Builds 6-byte spectrum sweep command
    public static func makeSpectrumPacket(
        mode: Int = DrHDConstants.packetModeSpectrumRDA,
        spanBeginFreq: Int,
        span: Int,
        refLevel: Int,
        powerMode: Int,
        isRdaMode: Bool = true
    ) -> [UInt8] {
        var packet = [UInt8](repeating: 0, count: 6)
        packet[0] = DrHDConstants.packetHeaderByte // 61
        packet[1] = UInt8(((mode & 0x0F) << 4) | 0x04)
        packet[2] = UInt8(powerMode << 5)
        
        let spanFrameBegin = spanBeginFreq + (span > 100 ? 0 : span / 2)
        packet[3] = UInt8((spanFrameBegin >> 8) & 0xFF)
        packet[4] = UInt8(spanFrameBegin & 0xFF)
        
        let sc = spanCode(span: span, isRdaMode: isRdaMode)
        packet[3] |= UInt8((Int(sc) << 5) & 0xE0)
        
        if mode == DrHDConstants.packetModeSpectrumRDACal || mode == DrHDConstants.packetModeSpectrumMXLCal {
            packet[1] |= UInt8((refLevel & 0x60) >> 5)
            packet[2] |= UInt8(refLevel & 0x1F)
            packet[3] |= UInt8((refLevel & 0x80) >> 4)
        } else {
            var adjustedRef = refLevel
            if span < 100 {
                adjustedRef = min(110, refLevel + 20)
            }
            packet[2] |= UInt8(((110 - adjustedRef) / 5) & 0x1F)
            packet[1] |= UInt8((Int(sc) >> 3) & 0x03)
        }
        
        packet[5] = CRC8.compute(packet, offset: 0, length: 5)
        return packet
    }
    
    /// Builds 4-byte service / telemetry request packet
    public static func makeServicePacket(mode: Int = DrHDConstants.packetModeService, serviceMode: Int = 0) -> [UInt8] {
        var packet = [UInt8](repeating: 0, count: 4)
        packet[0] = DrHDConstants.packetHeaderByte
        packet[1] = UInt8((mode & 0x0F) << 4)
        packet[2] = UInt8(serviceMode & 0xFF)
        packet[3] = CRC8.compute(packet, offset: 0, length: 3)
        return packet
    }
    
    /// Decodes a DiSEqC hex string e.g. "[E0 10 38 F0]" and frames it into DrHD command
    public static func decodeDiseqcCommand(_ commandString: String) -> [UInt8] {
        var values: [UInt8] = [DrHDConstants.packetHeaderByte, 0]
        
        let cleaned = commandString.replacingOccurrences(of: "[", with: "")
                                   .replacingOccurrences(of: "]", with: "")
        let tokens = cleaned.split(separator: " ")
        for token in tokens {
            if let byteVal = UInt8(token, radix: 16) {
                values.append(byteVal)
            }
        }
        
        let neededLength = values.count + (values.count % 2 == 0 ? 2 : 1)
        var result = [UInt8](repeating: 0, count: neededLength)
        for i in 0..<values.count {
            result[i] = values[i]
        }
        
        result[1] = 0x50 // 80: PACKET_MODE_DISEQC_COMMAND (5) << 4
        result[1] += UInt8((values.count - 2) << 1)
        result[result.count - 1] = CRC8.compute(result, offset: 0, length: result.count - 1)
        return result
    }
    
    private static func spanCode(span: Int, isRdaMode: Bool) -> UInt8 {
        let b: Int = isRdaMode ? (span < 100 ? 8 : 0) : 11
        let i: Int
        switch span {
        case 8, 700:
            i = b + 4
        case 16, 800:
            i = b + 3
        case 24, 26, 27, 28, 1200:
            i = b + 2
        case 40, 54, 1600:
            i = b + 1
        case 72, 2000:
            return UInt8(b & 0xFF)
        case 300:
            i = b + 6
        case 600:
            i = b + 5
        default:
            i = b + 7
        }
        return UInt8(i & 0xFF)
    }
}
