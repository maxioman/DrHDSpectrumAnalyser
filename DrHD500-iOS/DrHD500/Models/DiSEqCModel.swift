//
//  DiSEqCModel.swift
//  DrHD500
//
//  DiSEqC 1.0 / 1.1 Switch, 1.2 Motor & USALS positioning commands
//

import Foundation

public enum DiSEqCProtocol: String, CaseIterable, Identifiable {
    case off = "DiSEqC OFF"
    case diseqc10 = "DiSEqC 1.0"
    case diseqc11 = "DiSEqC 1.1"
    case diseqc12 = "DiSEqC 1.2"
    case usals = "USALS"
    
    public var id: String { rawValue }
}

public struct DiSEqCPort: Identifiable, Hashable {
    public var id: String { "\(name)-\(command)" }
    public let name: String
    public let command: String
}

public struct DiSEqCCommands {
    public static let diseqc10Ports: [DiSEqCPort] = [
        DiSEqCPort(name: "Port 1 (LNB A)", command: "[E0 10 38 F0]"),
        DiSEqCPort(name: "Port 2 (LNB B)", command: "[E0 10 38 F4]"),
        DiSEqCPort(name: "Port 3 (LNB C)", command: "[E0 10 38 F8]"),
        DiSEqCPort(name: "Port 4 (LNB D)", command: "[E0 10 38 FC]")
    ]
    
    public static let diseqc11Ports: [DiSEqCPort] = (1...16).map { idx in
        let hex = String(format: "%02X", 0xF0 + (idx - 1))
        return DiSEqCPort(name: "Port \(idx)", command: "[E0 10 39 \(hex)]")
    }
    
    // Motor commands (DiSEqC 1.2)
    public static let motorStop = "[E0 31 60]"
    public static let motorDriveEast = "[E0 31 68 00]"
    public static let motorDriveWest = "[E0 31 69 00]"
    public static let motorStepEast = "[E0 31 68 01]"
    public static let motorStepWest = "[E0 31 69 01]"
    
    public static func motorGotoPosition(number: Int) -> String {
        return String(format: "[E0 31 6B %02X]", number)
    }
    
    public static func motorStorePosition(number: Int) -> String {
        return String(format: "[E0 31 6A %02X]", number)
    }
    
    // USALS (DiSEqC 1.3 / Goto X)
    public static func usalsGotoX(localLat: Double, localLong: Double, satPosDegrees: Double) -> String {
        // Calculate USALS angular position
        let dLong = localLong - satPosDegrees
        let radLat = localLat * .pi / 180.0
        let radDLong = dLong * .pi / 180.0
        
        let az = atan(tan(radDLong) / sin(radLat)) * 180.0 / .pi
        let isEast = az < 0
        let absAz = abs(az)
        
        let degreePart = Int(absAz)
        let fracPart = Int((absAz - Double(degreePart)) * 16.0)
        let rawAngle = ((degreePart & 0xFF) << 4) | (fracPart & 0x0F)
        
        let b2 = isEast ? 0xE0 | ((rawAngle >> 8) & 0x0F) : 0xD0 | ((rawAngle >> 8) & 0x0F)
        let b3 = rawAngle & 0xFF
        
        return String(format: "[E0 31 6E %02X %02X]", b2, b3)
    }
}
