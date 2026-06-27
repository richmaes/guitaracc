import SwiftUI

struct PatchHeaderArea: View {
    @EnvironmentObject var serial: USBSerialManager
    @Binding var showStatus: Bool
    @Binding var showSettings: Bool
    @Binding var showMidiStats: Bool
    @Binding var showImport: Bool
    @Binding var showExport: Bool
    @Binding var showTerminal: Bool

    var body: some View {
        HStack(spacing: 8) {
            // Connection indicator
            Circle()
                .fill(serial.isConnected ? Color.green : Color.red)
                .frame(width: 10, height: 10)
            Text(serial.isConnected ? "Connected" : "Disconnected")
                .font(.caption)
                .foregroundStyle(.secondary)

            if !serial.isConnected {
                Button("Connect") {
                    Task { await serial.autoConnectCLI() }
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.small)
            } else {
                Button("Disconnect") { serial.disconnect() }
                    .controlSize(.small)
            }

            Spacer()

            Divider().frame(height: 20)

            Button { showStatus = true } label: {
                Label("Status", systemImage: "info.circle")
            }
            .accessibilityIdentifier("PatchHeaderArea.StatusButton")

            Button { showSettings = true } label: {
                Label("Settings", systemImage: "slider.horizontal.3")
            }
            .accessibilityIdentifier("PatchHeaderArea.SettingsButton")

            Button { showMidiStats = true } label: {
                Label("MIDI", systemImage: "music.note")
            }
            .accessibilityIdentifier("PatchHeaderArea.MidiStatsButton")

            Divider().frame(height: 20)

            Button { showImport = true } label: {
                Label("Import", systemImage: "arrow.down.doc")
            }
            .accessibilityIdentifier("PatchHeaderArea.ImportButton")

            Button { showExport = true } label: {
                Label("Export", systemImage: "arrow.up.doc")
            }
            .accessibilityIdentifier("PatchHeaderArea.ExportButton")

            Divider().frame(height: 20)

            Button { showTerminal = true } label: {
                Label("Terminal", systemImage: "terminal")
            }
            .accessibilityIdentifier("PatchHeaderArea.TerminalButton")
        }
        .labelStyle(.iconOnly)
        .padding(.horizontal, 12)
        .padding(.vertical, 8)
        .background(.thinMaterial)
    }
}
