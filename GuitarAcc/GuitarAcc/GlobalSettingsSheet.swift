import SwiftUI

struct GlobalSettingsSheet: View {
    @EnvironmentObject var serial: USBSerialManager
    @Environment(\.dismiss) private var dismiss

    // Local edit state
    @State private var midiChannel: Int = 1
    @State private var bleScanInterval: Int = 100
    @State private var runningAvgEnable: Bool = true
    @State private var runningAvgDepth: Int = 5
    @State private var accelScale: [Int] = Array(repeating: 1000, count: 6)
    @State private var accelOffset: [Int] = Array(repeating: 0, count: 6)

    @State private var isSaving = false
    @State private var saveError: String?

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Text("Global Settings").font(.title2).bold()
                Spacer()
                Button("Done") { dismiss() }
            }
            Divider()

            if !serial.isConnected {
                Text("Not connected.").foregroundStyle(.secondary)
            } else {
                Form {
                    Section("MIDI") {
                        Stepper("Channel: \(midiChannel)", value: $midiChannel, in: 1...16)
                    }
                    Section("BLE") {
                        Stepper("Scan interval: \(bleScanInterval) ms",
                                value: $bleScanInterval, in: 10...1000, step: 10)
                    }
                    Section("Filter") {
                        Toggle("Running average", isOn: $runningAvgEnable)
                        Stepper("Depth: \(runningAvgDepth) samples",
                                value: $runningAvgDepth, in: 3...10)
                            .disabled(!runningAvgEnable)
                    }
                    Section("Accelerometer Scale (milli-g)") {
                        ForEach(AccelAxis.allCases, id: \.rawValue) { axis in
                            Stepper("\(axis.label): \(accelScale[axis.rawValue]) mg",
                                    value: $accelScale[axis.rawValue], in: 100...4000, step: 100)
                        }
                    }
                    Section("Accelerometer Offset (milli-g)") {
                        ForEach(AccelAxis.allCases, id: \.rawValue) { axis in
                            Stepper("\(axis.label): \(accelOffset[axis.rawValue]) mg",
                                    value: $accelOffset[axis.rawValue], in: -2000...2000, step: 10)
                        }
                    }
                }

                if let err = saveError {
                    Text(err).foregroundStyle(.red).font(.caption)
                }

                HStack {
                    Spacer()
                    Button("Apply & Save") { Task { await apply() } }
                        .buttonStyle(.borderedProminent)
                        .disabled(isSaving)
                }
            }
        }
        .padding()
        .frame(minWidth: 420, minHeight: 500)
        .onAppear { seedFromDevice() }
        .onChange(of: serial.currentGlobalConfig) { seedFromDevice() }
    }

    private func seedFromDevice() {
        guard let g = serial.currentGlobalConfig else { return }
        midiChannel = g.midiChannel + 1       // firmware stores 0-based
        bleScanInterval = g.bleScanIntervalMs
        runningAvgEnable = g.runningAverageEnable
        runningAvgDepth = g.runningAverageDepth
        if g.accelScale.count == 6 { accelScale = g.accelScale }
        if g.accelOffset.count == 6 { accelOffset = g.accelOffset }
    }

    private func apply() async {
        isSaving = true
        saveError = nil
        for (axis, value) in accelScale.enumerated() {
            _ = await serial.runCommandCollectingOutput("config accel_scale \(axis) \(value)",
                                                        perLineTimeout: 0.6, maxLines: 10)
        }
        for (axis, value) in accelOffset.enumerated() {
            _ = await serial.runCommandCollectingOutput("config accel_offset \(axis) \(value)",
                                                        perLineTimeout: 0.6, maxLines: 10)
        }
        _ = await serial.runCommandCollectingOutput("config midi_ch \(midiChannel)",
                                                    perLineTimeout: 0.5, maxLines: 10)
        _ = await serial.runCommandCollectingOutput("config scan_interval \(bleScanInterval)",
                                                    perLineTimeout: 0.5, maxLines: 10)
        _ = await serial.runCommandCollectingOutput("config avg_enable \(runningAvgEnable ? 1 : 0)",
                                                    perLineTimeout: 0.5, maxLines: 10)
        _ = await serial.runCommandCollectingOutput("config avg_depth \(runningAvgDepth)",
                                                    perLineTimeout: 0.5, maxLines: 10)
        _ = await serial.runCommandCollectingOutput("config save", perLineTimeout: 1.0, maxLines: 10)
        await serial.loadGlobalConfig()
        isSaving = false
    }
}
