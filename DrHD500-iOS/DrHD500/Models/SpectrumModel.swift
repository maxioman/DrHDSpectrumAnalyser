//
//  SpectrumModel.swift
//  DrHD500
//
//  Reversed from com.drhd.finder500.base.SpectrumInfo
//  Real-time 300-point RF sweep processor with interactive markers, Max/Min/Avg hold,
//  and Satellite / Terrestrial UHF frequency tracking.
//

import Foundation

public struct SpectrumModel: Equatable {
    public var centerFrequencyMHz: Float = 670.0 // Default to middle of UHF (474-866 MHz)
    public var spanMHz: Int = 400                // Default to Full UHF span (400 MHz)
    public var refLevelDbuV: Int = 85            // Reference level in dBµV (60 .. 110)
    public var activeBand: SignalBand = .uhf
    
    // Normalized 0.0 ... 1.0 (raw byte / 254.0)
    public var levels: [Float] = [Float](repeating: 0.0, count: 300)
    public var maxHoldLevels: [Float] = [Float](repeating: 0.0, count: 300)
    public var minHoldLevels: [Float] = [Float](repeating: 1.0, count: 300)
    public var avgLevels: [Float] = [Float](repeating: 0.0, count: 300)
    
    // Peak Tracking
    public var peakFrequencyMHz: Float = 0.0
    public var peakLevelDbuV: Float = 0.0
    public var peakIndex: Int = 0
    public var peakChannel: TerrestrialChannel?
    
    // User Interactive Marker
    public var markerIndex: Int? = nil
    public var markerFrequencyMHz: Float? = nil
    public var markerLevelDbuV: Float? = nil
    public var markerChannel: TerrestrialChannel? = nil
    
    // Trace Display Toggles
    public var showLive: Bool = true
    public var showMaxHold: Bool = true
    public var showAvg: Bool = false
    public var showMinHold: Bool = false
    public var isTrackingPeak: Bool = true
    
    public var lastUpdated: Date = Date()
    
    private var tempBuffer = [Float](repeating: 0.0, count: 300)
    private var receivedChunks = Set<Int>()
    private var isFirstSweep: Bool = true
    
    public init() {}
    
    public var startFrequencyMHz: Float {
        centerFrequencyMHz - (Float(spanMHz) / 2.0)
    }
    
    public var endFrequencyMHz: Float {
        centerFrequencyMHz + (Float(spanMHz) / 2.0)
    }
    
    public mutating func ingestPacket(_ packet: [UInt8]) -> Bool {
        guard packet.count >= 104, packet[0] == DrHDConstants.incomingHeaderByte else {
            return false
        }
        
        let packetType = Int(packet[1]) & 0xFF
        let chunkIndex = Int(packet[1] & 0x0F)
        
        // Spectrum packets: 0, 1, 2 or 32..49 or 128..139
        let isSpectrum = (packetType <= 2) || (packetType >= 32 && packetType <= 49) || (packetType >= 128 && packetType <= 139)
        guard isSpectrum, chunkIndex < 3 else { return false }
        
        let startOffset = chunkIndex * 100
        for i in 0..<100 {
            let rawByte = packet[i + 2]
            tempBuffer[startOffset + i] = Float(rawByte) / 254.0
        }
        receivedChunks.insert(chunkIndex)
        
        // If all 3 chunks (300 points) collected, commit frame
        if receivedChunks.count >= 3 {
            levels = tempBuffer
            
            if isFirstSweep {
                maxHoldLevels = levels
                minHoldLevels = levels
                avgLevels = levels
                isFirstSweep = false
            } else {
                for i in 0..<300 {
                    maxHoldLevels[i] = max(maxHoldLevels[i], levels[i])
                    minHoldLevels[i] = min(minHoldLevels[i], levels[i])
                    avgLevels[i] = (levels[i] + (avgLevels[i] * 9.0)) / 10.0
                }
            }
            
            // Calculate Peak
            if let (maxIdx, maxVal) = levels.enumerated().max(by: { $0.element < $1.element }) {
                peakIndex = maxIdx
                let fraction = Float(maxIdx) / 299.0
                peakFrequencyMHz = startFrequencyMHz + fraction * Float(spanMHz)
                peakLevelDbuV = calculateDbuV(normalizedVal: maxVal)
                
                if activeBand == .uhf {
                    peakChannel = TerrestrialDatabase.shared.findUHFChannel(forFrequency: peakFrequencyMHz)
                } else {
                    peakChannel = nil
                }
                
                if isTrackingPeak {
                    updateMarker(atIndex: maxIdx)
                }
            }
            
            // Re-evaluate user marker if set
            if let mIdx = markerIndex, !isTrackingPeak {
                updateMarker(atIndex: mIdx)
            }
            
            receivedChunks.removeAll()
            lastUpdated = Date()
            return true
        }
        return false
    }
    
    public func calculateDbuV(normalizedVal: Float) -> Float {
        // Dynamic range is approx 60 dB down from Ref Level
        let minDbuV = Float(refLevelDbuV) - 60.0
        return minDbuV + (normalizedVal * 60.0)
    }
    
    public mutating func updateMarker(atIndex idx: Int) {
        let clamped = max(0, min(299, idx))
        markerIndex = clamped
        let fraction = Float(clamped) / 299.0
        let freq = startFrequencyMHz + fraction * Float(spanMHz)
        markerFrequencyMHz = freq
        markerLevelDbuV = calculateDbuV(normalizedVal: levels[clamped])
        
        if activeBand == .uhf {
            markerChannel = TerrestrialDatabase.shared.findUHFChannel(forFrequency: freq)
        } else {
            markerChannel = nil
        }
    }
    
    public mutating func setMarkerByFrequency(_ freqMHz: Float) {
        let clampedFreq = max(startFrequencyMHz, min(endFrequencyMHz, freqMHz))
        let fraction = (clampedFreq - startFrequencyMHz) / Float(spanMHz)
        let idx = Int(round(fraction * 299.0))
        isTrackingPeak = false
        updateMarker(atIndex: idx)
    }
    
    public mutating func resetMaxHold() {
        maxHoldLevels = levels
        minHoldLevels = levels
        avgLevels = levels
    }
}
