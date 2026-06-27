import SwiftUI

struct TerminalView: View {
    @EnvironmentObject var serial: USBSerialManager
    @Environment(\.dismiss) private var dismiss
    @State private var input = ""
    @State private var selectedTab = 0   // 0 = Serial I/O, 1 = Event Log

    var body: some View {
        VStack(spacing: 0) {
            HStack {
                Text("Terminal").font(.title2).bold()
                Spacer()
                Picker("", selection: $selectedTab) {
                    Text("Serial I/O").tag(0)
                    Text("Event Log").tag(1)
                }
                .pickerStyle(.segmented)
                .frame(width: 200)
                Spacer()
                Button("Clear") {
                    if selectedTab == 0 { serial.clearTerminalOutput() }
                    else { serial.log.removeAll() }
                }
                Button("Done") { dismiss() }
            }
            .padding()

            Divider()

            // Log file path
            HStack(spacing: 6) {
                Image(systemName: "doc.text").foregroundStyle(.secondary)
                Text(DebugLog.fileURL.path)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .textSelection(.enabled)
                Spacer()
                Button {
                    NSWorkspace.shared.selectFile(DebugLog.fileURL.path,
                                                 inFileViewerRootedAtPath: "")
                } label: {
                    Image(systemName: "folder")
                }
                .buttonStyle(.plain)
                .help("Reveal in Finder")
            }
            .padding(.horizontal)
            .padding(.vertical, 4)
            .background(Color.secondary.opacity(0.08))

            Divider()

            if selectedTab == 0 { serialPane } else { eventLogPane }

            Divider()

            if selectedTab == 0 {
                HStack {
                    TextField("Command", text: $input)
                        .font(.system(.body, design: .monospaced))
                        .textFieldStyle(.roundedBorder)
                        .onSubmit { send() }
                        .disabled(!serial.isConnected)
                    Button("Send") { send() }
                        .disabled(input.isEmpty || !serial.isConnected)
                }
                .padding(8)
            }
        }
        .frame(minWidth: 640, minHeight: 440)
    }

    // MARK: - Panes

    private var serialPane: some View {
        ScrollViewReader { proxy in
            ScrollView {
                Text(serial.terminalOutput.isEmpty ? "No serial output yet." : serial.terminalOutput)
                    .font(.system(.body, design: .monospaced))
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(8)
                    .textSelection(.enabled)
                    .id("serialBottom")
            }
            .background(Color.black.opacity(0.85))
            .foregroundStyle(.green)
            .onChange(of: serial.terminalOutput) {
                withAnimation { proxy.scrollTo("serialBottom", anchor: .bottom) }
            }
        }
    }

    private var eventLogPane: some View {
        ScrollViewReader { proxy in
            ScrollView {
                LazyVStack(alignment: .leading, spacing: 2) {
                    ForEach(serial.log.indices, id: \.self) { i in
                        Text(serial.log[i])
                            .font(.system(.caption, design: .monospaced))
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .foregroundStyle(logColor(serial.log[i]))
                    }
                    Color.clear.frame(height: 1).id("logBottom")
                }
                .padding(8)
                .textSelection(.enabled)
            }
            .background(Color.black.opacity(0.85))
            .onChange(of: serial.log.count) {
                withAnimation { proxy.scrollTo("logBottom", anchor: .bottom) }
            }
        }
    }

    // MARK: - Helpers

    private func send() {
        guard !input.isEmpty else { return }
        serial.sendRawCommand(input)
        input = ""
    }

    private func logColor(_ entry: String) -> Color {
        let lower = entry.lowercased()
        if lower.contains("error") || lower.contains("fail") || lower.contains("no cli") { return .red }
        if entry.hasPrefix(">") { return .cyan }
        if lower.contains("connected:") { return .green }
        if lower.contains("probe") || lower.contains("response") { return .yellow }
        return Color.white.opacity(0.85)
    }
}
