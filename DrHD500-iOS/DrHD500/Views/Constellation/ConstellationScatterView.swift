//
//  ConstellationScatterView.swift
//  DrHD500
//
//  Professional 2D I/Q Constellation Analyzer with dynamic symbol targets
//  (QPSK, 8-PSK, 16/64/256-QAM), phosphor decay persistence, and live MER / EVM telemetry.
//

import SwiftUI
#if canImport(UIKit)
import UIKit
#endif

public struct ConstellationScatterView: View {
    @ObservedObject var session: DeviceSession
    
    private var cardBackground: Color {
        #if os(iOS)
        return Color(uiColor: .secondarySystemGroupedBackground)
        #else
        return Color.secondary.opacity(0.12)
        #endif
    }
    
    public var body: some View {
        VStack(spacing: 12) {
            // MARK: - Header with Modulation & Quality Badges
            HStack {
                VStack(alignment: .leading, spacing: 2) {
                    Text("CONSTELLATION ANALYZER")
                        .font(.caption)
                        .fontWeight(.bold)
                        .foregroundColor(.secondary)
                    
                    // Modulation Selection Menu
                    Menu {
                        ForEach(ConstellationModulation.allCases) { mod in
                            Button(action: { session.constellation.modulation = mod }) {
                                HStack {
                                    Text(mod.rawValue)
                                    if session.constellation.modulation == mod {
                                        Image(systemName: "checkmark")
                                    }
                                }
                            }
                        }
                    } label: {
                        HStack(spacing: 4) {
                            Text(session.constellation.modulation.rawValue)
                                .font(.subheadline)
                                .fontWeight(.bold)
                            Image(systemName: "chevron.down")
                                .font(.caption2)
                        }
                        .foregroundColor(.primary)
                    }
                }
                
                Spacer()
                
                // MER & EVM Readouts
                HStack(spacing: 12) {
                    VStack(alignment: .trailing, spacing: 1) {
                        Text("MER")
                            .font(.system(size: 9, weight: .bold))
                            .foregroundColor(.secondary)
                        Text(String(format: "%.1f dB", session.constellation.calculatedMER))
                            .font(.system(.headline, design: .monospaced))
                            .fontWeight(.bold)
                            .foregroundColor(merColor(session.constellation.calculatedMER))
                    }
                    
                    VStack(alignment: .trailing, spacing: 1) {
                        Text("EVM")
                            .font(.system(size: 9, weight: .bold))
                            .foregroundColor(.secondary)
                        Text(String(format: "%.1f %%", session.constellation.calculatedEVM))
                            .font(.system(.headline, design: .monospaced))
                            .fontWeight(.semibold)
                            .foregroundColor(.secondary)
                    }
                }
                .padding(.horizontal, 10)
                .padding(.vertical, 4)
                .background(cardBackground)
                .cornerRadius(8)
            }
            .padding(.horizontal)
            
            // MARK: - Oscilloscope Phosphor Canvas
            GeometryReader { geo in
                let size = min(geo.size.width, geo.size.height)
                let center = CGPoint(x: size / 2.0, y: size / 2.0)
                let radius = (size / 2.0) * 0.86
                
                ZStack {
                    // Scope Background
                    Circle()
                        .fill(Color(white: 0.04))
                        .frame(width: size, height: size)
                    
                    // Radar Ring Grids
                    Circle()
                        .stroke(Color.gray.opacity(0.2), lineWidth: 1)
                        .frame(width: radius * 2, height: radius * 2)
                    
                    Circle()
                        .stroke(Color.gray.opacity(0.12), style: StrokeStyle(lineWidth: 1, dash: [4, 4]))
                        .frame(width: radius * 1.414, height: radius * 1.414)
                    
                    // I / Q Crosshair Axes
                    Path { path in
                        // Horizontal (In-phase)
                        path.move(to: CGPoint(x: center.x - radius, y: center.y))
                        path.addLine(to: CGPoint(x: center.x + radius, y: center.y))
                        // Vertical (Quadrature)
                        path.move(to: CGPoint(x: center.x, y: center.y - radius))
                        path.addLine(to: CGPoint(x: center.x, y: center.y + radius))
                    }
                    .stroke(Color.gray.opacity(0.3), lineWidth: 1)
                    
                    // Ideal Symbol Target Rings
                    ForEach(Array(session.constellation.idealSymbols.enumerated()), id: \.offset) { _, sym in
                        let pos = mapPoint(sym, center: center, radius: radius, zoom: session.constellation.zoom)
                        Circle()
                            .stroke(Color.orange.opacity(0.6), lineWidth: ringWidthForModulation())
                            .frame(width: ringSizeForModulation(), height: ringSizeForModulation())
                            .position(pos)
                    }
                    
                    // Phosphor Decay Scatter Points
                    Canvas { context, _ in
                        for pt in session.constellation.points {
                            let rawPoint = CGPoint(x: CGFloat(pt.i), y: CGFloat(pt.q))
                            let screenPos = mapPoint(rawPoint, center: center, radius: radius, zoom: session.constellation.zoom)
                            
                            // Phosphor color transition from bright yellow (newest) -> cyan -> deep blue (oldest)
                            let color = phosphorColor(age: pt.age)
                            let rect = CGRect(x: screenPos.x - 2, y: screenPos.y - 2, width: 4, height: 4)
                            context.fill(Path(ellipseIn: rect), with: .color(color))
                        }
                    }
                    
                    // Quality Stamp Badge
                    VStack {
                        Spacer()
                        HStack {
                            Text(session.constellation.qualityRating)
                                .font(.system(size: 9, weight: .bold))
                                .padding(.horizontal, 6)
                                .padding(.vertical, 3)
                                .background(Color.black.opacity(0.6))
                                .foregroundColor(merColor(session.constellation.calculatedMER))
                                .cornerRadius(4)
                            Spacer()
                            Text("\(session.constellation.points.count) pts")
                                .font(.system(size: 9, design: .monospaced))
                                .foregroundColor(.gray)
                        }
                        .padding(8)
                    }
                }
                .frame(width: size, height: size)
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            }
            .frame(height: 290)
            .padding(.horizontal)
            
            // MARK: - Constellation Zoom & Control Bar
            HStack(spacing: 8) {
                Text("Zoom:")
                    .font(.caption)
                    .foregroundColor(.secondary)
                
                Menu {
                    Picker("Zoom", selection: $session.constellation.zoom) {
                        ForEach(ConstellationZoom.allCases) { z in
                            Text(z.rawValue).tag(z)
                        }
                    }
                } label: {
                    HStack(spacing: 4) {
                        Text(session.constellation.zoom.rawValue)
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
                
                Button(action: { session.constellation.clear() }) {
                    Label("Clear Points", systemImage: "trash")
                        .font(.caption)
                        .padding(.horizontal, 10)
                        .padding(.vertical, 6)
                        .background(cardBackground)
                        .cornerRadius(8)
                }
            }
            .padding(.horizontal)
            .padding(.vertical, 6)
            .background(cardBackground)
            .cornerRadius(10)
            .padding(.horizontal)
        }
    }
    
    // MARK: - Coordinate Mapping & Math
    private func mapPoint(_ pt: CGPoint, center: CGPoint, radius: CGFloat, zoom: ConstellationZoom) -> CGPoint {
        var x = pt.x
        var y = pt.y
        
        switch zoom {
        case .full:
            break
        case .zoom2x:
            x *= 2.0
            y *= 2.0
        case .q1: // (+, +)
            x = (x - 0.5) * 2.0
            y = (y - 0.5) * 2.0
        case .q2: // (-, +)
            x = (x + 0.5) * 2.0
            y = (y - 0.5) * 2.0
        case .q3: // (-, -)
            x = (x + 0.5) * 2.0
            y = (y + 0.5) * 2.0
        case .q4: // (+, -)
            x = (x - 0.5) * 2.0
            y = (y + 0.5) * 2.0
        }
        
        return CGPoint(
            x: center.x + x * radius,
            y: center.y - y * radius
        )
    }
    
    private func ringSizeForModulation() -> CGFloat {
        switch session.constellation.modulation {
        case .qpsk: return 22
        case .psk8: return 16
        case .qam16: return 14
        case .qam64: return 8
        case .qam256: return 4
        }
    }
    
    private func ringWidthForModulation() -> CGFloat {
        switch session.constellation.modulation {
        case .qpsk, .psk8: return 1.5
        case .qam16: return 1.2
        case .qam64, .qam256: return 0.8
        }
    }
    
    private func phosphorColor(age: Double) -> Color {
        // age: 0.0 (newest) to 1.0 (oldest)
        if age < 0.25 {
            return Color.yellow.opacity(0.95)
        } else if age < 0.55 {
            return Color.cyan.opacity(0.85)
        } else if age < 0.85 {
            return Color(red: 0.1, green: 0.6, blue: 0.9).opacity(0.65)
        } else {
            return Color.blue.opacity(0.35)
        }
    }
    
    private func merColor(_ mer: Float) -> Color {
        if mer >= 28.0 {
            return .green
        } else if mer >= 22.0 {
            return .blue
        } else if mer >= 16.0 {
            return .orange
        } else {
            return .red
        }
    }
}
