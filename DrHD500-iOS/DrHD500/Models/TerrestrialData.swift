//
//  TerrestrialData.swift
//  DrHD500
//
//  Terrestrial (VHF/UHF DVB-T/T2) and Cable (DVB-C) frequency database
//  Loaded from Terrestrial&Cable_1000Combo_EU.xml
//

import Foundation

public enum SignalBand: String, CaseIterable, Identifiable {
    case satellite = "Satellite (DVB-S/S2)"
    case uhf = "Terrestrial UHF (DVB-T/T2)"
    case vhf = "Terrestrial VHF (DVB-T)"
    case cable = "Cable TV (DVB-C)"
    
    public var id: String { rawValue }
    public var isTerrestrial: Bool { self != .satellite }
    
    public var shortName: String {
        switch self {
        case .satellite: return "SAT"
        case .uhf: return "UHF"
        case .vhf: return "VHF"
        case .cable: return "CATV"
        }
    }
}

public struct TerrestrialChannel: Identifiable, Hashable, Codable {
    public var id: String { "\(type)_\(channel)_\(Int(frequencyMHz))" }
    public let channel: Int
    public let frequencyMHz: Float
    public let centreFrequencyHz: Int
    public let bandwidthMHz: Int
    public let bandwidthCode: Int
    public let type: String
    
    public var isT2: Bool {
        type.uppercased().contains("T2")
    }
    
    public var displayTitle: String {
        "CH \(channel) (\(String(format: "%.1f", frequencyMHz)) MHz) - \(bandwidthMHz) MHz [\(type)]"
    }
    
    public var shortTitle: String {
        "CH \(channel) (\(String(format: "%.0f", frequencyMHz)) MHz)"
    }
}

public struct TerrestrialBandGroup: Identifiable, Codable {
    public var id: String { name }
    public let name: String
    public let channels: [TerrestrialChannel]
}

public final class TerrestrialDatabase {
    public static let shared = TerrestrialDatabase()
    
    public private(set) var bands: [TerrestrialBandGroup] = []
    public private(set) var uhfChannels: [TerrestrialChannel] = []
    public private(set) var vhfChannels: [TerrestrialChannel] = []
    public private(set) var cableChannels: [TerrestrialChannel] = []
    
    private init() {
        loadDatabase()
    }
    private func loadDatabase() {
        var resourceURL: URL? = Bundle.main.url(forResource: "terrestrial", withExtension: "json")
        #if SWIFT_PACKAGE
        if resourceURL == nil {
            resourceURL = Bundle.module.url(forResource: "terrestrial", withExtension: "json")
        }
        #endif
        guard let url = resourceURL else {
            // Fallback: generate default UHF channels if bundle resource lookup fails
            loadDefaultUHF()
            return
        }
        
        do {
            let data = try Data(contentsOf: url)
            let decoded = try JSONDecoder().decode([TerrestrialBandGroup].self, from: data)
            self.bands = decoded
            
            for band in decoded {
                let nameLower = band.name.lowercased()
                if nameLower.contains("uhf") {
                    self.uhfChannels = band.channels
                } else if nameLower.contains("vhf") {
                    self.vhfChannels = band.channels
                } else if nameLower.contains("cable") {
                    self.cableChannels = band.channels
                }
            }
        } catch {
            print("Failed to decode terrestrial.json: \(error). Using defaults.")
            loadDefaultUHF()
        }
    }
    
    private func loadDefaultUHF() {
        var chs: [TerrestrialChannel] = []
        // Standard European UHF DVB-T/T2 grid: CH 21 (474 MHz) to CH 69 (858 MHz), 8 MHz steps
        for ch in 21...69 {
            let freq = 474.0 + Float(ch - 21) * 8.0
            chs.append(TerrestrialChannel(
                channel: ch,
                frequencyMHz: freq,
                centreFrequencyHz: Int(freq * 1_000_000),
                bandwidthMHz: 8,
                bandwidthCode: 0,
                type: "DVB-T2"
            ))
        }
        self.uhfChannels = chs
    }
    
    public func findUHFChannel(forFrequency freqMHz: Float) -> TerrestrialChannel? {
        uhfChannels.first { abs($0.frequencyMHz - freqMHz) <= 4.0 }
    }
}
