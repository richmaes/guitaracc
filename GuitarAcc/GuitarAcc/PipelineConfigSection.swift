import SwiftUI

/// Pipeline configuration editor with a side-by-side live monitor panel.
struct PipelineConfigSection: View {
    @EnvironmentObject var serial: USBSerialManager

    @State private var rho: Double = 0
    @State private var theta: Double = 0
    @State private var midiCC: Int = 1
    @State private var funcTypeIndex: Int = 0

    @State private var linearScale: Double = 1.0
    @State private var linearOffset: Double = 0.0
    @State private var expExponent: Double = 1.0
    @State private var scurveSteepness: Double = 5.0
    @State private var lookupValues: [Int] = [0, 32, 64, 96, 127]

    @State private var isSaving = false
    @State private var saveError: String?

    private let funcTypes = ["linear", "exponential", "scurve", "lookup"]

    var body: some View {
        GroupBox {
            if !serial.isConnected {
                Text("Not connected").foregroundStyle(.secondary).padding(8)
            } else {
                HStack(alignment: .top, spacing: 20) {
                    settingsPanel
                    Divider()
                    monitorPanel
                }
                .padding(8)
            }
        } label: {
            HStack {
                Label("Pipeline", systemImage: "dial.medium")
                Spacer()
                monitorToggle
            }
        }
        .onAppear { seedFromDevice() }
        .onChange(of: serial.currentPipelineConfig) { seedFromDevice() }
    }

    // MARK: - Monitor Toggle

    private var monitorToggle: some View {
        Button {
            if serial.isMonitoring { serial.stopMonitoring() }
            else { serial.startMonitoring() }
        } label: {
            Label(
                serial.isMonitoring ? "Stop Monitor" : "Monitor",
                systemImage: serial.isMonitoring ? "stop.circle.fill" : "waveform"
            )
        }
        .buttonStyle(.bordered)
        .tint(serial.isMonitoring ? .red : .accentColor)
        .controlSize(.small)
        .disabled(!serial.isConnected)
    }

    // MARK: - Settings Panel (left)

    private var settingsPanel: some View {
        VStack(alignment: .leading, spacing: 14) {
            rotationSection
            Divider()
            outputSection
            Divider()
            conversionSection
            Divider()
            applyRow
        }
        .frame(minWidth: 300)
    }

