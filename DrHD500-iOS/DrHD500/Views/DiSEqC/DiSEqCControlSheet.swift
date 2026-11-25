//
//  DiSEqCControlSheet.swift
//  DrHD500
//
//  Controls for DiSEqC 1.0/1.1 switches, 1.2 motors, and USALS auto-calculation
//

import SwiftUI

public struct DiSEqCControlSheet: View {
    @ObservedObject var session: DeviceSession
    @Environment(\.dismiss) private var dismiss
    
    @State private var selectedTab: Int = 0
    @State private var localLatitude: Double = 25.2048
    @State private var localLongitude: Double = 55.2708
    @State private var motorPosition: Int = 1
    
    public var body: some View {
        NavigationView {
            VStack(spacing: 0) {
                Picker("DiSEqC Mode", selection: $selectedTab) {
                    Text("1.0 Switch").tag(0)
                    Text("1.1 Switch").tag(1)
                    Text("1.2 Motor").tag(2)
                    Text("USALS").tag(3)
                }
                .pickerStyle(.segmented)
                .padding()
                
                Form {
                    if selectedTab == 0 {
                        Section(header: Text("DiSEqC 1.0 (4 Ports)")) {
                            ForEach(DiSEqCCommands.diseqc10Ports) { port in
                                Button(action: {
                                    session.sendDiSEqCCommand(port.command)
                                }) {
                                    HStack {
                                        Text(port.name)
                                            .foregroundColor(.primary)
                                        Spacer()
                                        Text(port.command)
                                            .font(.caption)
                                            .foregroundColor(.secondary)
                                    }
                                }
                            }
                        }
                    } else if selectedTab == 1 {
                        Section(header: Text("DiSEqC 1.1 (16 Ports)")) {
                            ForEach(DiSEqCCommands.diseqc11Ports) { port in
                                Button(action: {
                                    session.sendDiSEqCCommand(port.command)
                                }) {
                                    HStack {
                                        Text(port.name)
                                            .foregroundColor(.primary)
                                        Spacer()
                                        Text(port.command)
                                            .font(.caption)
                                            .foregroundColor(.secondary)
                                    }
                                }
                            }
                        }
                    } else if selectedTab == 2 {
                        Section(header: Text("DiSEqC 1.2 Motor Drive")) {
                            HStack(spacing: 12) {
                                Button("Drive East") {
                                    session.sendDiSEqCCommand(DiSEqCCommands.motorDriveEast)
                                }
                                .buttonStyle(.borderedProminent)
                                .tint(.blue)
                                
                                Button("STOP") {
                                    session.sendDiSEqCCommand(DiSEqCCommands.motorStop)
                                }
                                .buttonStyle(.borderedProminent)
                                .tint(.red)
                                
                                Button("Drive West") {
                                    session.sendDiSEqCCommand(DiSEqCCommands.motorDriveWest)
                                }
                                .buttonStyle(.borderedProminent)
                                .tint(.blue)
                            }
                            .frame(maxWidth: .infinity, alignment: .center)
                            .padding(.vertical, 4)
                            
                            HStack(spacing: 16) {
                                Button("Step East (1°)") {
                                    session.sendDiSEqCCommand(DiSEqCCommands.motorStepEast)
                                }
                                .buttonStyle(.bordered)
                                
                                Button("Step West (1°)") {
                                    session.sendDiSEqCCommand(DiSEqCCommands.motorStepWest)
                                }
                                .buttonStyle(.bordered)
                            }
                            .frame(maxWidth: .infinity, alignment: .center)
                        }
                        
                        Section(header: Text("Stored Satellite Positions")) {
                            Stepper("Position: \(motorPosition)", value: $motorPosition, in: 1...32)
                            
                            HStack {
                                Button("Goto Position \(motorPosition)") {
                                    session.sendDiSEqCCommand(DiSEqCCommands.motorGotoPosition(number: motorPosition))
                                }
                                .buttonStyle(.borderedProminent)
                                
                                Spacer()
                                
                                Button("Store Position") {
                                    session.sendDiSEqCCommand(DiSEqCCommands.motorStorePosition(number: motorPosition))
                                }
                                .buttonStyle(.bordered)
                            }
                        }
                    } else if selectedTab == 3 {
                        Section(header: Text("Local Coordinates for USALS")) {
                            HStack {
                                Text("Latitude:")
                                Spacer()
                                #if os(iOS)
                                TextField("Latitude", value: $localLatitude, format: .number)
                                    .keyboardType(.decimalPad)
                                    .multilineTextAlignment(.trailing)
                                #else
                                TextField("Latitude", value: $localLatitude, format: .number)
                                    .multilineTextAlignment(.trailing)
                                #endif
                                Text("°")
                            }
                            HStack {
                                Text("Longitude:")
                                Spacer()
                                #if os(iOS)
                                TextField("Longitude", value: $localLongitude, format: .number)
                                    .keyboardType(.decimalPad)
                                    .multilineTextAlignment(.trailing)
                                #else
                                TextField("Longitude", value: $localLongitude, format: .number)
                                    .multilineTextAlignment(.trailing)
                                #endif
                                Text("°")
                            }
                        }
                        
                        Section(header: Text("Target Satellite")) {
                            if let sat = session.selectedSatellite, let posDeg = Double(sat.position) {
                                let degreeVal = posDeg / 10.0
                                Text("Target: \(sat.name) (\(sat.formattedPosition))")
                                    .font(.headline)
                                
                                Button("Send USALS Goto X") {
                                    let cmd = DiSEqCCommands.usalsGotoX(
                                        localLat: localLatitude,
                                        localLong: localLongitude,
                                        satPosDegrees: degreeVal
                                    )
                                    session.sendDiSEqCCommand(cmd)
                                }
                                .buttonStyle(.borderedProminent)
                                .tint(.green)
                            } else {
                                Text("Please select a satellite first.")
                                    .foregroundColor(.secondary)
                            }
                        }
                    }
                }
            }
            .navigationTitle("DiSEqC & Motor Controls")
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
