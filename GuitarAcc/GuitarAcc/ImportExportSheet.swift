import SwiftUI

struct ImportSheet: View {
    @EnvironmentObject var serial: USBSerialManager
    @Environment(\.dismiss) private var dismiss
    @State private var jsonText = ""
    @State private var status = ""
    @State private var isWorking = false

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Text("Import Configuration").font(.title2).bold()
                Spacer()
                Button("Done") { dismiss() }
            }
            Divider()
            Text("Paste JSON configuration below. The device will validate and save it automatically.")
                .foregroundStyle(.secondary).font(.caption)

            TextEditor(text: $jsonText)
                .font(.system(.body, design: .monospaced))
                .frame(minHeight: 250)
                .border(Color.secondary.opacity(0.3))

            if !status.isEmpty {
                Text(status).font(.caption).foregroundStyle(.secondary)
            }

            HStack {
                Spacer()
                Button("Import") { Task { await runImport() } }
                    .buttonStyle(.borderedProminent)
                    .disabled(jsonText.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty || !serial.isConnected || isWorking)
            }
        }
        .padding()
        .frame(minWidth: 500, minHeight: 400)
    }

    private func runImport() async {
        isWorking = true
        status = "Sending config import…"
        serial.sendCommand("config import")
        try? await Task.sleep(nanoseconds: 300_000_000)

        // Send JSON line by line, then Ctrl-D terminator
        for line in jsonText.components(separatedBy: .newlines) {
            serial.sendCommand(line)
        }
        serial.sendCommand("\u{04}")   // Ctrl-D — Zephyr shell import terminator
        try? await Task.sleep(nanoseconds: 500_000_000)
        status = "Import sent. Check terminal for device response."
        isWorking = false
    }
}

struct ExportSheet: View {
    @EnvironmentObject var serial: USBSerialManager
    @Environment(\.dismiss) private var dismiss
    @State private var exportText = ""
    @State private var isWorking = false

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Text("Export Configuration").font(.title2).bold()
                Spacer()
                Button("Done") { dismiss() }
            }
            Divider()

            if exportText.isEmpty {
                Text("Press Export to fetch the full configuration from the device.")
                    .foregroundStyle(.secondary).font(.caption)
            }

            ScrollView {
                Text(exportText.isEmpty ? "No data yet." : exportText)
                    .font(.system(.body, design: .monospaced))
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(4)
            }
            .frame(minHeight: 250)
            .border(Color.secondary.opacity(0.3))

            HStack {
                Button("Export All") { Task { await fetchExport("config export") } }
                Button("Export Global") { Task { await fetchExport("config export global") } }
                Button("Export Patch \(serial.currentPatchIndex)") {
                    Task { await fetchExport("config export patch \(serial.currentPatchIndex)") }
                }
                Spacer()
                if !exportText.isEmpty {
                    Button("Copy") { NSPasteboard.general.clearContents(); NSPasteboard.general.setString(exportText, forType: .string) }
                }
            }
            .disabled(!serial.isConnected || isWorking)
        }
        .padding()
        .frame(minWidth: 500, minHeight: 400)
    }

    private func fetchExport(_ command: String) async {
        isWorking = true
        exportText = await serial.runCommandCollectingOutput(command, perLineTimeout: 0.8, maxLines: 2000)
        isWorking = false
    }
}
