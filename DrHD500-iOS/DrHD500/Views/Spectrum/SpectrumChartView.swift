//
//  SpectrumChartView.swift
//  DrHD500
//
//  Professional RF Spectrum Analyzer with interactive drag-to-inspect marker,
//  peak detection, UHF/VHF/Satellite channel overlays, and multi-trace visualization.
//

import SwiftUI
#if canImport(UIKit)
import UIKit
#endif

public struct SpectrumChartView: View {
    @ObservedObject var session: DeviceSession
    @State private var showChannelSheet = false
    
    private var cardBackground: Color {
        #if os(iOS)
        return Color(uiColor: .secondarySystemGroupedBackground)
        #else
        return Color.secondary.opacity(0.12)
        #endif
    }
    
    public var body: some View {
        VStack(spacing: 8) {
            // MARK: - Band & Channel Quick Bar
            HStack(spacing: 8) {
                // Band Selector Menu
                Menu {
                    ForEach(SignalBand.allCases) { band in
                        Button(action: { session.selectBand(band) }) {
                            HStack {
                                Text(band.rawValue)
                                if session.activeBand == band {
                                    Image(systemName: "checkmark")
                                }
                            }
                        }
                    }
                } label: {
                    HStack(spacing: 4) {
                        Image(systemName: "antenna.radiowaves.left.and.right")
                        Text(session.activeBand.shortName)
                            .fontWeight(.bold)
                        Image(systemName: "chevron.down")
                            .font(.caption2)
                    }
                    .font(.caption)
                    .padding(.horizontal, 10)
                    .padding(.vertical, 6)
                    .background(Color.blue.opacity(0.18))
                    .foregroundColor(.blue)
                    .cornerRadius(8)
                }
                
                // Active Channel or Transponder Selector
                if session.activeBand == .uhf {
                    Button(action: { showChannelSheet = true }) {
                        HStack(spacing: 6) {
                            Text(session.selectedUHFChannel?.displayTitle ?? "Select UHF Channel")
                                .font(.caption)
                                .fontWeight(.semibold)
                                .lineLimit(1)
                            Image(systemName: "magnifyingglass")
                                .font(.caption2)
                        }
                        .padding(.horizontal, 10)
                        .padding(.vertical, 6)
                        .background(cardBackground)
                        .cornerRadius(8)
                    }
                    
                    // Stepper buttons for channels
                    HStack(spacing: 2) {
                        Button(action: { stepUHF(by: -1) }) {
                            Image(systemName: "chevron.left")
                                .font(.caption)
                                .padding(6)
                                .background(cardBackground)
                                .clipShape(Circle())
                        }
                        Button(action: { stepUHF(by: 1) }) {
                            Image(systemName: "chevron.right")
                                .font(.caption)
                                .padding(6)
                                .background(cardBackground)
                                .clipShape(Circle())
                        }
                    }
                } else if session.activeBand == .satellite {
                    HStack(spacing: 4) {
                        Text("\(session.selectedSatellite?.name ?? "Sat") • \(session.selectedTransponder?.displayString ?? "")")
                            .font(.caption)
                            .fontWeight(.semibold)
                            .lineLimit(1)
                    }
                    .padding(.horizontal, 10)
                    .padding(.vertical, 6)
                    .background(cardBackground)
                    .cornerRadius(8)
                }
                
                Spacer()
                
                // Peak Track / Reset Button
                Button(action: { session.spectrum.resetMaxHold() }) {
                    Image(systemName: "arrow.counterclockwise")
                        .font(.caption)
                        .padding(6)
                        .background(cardBackground)
                        .clipShape(Circle())
                }
            }
            .padding(.horizontal)
            
            // MARK: - Frequency & Marker Info Readout Bar
            HStack {
                // Peak readout
                VStack(alignment: .leading, spacing: 1) {
                    Text("PEAK")
                        .font(.system(size: 9, weight: .bold))
                        .foregroundColor(.yellow)
                    HStack(spacing: 4) {
                        Text(String(format: "%.1f MHz", session.spectrum.peakFrequencyMHz))
                            .font(.system(.subheadline, design: .monospaced))
                            .fontWeight(.bold)
                            .foregroundColor(.yellow)
                        Text(String(format: "(%.1f dBµV)", session.spectrum.peakLevelDbuV))
                            .font(.caption2)
                            .foregroundColor(.secondary)
                        if let ch = session.spectrum.peakChannel {
                            Text("[\(ch.shortTitle)]")
                                .font(.system(size: 10, weight: .bold))
                                .foregroundColor(.green)
                        }
                    }
                }
                
                Spacer()
                
                // Marker Readout (if placed)
                if let mFreq = session.spectrum.markerFrequencyMHz, let mLev = session.spectrum.markerLevelDbuV {
                    VStack(alignment: .trailing, spacing: 1) {
                        HStack(spacing: 4) {
                            Text("MARKER")
                                .font(.system(size: 9, weight: .bold))
                                .foregroundColor(.red)
                            if session.spectrum.isTrackingPeak {
                                Text("(TRACKING PEAK)")
                                    .font(.system(size: 8))
                                    .foregroundColor(.secondary)
                            }
                        }
                        HStack(spacing: 4) {
                            Text(String(format: "%.1f MHz", mFreq))
                                .font(.system(.subheadline, design: .monospaced))
                                .fontWeight(.bold)
                                .foregroundColor(.red)
                            Text(String(format: "(%.1f dBµV)", mLev))
                                .font(.caption2)
                                .foregroundColor(.secondary)
                            if let ch = session.spectrum.markerChannel {
                                Text("[\(ch.shortTitle)]")
                                    .font(.system(size: 10, weight: .bold))
                                    .foregroundColor(.cyan)
                            }
                        }
                    }
                }
            }
            .padding(.horizontal)
            .padding(.vertical, 4)
            .background(cardBackground)
            .cornerRadius(8)
            .padding(.horizontal)
            
            // MARK: - Interactive Spectrum Graph Canvas
            GeometryReader { geo in
                let width = geo.size.width
                let height = geo.size.height
                let paddingLeft: CGFloat = 36
                let paddingBottom: CGFloat = 20
                let plotWidth = max(10, width - paddingLeft)
                let plotHeight = max(10, height - paddingBottom)
                
                ZStack(alignment: .topLeading) {
                    // 1. Grid Background & dBµV Axis Labels
                    Path { path in
                        for i in 0...4 {
                            let y = plotHeight * CGFloat(i) / 4.0
                            path.move(to: CGPoint(x: paddingLeft, y: y))
                            path.addLine(to: CGPoint(x: width, y: y))
                        }
                        for i in 0...5 {
                            let x = paddingLeft + (plotWidth * CGFloat(i) / 5.0)
                            path.move(to: CGPoint(x: x, y: 0))
                            path.addLine(to: CGPoint(x: x, y: plotHeight))
                        }
                    }
                    .stroke(Color.gray.opacity(0.18), style: StrokeStyle(lineWidth: 1, dash: [4, 4]))
                    
                    // Y-Axis dBµV Labels
                    ForEach(0...4, id: \.self) { i in
                        let y = plotHeight * CGFloat(i) / 4.0
                        let dbuv = Float(session.spectrum.refLevelDbuV) - Float(i) * 15.0
                        Text(String(format: "%.0f", dbuv))
                            .font(.system(size: 9, design: .monospaced))
                            .foregroundColor(.secondary)
                            .position(x: paddingLeft / 2.0, y: y)
                    }
                    
                    // 2. Max Hold Trace (Warm Amber)
                    if session.spectrum.showMaxHold {
                        Path { path in
                            let points = session.spectrum.maxHoldLevels
                            guard !points.isEmpty else { return }
                            for (idx, val) in points.enumerated() {
                                let x = paddingLeft + (CGFloat(idx) / CGFloat(points.count - 1)) * plotWidth
                                let y = plotHeight * (1.0 - CGFloat(val))
                                if idx == 0 { path.move(to: CGPoint(x: x, y: y)) }
                                else { path.addLine(to: CGPoint(x: x, y: y)) }
                            }
                        }
                        .stroke(Color.orange.opacity(0.75), lineWidth: 1.2)
                    }
                    
                    // 3. Average Trace (Green)
                    if session.spectrum.showAvg {
                        Path { path in
                            let points = session.spectrum.avgLevels
                            guard !points.isEmpty else { return }
                            for (idx, val) in points.enumerated() {
                                let x = paddingLeft + (CGFloat(idx) / CGFloat(points.count - 1)) * plotWidth
                                let y = plotHeight * (1.0 - CGFloat(val))
                                if idx == 0 { path.move(to: CGPoint(x: x, y: y)) }
                                else { path.addLine(to: CGPoint(x: x, y: y)) }
                            }
                        }
                        .stroke(Color.green.opacity(0.8), lineWidth: 1.2)
                    }
                    
                    // 4. Live RF Sweep Trace (Neon Cyan with Area Fill)
                    if session.spectrum.showLive {
                        // Area gradient fill
                        Path { path in
                            let points = session.spectrum.levels
                            guard !points.isEmpty else { return }
                            path.move(to: CGPoint(x: paddingLeft, y: plotHeight))
                            for (idx, val) in points.enumerated() {
                                let x = paddingLeft + (CGFloat(idx) / CGFloat(points.count - 1)) * plotWidth
                                let y = plotHeight * (1.0 - CGFloat(val))
                                path.addLine(to: CGPoint(x: x, y: y))
                            }
                            path.addLine(to: CGPoint(x: width, y: plotHeight))
                            path.closeSubpath()
                        }
                        .fill(
                            LinearGradient(
                                colors: [Color.cyan.opacity(0.35), Color.cyan.opacity(0.02)],
                                startPoint: .top,
                                endPoint: .bottom
                            )
                        )
                        
                        // Line stroke
                        Path { path in
                            let points = session.spectrum.levels
                            guard !points.isEmpty else { return }
                            for (idx, val) in points.enumerated() {
                                let x = paddingLeft + (CGFloat(idx) / CGFloat(points.count - 1)) * plotWidth
                                let y = plotHeight * (1.0 - CGFloat(val))
                                if idx == 0 { path.move(to: CGPoint(x: x, y: y)) }
                                else { path.addLine(to: CGPoint(x: x, y: y)) }
                            }
                        }
                        .stroke(Color.cyan, lineWidth: 1.8)
                    }
                    
                    // 5. Interactive Marker Cursor Line & Peak Dot
                    if let mIdx = session.spectrum.markerIndex {
                        let mx = paddingLeft + (CGFloat(mIdx) / 299.0) * plotWidth
                        let my = plotHeight * (1.0 - CGFloat(session.spectrum.levels[mIdx]))
                        
                        // Vertical marker line
                        Path { path in
                            path.move(to: CGPoint(x: mx, y: 0))
                            path.addLine(to: CGPoint(x: mx, y: plotHeight))
                        }
                        .stroke(Color.red, style: StrokeStyle(lineWidth: 1.2, dash: [4, 4]))
                        
                        // Marker target circle
                        Circle()
                            .fill(Color.red)
                            .frame(width: 8, height: 8)
                            .position(x: mx, y: my)
                    }
                    
                    // X-Axis Frequency Scale Labels
                    ForEach(0...5, id: \.self) { i in
                        let frac = Float(i) / 5.0
                        let freq = session.spectrum.startFrequencyMHz + frac * Float(session.spectrum.spanMHz)
                        let x = paddingLeft + (plotWidth * CGFloat(i) / 5.0)
                        Text(String(format: "%.0f", freq))
                            .font(.system(size: 9, design: .monospaced))
                            .foregroundColor(.secondary)
                            .position(x: x, y: plotHeight + 10)
                    }
                }
                .contentShape(Rectangle())
                .gesture(
                    DragGesture(minimumDistance: 0)
                        .onChanged { value in
                            let localX = value.location.x - paddingLeft
                            let fraction = max(0.0, min(1.0, localX / plotWidth))
                            let idx = Int(round(fraction * 299.0))
                            session.spectrum.isTrackingPeak = false
                            session.spectrum.updateMarker(atIndex: idx)
                        }
                )
            }
            .frame(height: 230)
            .padding(.horizontal, 8)
            
            // MARK: - Spectrum Analyzer Controls (Span, Ref Level, Trace Toggles)
            VStack(spacing: 8) {
                // Span & Ref Level Selectors
                HStack(spacing: 8) {
                    Text("Span:")
                        .font(.caption)
                        .foregroundColor(.secondary)
                    
                    Menu {
                        Picker("Span", selection: $session.spectrum.spanMHz) {
                            if session.activeBand == .uhf {
                                Text("Full UHF (400 MHz)").tag(400)
                                Text("100 MHz").tag(100)
                                Text("50 MHz").tag(50)
                                Text("8 MHz (CH)").tag(8)
                            } else {
                                Text("Full (1.2 GHz)").tag(1200)
                                Text("600 MHz").tag(600)
                                Text("300 MHz").tag(300)
                                Text("100 MHz").tag(100)
                            }
                        }
                    } label: {
                        HStack(spacing: 4) {
                            Text(session.activeBand == .uhf && session.spectrum.spanMHz == 400 ? "400 MHz" : "\(session.spectrum.spanMHz) MHz")
                                .font(.caption.bold())
                            Image(systemName: "chevron.up.chevron.down")
                                .font(.system(size: 9))
                        }
                        .padding(.horizontal, 8)
                        .padding(.vertical, 4)
                        .background(Color.primary.opacity(0.06))
                        .cornerRadius(6)
                    }
                    
                    Spacer()
                    
                    Text("Ref:")
                        .font(.caption)
                        .foregroundColor(.secondary)
                    
                    Menu {
                        Picker("Ref Level", selection: $session.spectrum.refLevelDbuV) {
                            ForEach(stride(from: 60, through: 110, by: 5).map { $0 }, id: \.self) { lev in
                                Text("\(lev) dBµV").tag(lev)
                            }
                        }
                    } label: {
                        HStack(spacing: 4) {
                            Text("\(session.spectrum.refLevelDbuV) dBµV")
                                .font(.caption.bold())
                            Image(systemName: "chevron.up.chevron.down")
                                .font(.system(size: 9))
                        }
                        .padding(.horizontal, 8)
                        .padding(.vertical, 4)
                        .background(Color.primary.opacity(0.06))
                        .cornerRadius(6)
                    }
                }
                .padding(.horizontal)
                
                // Trace & Marker Toggles
                HStack(spacing: 12) {
                    Toggle("Max Hold", isOn: $session.spectrum.showMaxHold)
                        .toggleStyle(CheckToggleStyle(color: .orange))
                    
                    Toggle("Average", isOn: $session.spectrum.showAvg)
                        .toggleStyle(CheckToggleStyle(color: .green))
                    
                    Spacer()
                    
                    Button(action: {
                        session.spectrum.isTrackingPeak = true
                        session.spectrum.updateMarker(atIndex: session.spectrum.peakIndex)
                    }) {
                        Text("Track Peak")
                            .font(.caption2)
                            .fontWeight(.semibold)
                            .padding(.horizontal, 8)
                            .padding(.vertical, 4)
                            .background(session.spectrum.isTrackingPeak ? Color.yellow.opacity(0.3) : cardBackground)
                            .foregroundColor(session.spectrum.isTrackingPeak ? .primary : .secondary)
                            .cornerRadius(6)
                    }
                }
                .padding(.horizontal)
            }
            .padding(.vertical, 6)
            .background(cardBackground)
            .cornerRadius(10)
            .padding(.horizontal)
        }
        .sheet(isPresented: $showChannelSheet) {
            UHFChannelPickerSheet(session: session)
        }
    }
    
