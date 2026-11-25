//
//  ConstellationModel.swift
//  DrHD500
//
//  Reversed from com.drhd.finder500.base.Constellation
//  Multi-standard I/Q constellation processor supporting QPSK, 8PSK, 16-QAM, 64-QAM, 256-QAM
//  with authentic MER/EVM calculations and phosphor decay.
//

import Foundation
import CoreGraphics

public enum ConstellationModulation: String, CaseIterable, Identifiable {
    case qpsk = "QPSK (DVB-S/T)"
    case psk8 = "8-PSK (DVB-S2)"
    case qam16 = "16-QAM (DVB-T/C)"
    case qam64 = "64-QAM (DVB-T/T2/C)"
    case qam256 = "256-QAM (DVB-T2/C)"
    
    public var id: String { rawValue }
    
    public var shortName: String {
        switch self {
        case .qpsk: return "QPSK"
        case .psk8: return "8-PSK"
        case .qam16: return "16-QAM"
        case .qam64: return "64-QAM"
        case .qam256: return "256-QAM"
        }
    }
}

public enum ConstellationZoom: String, CaseIterable, Identifiable {
    case full = "Full (1x)"
    case zoom2x = "Zoom (2x)"
    case q1 = "Q1 (+,+)"
    case q2 = "Q2 (-,+)"
    case q3 = "Q3 (-,-)"
    case q4 = "Q4 (+,-)"
    
    public var id: String { rawValue }
}

public struct ConstellationPoint: Identifiable, Equatable {
    public let id: Int
    public let i: Float // Normalized In-phase (-1.0 ... 1.0)
    public let q: Float // Normalized Quadrature (-1.0 ... 1.0)
    public let age: Double // 0.0 (newest) to 1.0 (oldest)
}

public struct ConstellationModel: Equatable {
    public var modulation: ConstellationModulation = .qpsk
    public var zoom: ConstellationZoom = .full
    public var points: [ConstellationPoint] = []
    
    // Telemetry Quality Metrics
    public var calculatedMER: Float = 0.0
    public var calculatedEVM: Float = 0.0
    public var qualityRating: String = "NO LOCK"
    public var lastUpdated: Date = Date()
    
    private struct RawPoint: Equatable {
        let id: Int
        let i: Float
        let q: Float
    }
    
    private let maxPoints: Int = 400
    private var pointIdCounter: Int = 0
    private var rawHistory: [RawPoint] = []
    
    public init() {}
    
    // Ideal Symbol Grid Generator for Selected Modulation
    public var idealSymbols: [CGPoint] {
        switch modulation {
        case .qpsk:
            let d: CGFloat = 0.707
            return [
                CGPoint(x: d, y: d), CGPoint(x: -d, y: d),
                CGPoint(x: -d, y: -d), CGPoint(x: d, y: -d)
            ]
            
        case .psk8:
            var pts: [CGPoint] = []
            for k in 0..<8 {
                let angle = (Double(k) * Double.pi / 4.0) + (Double.pi / 8.0)
                pts.append(CGPoint(x: cos(angle) * 0.85, y: sin(angle) * 0.85))
            }
            return pts
            
        case .qam16:
            var pts: [CGPoint] = []
            let vals: [CGFloat] = [-0.948, -0.316, 0.316, 0.948]
            for x in vals {
                for y in vals {
                    pts.append(CGPoint(x: x, y: y))
                }
            }
            return pts
            
        case .qam64:
            var pts: [CGPoint] = []
            let norm: CGFloat = sqrt(42.0)
            for xi in stride(from: -7, through: 7, by: 2) {
                for yi in stride(from: -7, through: 7, by: 2) {
                    pts.append(CGPoint(x: CGFloat(xi) / norm, y: CGFloat(yi) / norm))
                }
            }
            return pts
            
        case .qam256:
            var pts: [CGPoint] = []
            let norm: CGFloat = sqrt(170.0)
            for xi in stride(from: -15, through: 15, by: 2) {
                for yi in stride(from: -15, through: 15, by: 2) {
                    pts.append(CGPoint(x: CGFloat(xi) / norm, y: CGFloat(yi) / norm))
                }
            }
            return pts
        }
    }
    
    public mutating func ingestPacket(_ packet: [UInt8]) -> Bool {
        guard packet.count >= 104, packet[0] == DrHDConstants.incomingHeaderByte else {
            return false
        }
        
        let type = packet[1]
        // Constellation points are in types 3, 7, 8, 10
        guard type == 3 || type == 7 || type == 8 || type == 10 else {
            return false
        }
        
        var newRaw: [RawPoint] = []
        let ideal = idealSymbols
        var totalErrorSq: Double = 0.0
        var validCount = 0
        
        for idx in 0..<40 {
            let offset = (idx * 2) + 22
            let rawI = Int(packet[offset + 1]) & 0xFF
            let rawQ = Int(packet[offset]) & 0xFF
            
            // Filter noise centered at (128, 128)
            if rawI == 128 && rawQ == 128 { continue }
            if (rawI * rawI) + (rawQ * rawQ) < 200 { continue }
            
            // Normalize to [-1.0, 1.0]
            let normI = Float(rawI - 128) / 127.0
            let normQ = Float(rawQ - 128) / 127.0
            
            pointIdCounter += 1
            newRaw.append(RawPoint(id: pointIdCounter, i: normI, q: normQ))
            
            // Find closest ideal symbol for MER
            var minSqDist = Double.infinity
            let pt = CGPoint(x: CGFloat(normI), y: CGFloat(normQ))
            for sym in ideal {
                let dx = Double(pt.x - sym.x)
                let dy = Double(pt.y - sym.y)
                let sq = (dx * dx) + (dy * dy)
                if sq < minSqDist {
                    minSqDist = sq
                }
            }
            totalErrorSq += minSqDist
            validCount += 1
        }
        
        // Calculate true MER & EVM %
        if validCount > 0 {
            let meanError = totalErrorSq / Double(validCount)
            if meanError > 0 {
                let mer = max(0.0, min(42.0, -10.0 * log10(meanError)))
                calculatedMER = Float(mer)
                // EVM (%) = 100 * 10^(-MER/20)
                calculatedEVM = Float(100.0 * pow(10.0, -mer / 20.0))
                
                if mer >= 28.0 {
                    qualityRating = "EXCELLENT"
                } else if mer >= 22.0 {
                    qualityRating = "GOOD"
                } else if mer >= 16.0 {
                    qualityRating = "MARGINAL"
                } else {
                    qualityRating = "POOR"
                }
            }
        }
        
        rawHistory.append(contentsOf: newRaw)
        if rawHistory.count > maxPoints {
            rawHistory.removeFirst(rawHistory.count - maxPoints)
        }
        
        // Rebuild points with phosphor decay ages (0.0 to 1.0)
        let totalCount = rawHistory.count
        points = rawHistory.enumerated().map { index, item in
            let ageFraction = 1.0 - (Double(index) / Double(max(1, totalCount - 1)))
            return ConstellationPoint(id: item.id, i: item.i, q: item.q, age: ageFraction)
        }
        
        lastUpdated = Date()
        return true
    }
    
    public mutating func clear() {
        rawHistory.removeAll()
        points.removeAll()
        calculatedMER = 0.0
        calculatedEVM = 0.0
        qualityRating = "CLEARED"
    }
}
