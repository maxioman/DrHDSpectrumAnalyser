//
//  ContentView.swift
//  DrHD500
//
//  Root navigation and view container for DrHD 500
//

import SwiftUI
#if canImport(UIKit)
import UIKit
#endif

public struct ContentView: View {
    @StateObject private var session = DeviceSession()
    @State private var showSettings = false
    
    public var body: some View {
        NavigationView {
            VStack(spacing: 0) {
                // Top Battery, Current & Status Bar
                HardwareStatusBar(session: session)
                
                // Mode Switcher (Measures / Spectrum / Constellation)
                Picker("Mode", selection: $session.currentMode) {
                    Text("Measures").tag(AppMode.measures)
                    Text("Spectrum").tag(AppMode.spectrum)
                    Text("Constellation").tag(AppMode.constellation)
                }
                .pickerStyle(.segmented)
                .padding(.horizontal)
                .padding(.vertical, 8)
                #if os(iOS)
                .background(Color(uiColor: .systemBackground))
                #else
                .background(Color(nsColor: .windowBackgroundColor))
                #endif
                
                // Content Views
                Group {
                    switch session.currentMode {
                    case .measures:
                        SignalGaugesView(session: session)
                    case .spectrum:
                        SpectrumChartView(session: session)
                    case .constellation:
                        ConstellationScatterView(session: session)
                    }
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            }
            .navigationTitle("Dr.HD Sat Finder 500")
            #if os(iOS)
            .navigationBarTitleDisplayMode(.inline)
            #endif
            .toolbar {
                ToolbarItem(placement: .automatic) {
                    Button(action: { showSettings = true }) {
                        Image(systemName: "gearshape.fill")
                    }
                }
            }
            .sheet(isPresented: $showSettings) {
                ConnectionSettingsSheet(session: session)
            }
            .onAppear {
                session.connect()
            }
        }
        #if os(iOS)
        .navigationViewStyle(.stack)
        #endif
    }
}
