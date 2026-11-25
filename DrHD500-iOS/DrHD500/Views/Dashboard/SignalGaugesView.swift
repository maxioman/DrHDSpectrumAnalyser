//
//  SignalGaugesView.swift
//  DrHD500
//
//  Primary satellite alignment dashboard with live gauges and DVB info
//

import SwiftUI
#if canImport(UIKit)
import UIKit
#endif

public struct SignalGaugesView: View {
    @ObservedObject var session: DeviceSession
    @State private var showSatellitePicker = false
    @State private var showUHFChannelPicker = false
    @State private var showDiSEqCPicker = false
    
    private var cardBackground: Color {
        #if os(iOS)
        return Color(uiColor: .secondarySystemGroupedBackground)
        #else
        return Color.secondary.opacity(0.12)
        #endif
    }
    
    private var screenBackground: Color {
        #if os(iOS)
        return Color(uiColor: .systemGroupedBackground)
        #else
        return Color.clear
        #endif
    }
    
    public var body: some View {
        ScrollView {
            VStack(spacing: 16) {
                // Band Selector Pill
                Picker("Band", selection: $session.activeBand) {
                    ForEach(SignalBand.allCases) { band in
                        Text(band.shortName).tag(band)
                    }
                }
                .pickerStyle(.segmented)
                .padding(.horizontal)
                .onChange(of: session.activeBand) { newBand in
                    session.selectBand(newBand)
                }
                
                // Top Tuning Bar
                HStack {
                    if session.activeBand.isTerrestrial {
                        Button(action: { showUHFChannelPicker = true }) {
                            HStack(spacing: 8) {
                                Image(systemName: "tv.fill")
                                    .foregroundColor(.blue)
                                VStack(alignment: .leading, spacing: 2) {
                                    Text(session.selectedUHFChannel?.displayTitle ?? "Select UHF Channel")
                                        .font(.headline)
                                        .foregroundColor(.primary)
                                    Text("Terrestrial TV Channel • EU Grid")
                                        .font(.caption)
                                        .foregroundColor(.secondary)
                                }
                                Spacer()
                                Image(systemName: "chevron.right")
                                    .font(.caption)
                                    .foregroundColor(.secondary)
                            }
                            .padding()
                            .background(cardBackground)
                            .cornerRadius(12)
                        }
                    } else {
                        Button(action: { showSatellitePicker = true }) {
                            HStack(spacing: 8) {
                                Image(systemName: "antenna.radiowaves.left.and.right")
                                    .foregroundColor(.accentColor)
                                VStack(alignment: .leading, spacing: 2) {
                                    Text(session.selectedSatellite?.name ?? "Select Satellite")
                                        .font(.headline)
                                        .foregroundColor(.primary)
                                    Text(session.selectedTransponder?.displayString ?? "No Transponder")
                                        .font(.caption)
                                        .foregroundColor(.secondary)
                                }
                                Spacer()
                                Image(systemName: "chevron.right")
                                    .font(.caption)
                                    .foregroundColor(.secondary)
                            }
                            .padding()
                            .background(cardBackground)
                            .cornerRadius(12)
                        }
                    }
                }
                .padding(.horizontal)
                
                // Demodulator Lock Banner
                HStack(spacing: 12) {
                    Circle()
                        .fill(session.signalMetrics.isLocked ? Color.green : Color.orange)
                        .frame(width: 14, height: 14)
                        .overlay(
                            Circle()
                                .stroke(session.signalMetrics.isLocked ? Color.green.opacity(0.4) : Color.orange.opacity(0.4), lineWidth: 4)
                        )
                    
                    Text(session.signalMetrics.isLocked ? "CARRIER LOCKED" : "SEARCHING FOR CARRIER...")
                        .font(.headline)
                        .fontWeight(.heavy)
                        .foregroundColor(session.signalMetrics.isLocked ? .green : .orange)
                    
                    Spacer()
                    
                    // Audio Beeper Toggle
                    Button(action: {
                        session.audioBeeper.isMuted.toggle()
                    }) {
                        Image(systemName: session.audioBeeper.isMuted ? "speaker.slash.fill" : "speaker.wave.3.fill")
                            .font(.title3)
                            .foregroundColor(session.audioBeeper.isMuted ? .gray : .accentColor)
                            .padding(8)
                            .background(Color.secondary.opacity(0.12))
                            .clipShape(Circle())
                    }
                }
                .padding()
                .background(cardBackground)
                .cornerRadius(12)
                .padding(.horizontal)
                
                // Big Circular SNR Meter
                CircularGaugeView(
                    value: session.signalMetrics.snrDb,
                    maxValue: 20.0,
                    title: "SIGNAL / NOISE (SNR)",
                    unit: "dB",
                    isLocked: session.signalMetrics.isLocked
                )
                .padding(.vertical, 8)
                
                // Linear Bars (Power & MER)
                VStack(spacing: 14) {
                    LinearBarGaugeView(
                        title: "SIGNAL LEVEL (PWR)",
                        value: session.signalMetrics.pwrDbuV,
                        minValue: 30.0,
                        maxValue: 110.0,
                        unit: "dBµV",
                        barColor: .blue
                    )
                    
                    LinearBarGaugeView(
                        title: "MODULATION ERROR RATIO (MER)",
                        value: session.signalMetrics.merDb > 0 ? session.signalMetrics.merDb : session.constellation.calculatedMER,
                        minValue: 0.0,
                        maxValue: 25.0,
                        unit: "dB",
                        barColor: .purple
                    )
                }
                .padding()
                .background(cardBackground)
                .cornerRadius(12)
                .padding(.horizontal)
                
                // Quality Metrics Cards (CBER & PE)
                HStack(spacing: 12) {
                    VStack(alignment: .leading, spacing: 4) {
                        Text("CBER (Pre-BER)")
                            .font(.caption)
                            .foregroundColor(.secondary)
                        Text(session.signalMetrics.cber > 0 ? String(format: "%.2e", session.signalMetrics.cber) : "< 1.0e-6")
                            .font(.system(.body, design: .monospaced))
                            .fontWeight(.bold)
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding()
                    .background(cardBackground)
                    .cornerRadius(12)
                    
                    VStack(alignment: .leading, spacing: 4) {
                        Text("PACKET ERRORS (PE)")
                            .font(.caption)
                            .foregroundColor(.secondary)
                        Text("\(session.signalMetrics.packetErrors)")
                            .font(.system(.body, design: .monospaced))
                            .fontWeight(.bold)
                            .foregroundColor(session.signalMetrics.packetErrors > 0 ? .red : .green)
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding()
                    .background(cardBackground)
                    .cornerRadius(12)
                }
                .padding(.horizontal)
                
                // DVB SI Telemetry (Network Name & Orbit Position)
                if !session.signalMetrics.networkName.isEmpty || !session.signalMetrics.orbitalPosition.isEmpty {
                    VStack(alignment: .leading, spacing: 8) {
                        HStack {
                            Image(systemName: "globe.europe.africa.fill")
                                .foregroundColor(.teal)
                            Text("DVB IDENTIFIED NETWORK")
                                .font(.caption)
                                .fontWeight(.bold)
                                .foregroundColor(.secondary)
                        }
                        
                        HStack {
                            if !session.signalMetrics.networkName.isEmpty {
                                Text(session.signalMetrics.networkName)
                                    .font(.headline)
                            }
                            Spacer()
                            if !session.signalMetrics.orbitalPosition.isEmpty {
                                Text(session.signalMetrics.orbitalPosition)
                                    .font(.headline)
                                    .foregroundColor(.teal)
                            }
                        }
                        
                        if !session.signalMetrics.channelsFound.isEmpty {
                            Divider()
                            Text("Channels on Transponder: " + session.signalMetrics.channelsFound.prefix(5).joined(separator: ", "))
                                .font(.caption)
                                .foregroundColor(.secondary)
                        }
                    }
                    .padding()
                    .background(cardBackground)
                    .cornerRadius(12)
                    .padding(.horizontal)
                }
                
                // Quick LNB Power & DiSEqC Controls
                VStack(alignment: .leading, spacing: 10) {
                    HStack {
                        Text("LNB POWER & TONE")
                            .font(.caption)
                            .fontWeight(.bold)
                            .foregroundColor(.secondary)
                        Spacer()
                        Button("DiSEqC Setup") {
                            showDiSEqCPicker = true
                        }
                        .font(.caption)
                        .fontWeight(.semibold)
                    }
                    
                    ScrollView(.horizontal, showsIndicators: false) {
                        HStack(spacing: 8) {
                            ForEach(LNBVoltageMode.allCases) { mode in
                                Button(action: { session.setLNBVoltage(mode) }) {
                                    Text(mode.label)
                                        .font(.caption2)
                                        .fontWeight(.semibold)
                                        .padding(.horizontal, 10)
                                        .padding(.vertical, 8)
                                        .background(session.lnbVoltageMode == mode ? Color.accentColor : Color.secondary.opacity(0.12))
                                        .foregroundColor(session.lnbVoltageMode == mode ? .white : .primary)
                                        .cornerRadius(8)
                                }
                            }
                        }
                    }
                }
                .padding()
                .background(cardBackground)
                .cornerRadius(12)
                .padding(.horizontal)
            }
            .padding(.vertical)
        }
        .background(screenBackground)
        .sheet(isPresented: $showSatellitePicker) {
            SatellitePickerSheet(session: session)
        }
        .sheet(isPresented: $showDiSEqCPicker) {
            DiSEqCControlSheet(session: session)
        }
        .sheet(isPresented: $showUHFChannelPicker) {
            UHFChannelPickerSheet(session: session)
        }
    }
}
