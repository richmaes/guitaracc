import SwiftUI

struct PatchView: View {
    @EnvironmentObject var serial: USBSerialManager

    @State private var showStatus = false
    @State private var showSettings = false
    @State private var showMidiStats = false
    @State private var showImport = false
    @State private var showExport = false
    @State private var showTerminal = false

    var body: some View {
        VStack(spacing: 0) {
            PatchHeaderArea(
                showStatus:    $showStatus,
                showSettings:  $showSettings,
                showMidiStats: $showMidiStats,
                showImport:    $showImport,
                showExport:    $showExport,
                showTerminal:  $showTerminal
            )
            .accessibilityIdentifier("PatchHeaderArea")

            Divider()

            PatchSelectionArea()
                .accessibilityIdentifier("PatchSelectionArea")

            Divider()

            ScrollView(.vertical) {
                ControlPanelArea()
                    .accessibilityIdentifier("ControlPanelArea")
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(.horizontal)
            }
            .scrollIndicators(.visible)
        }
        .sheet(isPresented: $showStatus)    { StatusSheet() }
        .sheet(isPresented: $showSettings)  { GlobalSettingsSheet() }
        .sheet(isPresented: $showMidiStats) { MidiStatsSheet() }
        .sheet(isPresented: $showImport)    { ImportSheet() }
        .sheet(isPresented: $showExport)    { ExportSheet() }
        .sheet(isPresented: $showTerminal)  { TerminalView() }
        .task { await serial.autoConnectCLI() }
    }
}
