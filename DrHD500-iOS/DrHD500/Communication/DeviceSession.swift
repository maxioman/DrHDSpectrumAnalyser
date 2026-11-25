//
//  DeviceSession.swift
//  DrHD500
//
//  Central ViewModel managing device communication, telemetry, and background polling
//

import Foundation
import Combine
import SwiftUI

public enum TransportType: String, CaseIterable, Identifiable {
    case simulator = "Simulator"
    case ble = "Bluetooth Low Energy"
    case wifi = "Wi-Fi / TCP"
    
    public var id: String { rawValue }
}

public enum AppMode: String, CaseIterable, Identifiable {
    case measures = "Signal Measures"
    case spectrum = "Spectrum Analyzer"
    case constellation = "Constellation"
    
    public var id: String { rawValue }
}

@MainActor
public final class DeviceSession: ObservableObject {
    // Current Active Models
    @Published public var connectionState: TransportConnectionState = .disconnected
    @Published public var currentTransportType: TransportType = .simulator
    @Published public var currentMode: AppMode = .measures
    
    @Published public var signalMetrics = SignalMetrics()
    @Published public var hardwareTelemetry = HardwareTelemetry()
    @Published public var spectrum = SpectrumModel()
    @Published public var constellation = ConstellationModel()
    
    // Tuning settings
    @Published public var activeBand: SignalBand = .satellite
    @Published public var selectedSatellite: Satellite?
    @Published public var selectedTransponder: Transponder?
    @Published public var selectedUHFChannel: TerrestrialChannel?
    @Published public var selectedConverter: LNBConverter = .standardUniversalKu
    @Published public var lnbVoltageMode: LNBVoltageMode = .volt13
    
    // TCP Configuration
    @Published public var tcpHost: String = "192.168.4.1"
    @Published public var tcpPort: UInt16 = 8080
    
    // Audio Beeper
    public let audioBeeper = AudioToneGenerator.shared
    
    // Internal Components & Transports
    public let bleTransport = BLETransport()
    private let mockTransport = MockTransport()
    private var tcpTransport: TCPTransport?
    
    private var transport: SatFinderTransport
    private let framer = DrHDFramer()
    private var cancellables = Set<AnyCancellable>()
    private var pollTimer: Timer?
    private var serviceTick: Int = 0
    
    public init() {
        // Default to Simulator for immediate out-of-the-box readiness
        self.transport = mockTransport
        
        setupFramer()
        setupTransportSubscription()
        
        // Auto-select initial satellite and UHF channel
        if let firstSat = SatelliteDatabase.shared.satellites.first {
            self.selectedSatellite = firstSat
            self.selectedTransponder = firstSat.transponders.first
        }
        if let firstUHF = TerrestrialDatabase.shared.uhfChannels.first {
            self.selectedUHFChannel = firstUHF
        }
        self.spectrum.activeBand = self.activeBand
        self.spectrum.centerFrequencyMHz = selectedUHFChannel?.frequencyMHz ?? 670.0
        self.spectrum.spanMHz = 400
    }
    
    private func setupFramer() {
        framer.onPacketReceived = { [weak self] packet in
            self?.handleIncomingPacket(packet)
        }
    }
    
    private func setupTransportSubscription() {
        cancellables.removeAll()
        transport.onBytesReceived = { [weak self] bytes in
            self?.framer.appendBytes(bytes)
        }
        
        transport.connectionStatePublisher
            .receive(on: DispatchQueue.main)
            .sink { [weak self] state in
                self?.connectionState = state
                if case .connected = state {
                    self?.startPollingLoop()
                } else {
                    self?.stopPollingLoop()
                }
            }
            .store(in: &cancellables)
    }
    
    public func switchTransport(to type: TransportType) {
        disconnect()
        currentTransportType = type
        
        switch type {
        case .simulator:
            transport = mockTransport
        case .ble:
            transport = bleTransport
            bleTransport.startScanning()
        case .wifi:
            let tcp = TCPTransport(host: tcpHost, port: tcpPort)
            self.tcpTransport = tcp
            transport = tcp
        }
        
        setupTransportSubscription()
    }
    
