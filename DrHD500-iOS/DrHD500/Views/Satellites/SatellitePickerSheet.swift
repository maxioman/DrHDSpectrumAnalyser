//
//  SatellitePickerSheet.swift
//  DrHD500
//
//  Searchable satellite & transponder picker
//

import SwiftUI

public struct SatellitePickerSheet: View {
    @ObservedObject var session: DeviceSession
    @Environment(\.dismiss) private var dismiss
    @State private var searchText = ""
    @State private var expandedSatelliteId: String?
    
    private var filteredSatellites: [Satellite] {
        let all = SatelliteDatabase.shared.satellites
        if searchText.trimmingCharacters(in: .whitespaces).isEmpty {
            return all
        }
        return all.filter { sat in
            sat.name.localizedCaseInsensitiveContains(searchText) ||
            sat.formattedPosition.localizedCaseInsensitiveContains(searchText)
        }
    }
    
    public var body: some View {
        NavigationView {
            List {
                ForEach(filteredSatellites) { sat in
                    Section(header: satelliteHeader(sat)) {
                        if expandedSatelliteId == sat.id {
                            ForEach(sat.transponders) { tp in
                                Button(action: {
                                    session.selectedSatellite = sat
                                    session.tuneTransponder(tp)
                                    dismiss()
                                }) {
                                    HStack {
                                        VStack(alignment: .leading, spacing: 2) {
                                            Text(String(format: "%.0f MHz %@", tp.freqMHz, tp.polarization.label))
                                                .font(.headline)
                                                .foregroundColor(.primary)
                                            Text("SR: \(tp.symbolRateKSps) kS/s • \(tp.system.label)")
                                                .font(.caption)
                                                .foregroundColor(.secondary)
                                        }
                                        Spacer()
                                        if session.selectedTransponder?.id == tp.id {
                                            Image(systemName: "checkmark.circle.fill")
                                                .foregroundColor(.accentColor)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            .searchable(text: $searchText, prompt: "Search satellite or orbital degree...")
            .navigationTitle("Select Satellite")
            #if os(iOS)
            .navigationBarTitleDisplayMode(.inline)
            #endif
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Close") { dismiss() }
                }
            }
        }
    }
    
    private func satelliteHeader(_ sat: Satellite) -> some View {
        Button(action: {
            if expandedSatelliteId == sat.id {
                expandedSatelliteId = nil
            } else {
                expandedSatelliteId = sat.id
            }
        }) {
            HStack {
                Text(sat.name)
                    .font(.headline)
                    .foregroundColor(.primary)
                Spacer()
                Text(sat.formattedPosition)
                    .font(.subheadline)
                    .foregroundColor(.secondary)
                Image(systemName: expandedSatelliteId == sat.id ? "chevron.up" : "chevron.down")
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
            .contentShape(Rectangle())
        }
    }
}