    private var rotationSection: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Rotation").font(.headline)
            HStack(alignment: .center, spacing: 16) {
                OrientationSphereView(rho: $rho, theta: $theta,
                                      snapshot: serial.monitorSnapshot)
                VStack(spacing: 8) {
                    LabeledControl("Rho (°)") {
                        HStack {
                            Slider(value: $rho, in: 0...360, step: 1)
                            Text(String(format: "%.0f°", rho)).frame(width: 40, alignment: .trailing).monospacedDigit()
                        }
                    }
                    LabeledControl("Theta (°)") {
                        HStack {
                            Slider(value: $theta, in: 0...360, step: 1)
                            Text(String(format: "%.0f°", theta)).frame(width: 40, alignment: .trailing).monospacedDigit()
                        }
                    }
                }
            }
        }
    }

    private var outputSection: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Output").font(.headline)
            LabeledControl("MIDI CC") {
                HStack {
                    Slider(value: Binding(get: { Double(midiCC) }, set: { midiCC = Int($0) }), in: 0...127, step: 1)
                    Text("\(midiCC)").frame(width: 32, alignment: .trailing).monospacedDigit()
                }
            }
        }
    }

    private var conversionSection: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Conversion").font(.headline)
            Picker("Function", selection: $funcTypeIndex) {
                ForEach(funcTypes.indices, id: \.self) { i in Text(funcTypes[i]).tag(i) }
            }
            .pickerStyle(.segmented)

            switch funcTypes[funcTypeIndex] {
            case "linear":
                LabeledControl("Scale") {
                    HStack {
                        Slider(value: $linearScale, in: -10...10)
                        Text(String(format: "%.2f", linearScale)).frame(width: 44, alignment: .trailing).monospacedDigit()
                    }
                }
                LabeledControl("Offset") {
                    HStack {
                        Slider(value: $linearOffset, in: -1...1)
                        Text(String(format: "%.2f", linearOffset)).frame(width: 44, alignment: .trailing).monospacedDigit()
                    }
                }
            case "exponential":
                LabeledControl("Exponent") {
                    HStack {
                        Slider(value: $expExponent, in: 0.1...5.0)
                        Text(String(format: "%.2f", expExponent)).frame(width: 44, alignment: .trailing).monospacedDigit()
                    }
                }
            case "scurve":
                LabeledControl("Steepness") {
                    HStack {
                        Slider(value: $scurveSteepness, in: 1.0...20.0)
                        Text(String(format: "%.1f", scurveSteepness)).frame(width: 44, alignment: .trailing).monospacedDigit()
                    }
                }
            case "lookup":
                ForEach(0..<5, id: \.self) { i in
                    LabeledControl("v\(i)") {
                        HStack {
                            Slider(value: Binding(
                                get: { Double(lookupValues[i]) },
                                set: { lookupValues[i] = Int($0) }
                            ), in: 0...127, step: 1)
                            Text("\(lookupValues[i])").frame(width: 32, alignment: .trailing).monospacedDigit()
                        }
                    }
                }
            default:
                EmptyView()
            }
        }
    }

    private var applyRow: some View {
        VStack(alignment: .leading, spacing: 4) {
            Button(action: applyPipeline) {
                if isSaving {
                    ProgressView().controlSize(.small)
                } else {
                    Label("Apply & Save", systemImage: "checkmark.circle")
                }
            }
            .buttonStyle(.borderedProminent)
            .disabled(isSaving || !serial.isConnected)

            if let err = saveError {
                Text(err).foregroundStyle(.red).font(.caption)
            }
        }
    }

    // MARK: - Monitor Panel (right)

    private var monitorPanel: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Live State").font(.headline)

            if serial.isMonitoring, let snap = serial.monitorSnapshot {
                liveValues(snap)
            } else {
                disabledValues
            }
        }
        .frame(minWidth: 200)
    }

    @ViewBuilder
    private func liveValues(_ snap: MonitorSnapshot) -> some View {
        // Raw accelerometer
        Group {
            statRow("Accel X", "\(snap.rawAxis.x) mg")
            statRow("Accel Y", "\(snap.rawAxis.y) mg")
            statRow("Accel Z", "\(snap.rawAxis.z) mg")
        }

        Divider()

        // Scalar projection
        statRow("Projection", String(format: "%.4f", snap.scalarProjection))
        // Bipolar bar: maps -1…+1 to 0…1 for the progress view
        Gauge(value: (snap.scalarProjection + 1.0) / 2.0) {
            EmptyView()
        } currentValueLabel: {
            EmptyView()
        }
        .gaugeStyle(.linearCapacity)
        .tint(Gradient(colors: [.blue, .green, .orange]))
        .frame(width: 160)

        Divider()

        // MIDI output
        statRow("Function", snap.functionType)
        statRow("MIDI CC", "\(snap.midiOutput.cc)")
        statRow("MIDI Value", "\(snap.midiOutput.value)")
        Gauge(value: Double(snap.midiOutput.value) / 127.0) {
            EmptyView()
        } currentValueLabel: {
            EmptyView()
        }
        .gaugeStyle(.linearCapacity)
        .tint(Gradient(colors: [.purple, .cyan]))
        .frame(width: 160)
    }

    private var disabledValues: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Enable monitor to view pipeline state")
                .font(.caption)
                .foregroundStyle(.secondary)
                .fixedSize(horizontal: false, vertical: true)

            Group {
                statRow("Accel X",    "NaN")
                statRow("Accel Y",    "NaN")
                statRow("Accel Z",    "NaN")
                statRow("Projection", "NaN")
                statRow("MIDI CC",    "NaN")
                statRow("MIDI Value", "NaN")
            }
            .foregroundStyle(.secondary)
        }
    }

    private func statRow(_ label: String, _ value: String) -> some View {
        HStack(spacing: 4) {
            Text(label)
                .font(.caption)
                .foregroundStyle(.secondary)
                .frame(width: 90, alignment: .trailing)
            Text(value)
                .font(.system(.caption, design: .monospaced))
                .frame(minWidth: 60, alignment: .leading)
        }
    }

    // MARK: - Logic

    private func seedFromDevice() {
        guard let cfg = serial.currentPipelineConfig else { return }
        rho = cfg.rhoDegrees
        theta = cfg.thetaDegrees
        midiCC = cfg.midiCC
        switch cfg.conversion {
        case .linear(let s, let o):
            funcTypeIndex = 0; linearScale = s; linearOffset = o
        case .exponential(let e):
            funcTypeIndex = 1; expExponent = e
        case .scurve(let st):
            funcTypeIndex = 2; scurveSteepness = st
        case .lookup(let vals):
            funcTypeIndex = 3
            lookupValues = Array((vals + [0, 0, 0, 0, 0]).prefix(5))
        }
    }

    private func applyPipeline() {
        let conversion: ConversionFunction
        switch funcTypes[funcTypeIndex] {
        case "exponential": conversion = .exponential(exponent: expExponent)
        case "scurve":      conversion = .scurve(steepness: scurveSteepness)
        case "lookup":      conversion = .lookup(values: lookupValues)
        default:            conversion = .linear(scale: linearScale, offset: linearOffset)
        }
        let config = PipelineConfig(
            patch: serial.currentPatchIndex,
            rhoDegrees: rho, thetaDegrees: theta,
            midiCC: midiCC, conversion: conversion
        )
        isSaving = true
        saveError = nil
        Task {
            let ok = await serial.applyPipelineConfig(config)
            isSaving = false
            if !ok { saveError = "Apply failed — check terminal log." }
        }
    }
}

// MARK: - Helpers

private struct LabeledControl<Content: View>: View {
    let label: String
    let content: Content

    init(_ label: String, @ViewBuilder content: () -> Content) {
        self.label = label
        self.content = content()
    }

    var body: some View {
        HStack {
            Text(label)
                .frame(width: 80, alignment: .trailing)
                .foregroundStyle(.secondary)
                .font(.caption)
            content
        }
    }
}