    public func connectBLEDevice(_ device: DiscoveredBLEDevice) {
        if currentTransportType != .ble {
            switchTransport(to: .ble)
        }
        bleTransport.connectToDevice(device)
    }
    
    public func connect() {
        if currentTransportType == .wifi {
            let tcp = TCPTransport(host: tcpHost, port: tcpPort)
            self.tcpTransport = tcp
            self.transport = tcp
            setupTransportSubscription()
        }
        transport.connect()
    }
    
    public func disconnect() {
        transport.disconnect()
        stopPollingLoop()
        audioBeeper.stopTone()
    }
    
    // MARK: - Periodic Polling Loop (130ms cycle matching DrHD hardware)
    private func startPollingLoop() {
        pollTimer?.invalidate()
        pollTimer = Timer.scheduledTimer(withTimeInterval: 0.13, repeats: true) { [weak self] _ in
            Task { @MainActor [weak self] in
                self?.sendNextPollCommand()
            }
        }
    }
    
    private func stopPollingLoop() {
        pollTimer?.invalidate()
        pollTimer = nil
    }
    
    private func sendNextPollCommand() {
        serviceTick += 1
        
        // Ping service info (battery, voltages) every ~3 seconds
        if serviceTick % 25 == 0 {
            let srvPacket = DrHDPacketBuilder.makeServicePacket()
            transport.sendBytes(srvPacket)
            return
        }
        
        // Branch 1: Terrestrial / Cable (UHF, VHF, Cable)
        if activeBand.isTerrestrial {
            guard let ch = selectedUHFChannel ?? TerrestrialDatabase.shared.uhfChannels.first else { return }
            
            switch currentMode {
            case .measures:
                let packet = DrHDPacketBuilder.makeMeasuresTerPacket(
                    mode: DrHDConstants.packetModeTerSNR,
                    frequencyMHz: Int(ch.frequencyMHz),
                    isT2: ch.isT2,
                    bandwidthCode: ch.bandwidthCode,
                    powerMode: 0
                )
                transport.sendBytes(packet)
                
            case .constellation:
                // Mode 8 activates constellation stream on DrHD hardware
                let packet = DrHDPacketBuilder.makeMeasuresTerPacket(
                    mode: 8,
                    frequencyMHz: Int(ch.frequencyMHz),
                    isT2: ch.isT2,
                    bandwidthCode: ch.bandwidthCode,
                    powerMode: 0
                )
                transport.sendBytes(packet)
                
            case .spectrum:
                let spanBegin = max(50, Int(spectrum.centerFrequencyMHz) - (spectrum.spanMHz / 2))
                let packet = DrHDPacketBuilder.makeSpectrumPacket(
                    mode: DrHDConstants.packetModeSpectrumRDA,
                    spanBeginFreq: spanBegin,
                    span: spectrum.spanMHz,
                    refLevel: spectrum.refLevelDbuV,
                    powerMode: 0,
                    isRdaMode: false
                )
                transport.sendBytes(packet)
            }
            return
        }
        
        // Branch 2: Satellite (Ku / C-Band)
        guard let tp = selectedTransponder else { return }
        
        switch currentMode {
        case .measures:
            let lof = tp.freqMHz > 11700 ? selectedConverter.highFreq : selectedConverter.lowFreq
            let ifFreq = Int(abs(Float(lof) - tp.freqMHz))
            let isS2 = tp.system == .dvbS2
            
            let packet = DrHDPacketBuilder.makeMeasuresSatPacket(
                mode: DrHDConstants.packetModeSatSNR,
                intermediateFreq: ifFreq,
                symbolRate: tp.symbolRateKSps,
                isS2: isS2,
                powerMode: lnbVoltageMode.rawValue
            )
            transport.sendBytes(packet)
            
        case .constellation:
            let lof = tp.freqMHz > 11700 ? selectedConverter.highFreq : selectedConverter.lowFreq
            let ifFreq = Int(abs(Float(lof) - tp.freqMHz))
            let isS2 = tp.system == .dvbS2
            
            let packet = DrHDPacketBuilder.makeMeasuresSatPacket(
                mode: 8, // Mode 8 = Constellation in DrHD firmware
                intermediateFreq: ifFreq,
                symbolRate: tp.symbolRateKSps,
                isS2: isS2,
                powerMode: lnbVoltageMode.rawValue
            )
            transport.sendBytes(packet)
            
        case .spectrum:
            let lof = tp.freqMHz > 11700 ? selectedConverter.highFreq : selectedConverter.lowFreq
            let ifFreq = Int(abs(Float(lof) - tp.freqMHz))
            
            let packet = DrHDPacketBuilder.makeSpectrumPacket(
                mode: DrHDConstants.packetModeSpectrumRDA,
                spanBeginFreq: ifFreq,
                span: spectrum.spanMHz,
                refLevel: spectrum.refLevelDbuV,
                powerMode: lnbVoltageMode.rawValue,
                isRdaMode: true
            )
            transport.sendBytes(packet)
        }
    }
    