    private func stepUHF(by delta: Int) {
        let chs = TerrestrialDatabase.shared.uhfChannels
        guard !chs.isEmpty else { return }
        let currentIdx = chs.firstIndex(where: { $0.channel == session.selectedUHFChannel?.channel }) ?? 0
        let nextIdx = max(0, min(chs.count - 1, currentIdx + delta))
        session.selectUHFChannel(chs[nextIdx])
    }
}

// MARK: - UHF Channel Picker Sheet
struct UHFChannelPickerSheet: View {
    @ObservedObject var session: DeviceSession
    @Environment(\.dismiss) private var dismiss
    @State private var search = ""
    
    var filteredChannels: [TerrestrialChannel] {
        let all = TerrestrialDatabase.shared.uhfChannels
        if search.isEmpty { return all }
        return all.filter {
            "\($0.channel)".contains(search) ||
            String(format: "%.1f", $0.frequencyMHz).contains(search)
        }
    }
    
    var body: some View {
        NavigationView {
            List(filteredChannels) { ch in
                Button(action: {
                    session.selectUHFChannel(ch)
                    dismiss()
                }) {
                    HStack {
                        VStack(alignment: .leading, spacing: 2) {
                            Text("Channel \(ch.channel)")
                                .font(.headline)
                            Text("\(String(format: "%.1f", ch.frequencyMHz)) MHz • \(ch.bandwidthMHz) MHz Bandwidth [\(ch.type)]")
                                .font(.caption)
                                .foregroundColor(.secondary)
                        }
                        
                        Spacer()
                        
                        if session.selectedUHFChannel?.channel == ch.channel {
                            Image(systemName: "checkmark")
                                .foregroundColor(.blue)
                        }
                    }
                }
                .foregroundColor(.primary)
            }
            .searchable(text: $search, prompt: "Search Channel or Frequency")
            .navigationTitle("UHF TV Channels (EU)")
            #if os(iOS)
            .navigationBarTitleDisplayMode(.inline)
            #endif
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Done") { dismiss() }
                }
            }
        }
    }
}

// Simple Checkbox Style Toggle for Spectrum
struct CheckToggleStyle: ToggleStyle {
    let color: Color
    func makeBody(configuration: Configuration) -> some View {
        Button(action: { configuration.isOn.toggle() }) {
            HStack(spacing: 4) {
                Image(systemName: configuration.isOn ? "checkmark.square.fill" : "square")
                    .foregroundColor(configuration.isOn ? color : .secondary)
                    .font(.caption)
                configuration.label
                    .font(.caption2)
                    .foregroundColor(.primary)
            }
        }
    }
}
