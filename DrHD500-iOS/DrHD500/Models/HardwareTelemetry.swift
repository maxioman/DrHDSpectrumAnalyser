//
//  HardwareTelemetry.swift
//  DrHD500
//
//  Reversed from com.drhd.finder500.base.HardwareInfo
//

import Foundation

public struct HardwareTelemetry: Equatable {
    public var batteryMilliVolts: Int = 0
    public var lnbVolts: Float = 0.0
    public var psuVolts: Float = 0.0
    public var lnbCurrentMilliAmps: Int = 0
    public var isCharging: Bool = false
    public var lnbPowerMode: Int = 0
    public var hardwareVersion: Int = 0
    public var softwareVersion: Int = 0
    public var isTestingSoftware: Bool = false
    public var hardwareId: String = ""
    public var errorFlags: UInt8 = 0
    public var lastUpdated: Date = Date()
    
    public var batteryPercent: Int {
        let thresholds: [Int] = [8600, 8097, 7895, 7682, 7498, 7344, 7151, 7034, 6909, 6774, 6561, 6300]
        var step = 1
        while step < 11 && thresholds[step - 1] >= batteryMilliVolts {
            step += 1
        }
        return max(0, min(100, (11 - step) * 10))
    }
    
    public var batteryVoltsString: String {
        return String(format: "%.2f V", Float(batteryMilliVolts) / 1000.0)
    }
    
    public var lnbCurrentString: String {
        return "\(lnbCurrentMilliAmps) mA"
    }
    
    public static func parse(packet: [UInt8]) -> HardwareTelemetry? {
        guard packet.count >= 24, packet[0] == DrHDConstants.incomingHeaderByte, packet[1] == 13 else {
            return nil
        }
        
        var tel = HardwareTelemetry()
        tel.batteryMilliVolts = ((Int(packet[2]) & 0xFF) << 8) | (Int(packet[3]) & 0xFF)
        tel.lnbVolts = Float(((Int(packet[4]) & 0xFF) << 8) | (Int(packet[5]) & 0xFF)) / 10.0
        tel.psuVolts = Float(((Int(packet[6]) & 0xFF) << 8) | (Int(packet[7]) & 0xFF)) / 10.0
        tel.lnbCurrentMilliAmps = ((Int(packet[8]) & 0xFF) << 8) | (Int(packet[9]) & 0xFF)
        
        let p10 = packet[10]
        tel.isCharging = ((p10 & 1) == 1) && (tel.psuVolts > 8.0)
        tel.lnbPowerMode = Int((p10 & 0x0E) >> 1)
        
        tel.hardwareVersion = Int(packet[11] & 0xFF)
        tel.isTestingSoftware = packet[12] != 0
        tel.softwareVersion = Int(packet[13] & 0xFF)
        
        let hidBytes = Array(packet[14..<22])
        tel.hardwareId = hidBytes.map { String(format: "%02X", $0) }.joined()
        tel.errorFlags = packet[22]
        tel.lastUpdated = Date()
        
        return tel
    }
}
