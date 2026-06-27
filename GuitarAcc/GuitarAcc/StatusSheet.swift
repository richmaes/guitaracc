import SwiftUI

struct StatusSheet: View {
    @EnvironmentObject var serial: USBSerialManager
    @Environment(\.dismiss) private var dismiss
    @State private var statusText = "Press Refresh to query device."

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Text("Status").font(.title2).bold()
                Spacer()
                Button("Done") { dismiss() }
            }
            Divider()

            if !serial.isConnected {
                Text("Not connected.").foregroundStyle(.secondary)
            } else {
                ScrollView {
                    Text(statusText)
                        .font(.system(.body, design: .monospaced))
                        .frame(maxWidth: .infinity, alignment: .leading)
                }
                .frame(minHeight: 200)
            }

            HStack {
                Text("FW: \(serial.firmwareVersion.isEmpty ? "—" : serial.firmwareVersion)")
                    .font(.caption).foregroundStyle(.secondary)
                Spacer()
                Button("Refresh") { Task { await refresh() } }
                    .disabled(!serial.isConnected)
            }
        }
        .padding()
        .frame(minWidth: 400, minHeight: 300)
        .task { await refresh() }
    }

    private func refresh() async {
        guard serial.isConnected else { return }
        statusText = await serial.runCommandCollectingOutput("status", perLineTimeout: 0.6, maxLines: 50)
    }
}
