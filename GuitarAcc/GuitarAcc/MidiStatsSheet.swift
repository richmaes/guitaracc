import SwiftUI

struct MidiStatsSheet: View {
    @EnvironmentObject var serial: USBSerialManager
    @Environment(\.dismiss) private var dismiss
    @State private var stats: MidiRxStats?

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Text("MIDI Statistics").font(.title2).bold()
                Spacer()
                Button("Done") { dismiss() }
            }
            Divider()

            if !serial.isConnected {
                Text("Not connected.").foregroundStyle(.secondary)
            } else if let s = stats {
                Grid(alignment: .leading, horizontalSpacing: 20, verticalSpacing: 6) {
                    statsRow("Total bytes", "\(s.totalBytes)")
                    statsRow("Clock (0xF8)", "\(s.clockMessages)")
                    if let bpm = s.estimatedBpm { statsRow("BPM", "~\(bpm)") }
                    statsRow("Start (0xFA)", "\(s.startMessages)")
                    statsRow("Continue (0xFB)", "\(s.continueMessages)")
                    statsRow("Stop (0xFC)", "\(s.stopMessages)")
                    statsRow("Other", "\(s.otherMessages)")
                }
            } else {
                Text("Press Refresh.").foregroundStyle(.secondary)
            }

            Spacer()

            HStack {
                Button("Reset") {
                    serial.resetMidiRxStats()
                    Task { stats = await serial.loadMidiRxStats() }
                }
                Spacer()
                Button("Refresh") { Task { stats = await serial.loadMidiRxStats() } }
                    .buttonStyle(.borderedProminent)
            }
            .disabled(!serial.isConnected)
        }
        .padding()
        .frame(minWidth: 320, minHeight: 280)
        .task { stats = await serial.loadMidiRxStats() }
    }

    private func statsRow(_ label: String, _ value: String) -> some View {
        GridRow {
            Text(label).foregroundStyle(.secondary)
            Text(value).monospacedDigit()
        }
    }
}
