//
//  CircularGaugeView.swift
//  DrHD500
//
//  Circular precision gauge for SNR display
//

import SwiftUI

public struct CircularGaugeView: View {
    public let value: Float
    public let maxValue: Float
    public let title: String
    public let unit: String
    public let isLocked: Bool
    
    public init(value: Float, maxValue: Float = 20.0, title: String = "SNR", unit: String = "dB", isLocked: Bool = false) {
        self.value = value
        self.maxValue = maxValue
        self.title = title
        self.unit = unit
        self.isLocked = isLocked
    }
    
    private var progress: Double {
        return Double(max(0, min(1.0, value / maxValue)))
    }
    
    private var gaugeColor: Color {
        if !isLocked { return .orange }
        if value < 8.0 { return .red }
        if value < 11.0 { return .yellow }
        return .green
    }
    
    public var body: some View {
        ZStack {
            // Background track
            Circle()
                .trim(from: 0.15, to: 0.85)
                .stroke(Color.gray.opacity(0.25), style: StrokeStyle(lineWidth: 16, lineCap: .round))
                .rotationEffect(.degrees(90))
            
            // Progress arc
            Circle()
                .trim(from: 0.15, to: 0.15 + (progress * 0.70))
                .stroke(
                    AngularGradient(
                        gradient: Gradient(colors: [.red, .yellow, .green]),
                        center: .center,
                        startAngle: .degrees(144),
                        endAngle: .degrees(396)
                    ),
                    style: StrokeStyle(lineWidth: 16, lineCap: .round)
                )
                .rotationEffect(.degrees(90))
                .animation(.easeOut(duration: 0.2), value: value)
            
            // Inner readouts
            VStack(spacing: 2) {
                Text(title)
                    .font(.caption)
                    .fontWeight(.semibold)
                    .foregroundColor(.secondary)
                
                Text(String(format: "%.1f", max(0, value)))
                    .font(.system(size: 44, weight: .bold, design: .rounded))
                    .foregroundColor(.primary)
                
                HStack(spacing: 4) {
                    Text(unit)
                        .font(.subheadline)
                        .foregroundColor(.secondary)
                    
                    if isLocked {
                        Image(systemName: "checkmark.seal.fill")
                            .foregroundColor(.green)
                            .font(.caption)
                    }
                }
            }
        }
        .frame(width: 190, height: 190)
    }
}

public struct LinearBarGaugeView: View {
    public let title: String
    public let value: Float
    public let minValue: Float
    public let maxValue: Float
    public let unit: String
    public let barColor: Color
    
    public init(title: String, value: Float, minValue: Float = 0, maxValue: Float = 100, unit: String, barColor: Color = .blue) {
        self.title = title
        self.value = value
        self.minValue = minValue
        self.maxValue = maxValue
        self.unit = unit
        self.barColor = barColor
    }
    
    private var progress: Double {
        let clamped = max(minValue, min(maxValue, value))
        return Double((clamped - minValue) / (maxValue - minValue))
    }
    
    public var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Text(title)
                    .font(.caption)
                    .fontWeight(.medium)
                    .foregroundColor(.secondary)
                Spacer()
                Text(String(format: "%.1f %@", value, unit))
                    .font(.system(.subheadline, design: .monospaced, weight: .semibold))
            }
            
            GeometryReader { geo in
                ZStack(alignment: .leading) {
                    RoundedRectangle(cornerRadius: 6)
                        .fill(Color.gray.opacity(0.2))
                        .frame(height: 12)
                    
                    RoundedRectangle(cornerRadius: 6)
                        .fill(barColor)
                        .frame(width: max(0, geo.size.width * CGFloat(progress)), height: 12)
                        .animation(.easeOut(duration: 0.2), value: progress)
                }
            }
            .frame(height: 12)
        }
    }
}
