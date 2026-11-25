//
//  SignalMetrics.swift
//  DrHD500
//
//  Reversed from com.drhd.finder500.base.SignalInfo
//

import Foundation

public struct SignalMetrics: Equatable {
    public var isLocked: Bool = false
    public var snrDb: Float = 0.0
    public var pwrDbuV: Float = 0.0
    public var merDb: Float = 0.0
    public var cber: Float = 0.0
    public var packetErrors: Int = 0
    public var networkName: String = ""
    public var orbitalPosition: String = ""
    public var channelsFound: [String] = []
    public var lastUpdated: Date = Date()
    
    // Linear interpolation tables for Power (dBuV)
    private static let regValues: [Double] = [
        0.0, 406.0, 460.0, 540.0, 652.0, 774.0, 877.0, 1062.0, 1248.0, 1450.0,
        1726.0, 1919.0, 2070.0, 2348.0, 2458.0, 2827.0, 3181.0, 3724.0, 3998.0, 4032.0, 4064.0
    ]
    private static let pwrValues: [Double] = [
        130.0, 114.75, 112.75, 109.75, 105.75, 101.75, 96.75, 91.75, 86.75, 81.75,
        76.75, 71.75, 66.75, 61.75, 56.75, 51.75, 46.75, 41.75, 36.75, 36.0, 35.0
    ]
    
    public static func interpolatePower(rawVal: Double) -> Float {
        if rawVal <= regValues.first! { return Float(pwrValues.first!) }
        if rawVal >= regValues.last! { return Float(pwrValues.last!) }
        
        for i in 0..<(regValues.count - 1) {
            let x0 = regValues[i]
            let x1 = regValues[i + 1]
            if rawVal >= x0 && rawVal <= x1 {
                let y0 = pwrValues[i]
                let y1 = pwrValues[i + 1]
                let t = (rawVal - x0) / (x1 - x0)
                return Float(y0 + t * (y1 - y0))
            }
        }
        return Float(pwrValues.last!)
    }
    
    public static func parseSatPacket(_ packet: [UInt8], currentMetrics: SignalMetrics) -> SignalMetrics {
        guard packet.count >= 104, packet[0] == DrHDConstants.incomingHeaderByte else {
            return currentMetrics
        }
        
        var m = currentMetrics
        let type = packet[1]
        
        let lockOffsets: [UInt8] = [0, 0, 0, 21, 13, 13, 13, 21, 21, 5, 21, 13, 5, 0, 5, 5]
        if Int(type) < lockOffsets.count {
            let offset = Int(lockOffsets[Int(type)])
            if offset < packet.count {
                m.isLocked = (packet[offset] & 1) == 1
            }
        }
        
        // Exact SNR calculation reversed from com.drhd.finder500.base.DrhdDevice
        let rawSnr = Float(packet[2] & 0xFF)
        if type == 6 || type == 8 || type == 15 {
            // S2 SNR formula
            let i2 = ((Int(packet[3]) & 0xFF) << 6) + (Int(packet[4]) & 0x3F)
            if !(rawSnr == 64 && i2 == 1024) && i2 > 0 {
                let f2 = (2.0 * rawSnr * rawSnr) / Float(i2)
                m.snrDb = f2 > 1.0 ? Float(log10(Double(f2)) * 10.0) : 0.0
            } else {
                m.snrDb = 0.0
            }
        } else {
            // DVB-S SNR formula
            m.snrDb = rawSnr > 8 ? Float(log10(Double(rawSnr / 8.0)) * 10.0) : 0.0
        }
        
        // Power calculation from bytes 5..6
        let rawPwrVal = Double(((Int(packet[5]) & 0xFF) << 5) + (Int(packet[6]) & 0x1F))
        m.pwrDbuV = interpolatePower(rawVal: rawPwrVal)
        
        // CBER and PE (Uncorrected Packets)
        if packet.count >= 15 {
            let pe = ((Int(packet[11]) & 0xFF) << 8) | (Int(packet[12]) & 0xFF)
            m.packetErrors = pe
        }
        
        // Parse embedded DVB SI (NIT / SDT / SDS) from offset b + 1
        let lockOffset = Int(type) < lockOffsets.count ? Int(lockOffsets[Int(type)]) : 13
        if lockOffset + 1 < packet.count {
            let siBytes = Array(packet[(lockOffset + 1)...])
            parseSI(siBytes, metrics: &m)
        }
        
        m.lastUpdated = Date()
        return m
    }
    
    private static func parseSI(_ bytes: [UInt8], metrics: inout SignalMetrics) {
        var idx = 0
        while idx < bytes.count - 2 {
            let tag = bytes[idx]
            let len = Int(bytes[idx + 1])
            if idx + 2 + len > bytes.count { break }
            
            if tag == 0x40 { // NIT Network Name Descriptor
                let strBytes = Array(bytes[(idx + 2)..<(idx + 2 + len)])
                if let str = String(bytes: strBytes, encoding: .isoLatin1)?.trimmingCharacters(in: .whitespacesAndNewlines), !str.isEmpty {
                    metrics.networkName = str
                }
            } else if tag == 0x43 { // Satellite Delivery System (SDS)
                if len >= 11 {
                    let bcd1 = bytes[idx + 8]
                    let bcd2 = bytes[idx + 9]
                    let posVal = Float((Int(bcd1 >> 4) * 1000) + (Int(bcd1 & 0xF) * 100) + (Int(bcd2 >> 4) * 10) + Int(bcd2 & 0xF)) / 10.0
                    let isEast = (bytes[idx + 10] & 0x80) != 0
                    metrics.orbitalPosition = String(format: "%.1f°%@", posVal, isEast ? "E" : "W")
                }
            } else if tag == 0x48 { // Service Descriptor (Channel Name)
                if len >= 3 {
                    let providerLen = Int(bytes[idx + 5])
                    let serviceNameOffset = idx + 6 + providerLen
                    if serviceNameOffset < idx + 2 + len {
                        let serviceLen = Int(bytes[serviceNameOffset])
                        if serviceNameOffset + 1 + serviceLen <= idx + 2 + len {
                            let nameBytes = Array(bytes[(serviceNameOffset + 1)..<(serviceNameOffset + 1 + serviceLen)])
                            if let chName = String(bytes: nameBytes, encoding: .isoLatin1)?.trimmingCharacters(in: .whitespacesAndNewlines), !chName.isEmpty {
                                if !metrics.channelsFound.contains(chName) {
                                    metrics.channelsFound.append(chName)
                                }
                            }
                        }
                    }
                }
            }
            idx += 2 + len
        }
    }
}
