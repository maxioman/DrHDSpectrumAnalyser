//
//  HardwareStatusBar.swift
//  DrHD500
//
//  Status bar displaying battery, LNB current drain, and connection status
//

import SwiftUI
#if canImport(UIKit)
import UIKit
#endif

public struct HardwareStatusBar: View {
    @ObservedObject var session: DeviceSession
    
    private var batteryIcon: String {
        let pct = session.hardwareTelemetry.batteryPercent
        if session.hardwareTelemetry.isCharging {
            return "battery.100.bolt"
        }
        if pct >= 80 { return "battery.100" }
        if pct >= 50 { return "battery.75" }
        if pct >= 25 { return "battery.50" }
        return "battery.25"
    }
    
    private var isLnbOverloaded: Bool {
        return session.hardwareTelemetry.lnbCurrentMilliAmps > 400
    }
    
    public var body: some View {
        HStack(spacing: 12) {
            // Connection Pill
            HStack(spacing: 6) {
                Circle()
                    .fill(connectionColor)
                    .frame(width: 8, height: 8)
                
                Text(connectionLabel)
                    .font(.caption2)
                    .fontWeight(.semibold)
                    .lineLimit(1)
            }
            .padding(.horizontal, 8)
            .padding(.vertical, 4)
            .background(Color.secondary.opacity(0.12))
            .cornerRadius(12)
            
            Spacer()
            
            // LNB Power & Current Draw
            HStack(spacing: 4) {
                Image(systemName: "bolt.fill")
                    .font(.caption2)
                    .foregroundColor(isLnbOverloaded ? .red : .yellow)
                Text(session.hardwareTelemetry.lnbCurrentString)
                    .font(.caption2)
                    .fontWeight(.medium)
                    .foregroundColor(isLnbOverloaded ? .red : .primary)
            }
            .padding(.horizontal, 8)
            .padding(.vertical, 4)
            .background(Color.secondary.opacity(0.12))
            .cornerRadius(12)
            
            // Battery Level
            HStack(spacing: 4) {
                Image(systemName: batteryIcon)
                    .font(.caption2)
                    .foregroundColor(session.hardwareTelemetry.isCharging ? .green : .primary)
                Text("\(session.hardwareTelemetry.batteryPercent)%")
                    .font(.caption2)
                    .fontWeight(.medium)
            }
            .padding(.horizontal, 8)
            .padding(.vertical, 4)
            .background(Color.secondary.opacity(0.12))
            .cornerRadius(12)
        }
        .padding(.horizontal)
        .padding(.vertical, 6)
        #if os(iOS)
        .background(Color(uiColor: .secondarySystemBackground))
        #else
        .background(Color.secondary.opacity(0.1))
        #endif
    }
    
    private var connectionColor: Color {
        switch session.connectionState {
        case .connected: return .green
        case .connecting, .scanning: return .orange
        case .disconnected, .failed: return .red
        }
    }
    
    private var connectionLabel: String {
        switch session.connectionState {
        case .connected(let name): return name
        case .connecting(let name): return "Connecting \(name)..."
        case .scanning: return "Scanning..."
        case .disconnected: return "Disconnected"
        case .failed(let err): return "Failed: \(err)"
        }
    }
}
