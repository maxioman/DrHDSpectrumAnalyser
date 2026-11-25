//
//  SatelliteData.swift
//  DrHD500
//
//  Models for Satellites, Transponders, and LNB Converters
//

import Foundation

public enum Polarization: Int, Codable, CaseIterable, Identifiable {
    case horizontal = 0
    case vertical = 1
    case circularLeft = 2
    case circularRight = 3
    
    public var id: Int { rawValue }
    
    public var label: String {
        switch self {
        case .horizontal: return "H"
        case .vertical: return "V"
        case .circularLeft: return "L"
        case .circularRight: return "R"
        }
    }
}

public enum DvbStandard: Int, Codable, CaseIterable, Identifiable {
    case dvbS = 0
    case dvbS2 = 1
    
    public var id: Int { rawValue }
    
    public var label: String {
        switch self {
        case .dvbS: return "DVB-S"
        case .dvbS2: return "DVB-S2"
        }
    }
}

public struct Transponder: Identifiable, Codable, Hashable {
    public var id: String { "\(frequency)-\(polarization.rawValue)-\(symbolRate)" }
    public let frequency: Int      // Hz (e.g. 11727000) or KHz
    public let polarization: Polarization
    public let symbolRate: Int     // Symbol rate e.g. 27500000 or 27500
    public let system: DvbStandard
    
    public var freqMHz: Float {
        return frequency > 1000000 ? Float(frequency) / 1000.0 : Float(frequency)
    }
    
    public var symbolRateKSps: Int {
        return symbolRate > 100000 ? symbolRate / 1000 : symbolRate
    }
    
    public var displayString: String {
        let f = String(format: "%.0f", freqMHz)
        return "\(f) \(polarization.label) / \(symbolRateKSps) (\(system.label))"
    }
}

public struct Satellite: Identifiable, Codable, Hashable {
    public var id: String { "\(name)-\(position)" }
    public let name: String
    public let position: String
    public let transponders: [Transponder]
    
    public var formattedPosition: String {
        guard let posInt = Int(position) else { return position }
        let deg = Float(posInt) / 10.0
        return posInt >= 0 ? String(format: "%.1f°E", deg) : String(format: "%.1f°W", abs(deg))
    }
}

public struct LNBConverter: Identifiable, Codable, Hashable {
    public var id: String { title }
    public let title: String
    public let subtitle: String
    public let lowFreq: Int   // MHz (e.g. 9750)
    public let highFreq: Int  // MHz (e.g. 10600)
    public let minIf: Int
    public let maxIf: Int
    public let mode: Int
    
    public static let standardUniversalKu = LNBConverter(
        title: "UNIVERSAL",
        subtitle: "(9750/10600MHz)",
        lowFreq: 9750,
        highFreq: 10600,
        minIf: 950,
        maxIf: 2150,
        mode: 0
    )
}

public final class SatelliteDatabase: ObservableObject {
    public static let shared = SatelliteDatabase()
    
    @Published public var satellites: [Satellite] = []
    @Published public var converters: [LNBConverter] = []
    
    public init() {
        loadData()
    }
    
    private func loadData() {
        if let satURL = Bundle.main.url(forResource: "satellites", withExtension: "json") {
            if let data = try? Data(contentsOf: satURL) {
                if let decoded = try? JSONDecoder().decode([Satellite].self, from: data) {
                    self.satellites = decoded
                }
            }
        }
        
        if let convURL = Bundle.main.url(forResource: "converters", withExtension: "json") {
            if let data = try? Data(contentsOf: convURL) {
                if let decoded = try? JSONDecoder().decode([LNBConverter].self, from: data) {
                    self.converters = decoded
                }
            }
        }
        
        // Fallback default list if bundle resources not loaded yet
        if self.satellites.isEmpty {
            self.satellites = [
                Satellite(
                    name: "Hotbird 13E",
                    position: "130",
                    transponders: [
                        Transponder(frequency: 10992000, polarization: .vertical, symbolRate: 27500000, system: .dvbS),
                        Transponder(frequency: 11034000, polarization: .vertical, symbolRate: 27500000, system: .dvbS),
                        Transponder(frequency: 11054000, polarization: .horizontal, symbolRate: 27500000, system: .dvbS2),
                        Transponder(frequency: 11096000, polarization: .horizontal, symbolRate: 29900000, system: .dvbS2)
                    ]
                ),
                Satellite(
                    name: "Astra 19.2E",
                    position: "192",
                    transponders: [
                        Transponder(frequency: 10773000, polarization: .horizontal, symbolRate: 22000000, system: .dvbS2),
                        Transponder(frequency: 11362000, polarization: .horizontal, symbolRate: 22000000, system: .dvbS2),
                        Transponder(frequency: 11494000, polarization: .horizontal, symbolRate: 22000000, system: .dvbS2),
                        Transponder(frequency: 11836000, polarization: .horizontal, symbolRate: 27500000, system: .dvbS)
                    ]
                ),
                Satellite(
                    name: "Badr / Es'hail 26E",
                    position: "260",
                    transponders: [
                        Transponder(frequency: 11785000, polarization: .vertical, symbolRate: 27500000, system: .dvbS),
                        Transponder(frequency: 11804000, polarization: .horizontal, symbolRate: 27500000, system: .dvbS2)
                    ]
                ),
                Satellite(
                    name: "Nilesat 7W",
                    position: "-70",
                    transponders: [
                        Transponder(frequency: 11747000, polarization: .vertical, symbolRate: 27500000, system: .dvbS),
                        Transponder(frequency: 11766000, polarization: .horizontal, symbolRate: 27500000, system: .dvbS)
                    ]
                )
            ]
        }
        
        if self.converters.isEmpty {
            self.converters = [
                LNBConverter.standardUniversalKu,
                LNBConverter(title: "CIRCULAR", subtitle: "(10750MHz)", lowFreq: 10750, highFreq: 10750, minIf: 950, maxIf: 2150, mode: 0),
                LNBConverter(title: "C-BAND 5150", subtitle: "(5150MHz)", lowFreq: 5150, highFreq: 5150, minIf: 950, maxIf: 2150, mode: 0)
            ]
        }
    }
}