    public func selectBand(_ band: SignalBand) {
        self.activeBand = band
        self.spectrum.activeBand = band
        
        switch band {
        case .satellite:
            self.spectrum.centerFrequencyMHz = 11727.0
            self.spectrum.spanMHz = 1200
        case .uhf:
            let freq = selectedUHFChannel?.frequencyMHz ?? 670.0
            self.spectrum.centerFrequencyMHz = freq
            self.spectrum.spanMHz = 400
        case .vhf:
            self.spectrum.centerFrequencyMHz = 175.0
            self.spectrum.spanMHz = 180
        case .cable:
            self.spectrum.centerFrequencyMHz = 500.0
            self.spectrum.spanMHz = 600
        }
    }
    
    public func selectUHFChannel(_ channel: TerrestrialChannel) {
        self.selectedUHFChannel = channel
        if activeBand == .uhf {
            self.spectrum.centerFrequencyMHz = channel.frequencyMHz
        }
    }
    
    // MARK: - Incoming Packet Dispatcher
    private func handleIncomingPacket(_ packet: [UInt8]) {
        guard packet.count >= 104, packet[0] == DrHDConstants.incomingHeaderByte else { return }
        let type = packet[1]
        
        if type == 13 {
            // Hardware Status
            if let tel = HardwareTelemetry.parse(packet: packet) {
                self.hardwareTelemetry = tel
            }
        } else if type == 0 || type == 1 || type == 2 || (type >= 32 && type <= 49) || (type >= 128 && type <= 139) {
            // Spectrum Sweep Packet
            if spectrum.ingestPacket(packet) {
                // Spectrum frame complete
            }
        } else {
            // Sat Signal Measurements & Constellation
            self.signalMetrics = SignalMetrics.parseSatPacket(packet, currentMetrics: self.signalMetrics)
            _ = self.constellation.ingestPacket(packet)
            
            // Update audio pitch beeper
            self.audioBeeper.updateSignalTone(snrDb: self.signalMetrics.snrDb, isLocked: self.signalMetrics.isLocked)
        }
    }
    
    // MARK: - User Control Actions
    public func tuneTransponder(_ transponder: Transponder) {
        self.selectedTransponder = transponder
        // Update automatic voltage / 22K based on polarization & LOF
        let isHighBand = transponder.freqMHz > 11700
        switch transponder.polarization {
        case .vertical, .circularRight:
            self.lnbVoltageMode = isHighBand ? .volt13_22k : .volt13
        case .horizontal, .circularLeft:
            self.lnbVoltageMode = isHighBand ? .volt18_22k : .volt18
        }
    }
    
    public func sendDiSEqCCommand(_ hexCommand: String) {
        let packet = DrHDPacketBuilder.decodeDiseqcCommand(hexCommand)
        transport.sendBytes(packet)
    }
    
    public func setLNBVoltage(_ mode: LNBVoltageMode) {
        self.lnbVoltageMode = mode
    }
}
