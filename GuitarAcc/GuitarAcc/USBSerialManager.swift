// USBSerialManager.swift
// Serial port discovery, probing, and CLI communication for the GuitarAcc basestation.

import Foundation
import SwiftUI
import Combine
import ORSSerial

@MainActor
class USBSerialManager: NSObject, ObservableObject, ORSSerialPortDelegate {

    // MARK: - Published State

    @Published var availablePorts: [String] = []
    @Published var connectedPort: ORSSerialPort?
    @Published var isConnected: Bool = false
    @Published var log: [String] = [] {
        didSet { if let last = log.last { DebugLog.write(last) } }
    }

    @Published var patchCount: Int = 0
    @Published var firmwareVersion: String = ""
    @Published var capabilitiesDiscovered: Bool = false

    @Published var currentPatchIndex: Int = 0
    @Published var currentPatchConfig: PatchConfig?
    @Published var currentGlobalConfig: GlobalConfig?
    @Published var currentPipelineConfig: PipelineConfig?

    @Published var isSaving: Bool = false

    @Published var isMonitoring: Bool = false
    @Published var monitorSnapshot: MonitorSnapshot?

    /// Raw terminal output — all bytes received, ANSI stripped, appended in real time.
    @Published var terminalOutput: String = ""

    // MARK: - Private

    private var readBuffer = Data()
    private var pendingReadContinuation: CheckedContinuation<String, Never>?
    private var discoveredCurrentPatch: Int?
    private var monitorTask: Task<Void, Never>?

    private let baudRate: Int = 115200
    private let probeTimeout: TimeInterval = 1.5
    private let cliProbeCommand = "status\n"

    // MARK: - Port Discovery

    func discoverPorts() {
        let fm = FileManager.default
        guard let contents = try? fm.contentsOfDirectory(atPath: "/dev") else { return }
        let cu  = contents.filter { $0.hasPrefix("cu.usbmodem") || $0.hasPrefix("cu.usbserial") || $0.hasPrefix("cu.SLAB_USBtoUART") }.sorted().map { "/dev/" + $0 }
        let tty = contents.filter { $0.hasPrefix("tty.usbmodem") || $0.hasPrefix("tty.usbserial") || $0.hasPrefix("tty.SLAB_USBtoUART") }.sorted().map { "/dev/" + $0 }
        availablePorts = cu + tty
        log.append("Scan: found \(availablePorts.count) port(s): \(availablePorts.joined(separator: ", "))")
    }

    // MARK: - Auto-Connect

    func autoConnectCLI() async {
        log.append("Auto-connect: scanning for CLI port…")
        discoverPorts()
        for portName in availablePorts {
            if await probePort(portName) {
                await runCapabilityDiscovery()
                return
            }
        }
        isConnected = false
        connectedPort = nil
        log.append("No valid CLI port found.")
    }

    func disconnect() {
        connectedPort?.close()
        connectedPort = nil
        isConnected = false
        resetState()
        log.append("Disconnected.")
    }

    // MARK: - Probing

    private func probePort(_ portName: String) async -> Bool {
        guard let port = ORSSerialPort(path: portName) else { return false }
        log.append("Probing: \(portName)")
        port.baudRate = baudRate as NSNumber
        port.usesRTSCTSFlowControl = false
        port.delegate = self
        port.open()
        port.dtr = true

        try? await Task.sleep(nanoseconds: 500_000_000)
        guard port.isOpen else {
            log.append("Failed to open: \(portName)")
            return false
        }

        port.send(cliProbeCommand.data(using: .utf8)!)
        log.append("Probe sent '\(cliProbeCommand.trimmingCharacters(in: .newlines))' to \(portName)")

        // Collect multiple lines until the port goes quiet — the first line is just the
        // command echo; the actual status output and prompt are on subsequent lines.
        var accumulated = ""
        for _ in 0..<30 {
            let chunk = await readLine(from: port, timeout: 0.3)
            if chunk.isEmpty { break }
            accumulated += chunk
        }

        let cleaned = CLIOutputParser.stripANSI(accumulated)
        log.append("Probe response (\(accumulated.utf8.count) bytes): \(cleaned.prefix(300))")

        if cleaned.contains("GuitarAcc") || cleaned.contains("uart:~$") || cleaned.contains("Basestation") {
            connectedPort = port
            isConnected = true
            log.append("Connected: \(portName)")
            return true
        }

        log.append("No CLI match in response from \(portName) — closing.")
        port.close()
        return false
    }

    // MARK: - Low-Level Read

    private func readLine(from port: ORSSerialPort, timeout: TimeInterval) async -> String {
        guard pendingReadContinuation == nil else { return "" }
        return await withCheckedContinuation { (cont: CheckedContinuation<String, Never>) in
            self.pendingReadContinuation = cont
            self.readBuffer.removeAll(keepingCapacity: true)
            DispatchQueue.main.asyncAfter(deadline: .now() + timeout) { [weak self] in
                guard let self else { return }
                if let c = self.pendingReadContinuation {
                    self.pendingReadContinuation = nil
                    let str = String(data: self.readBuffer, encoding: .utf8) ?? ""
                    self.readBuffer.removeAll(keepingCapacity: true)
                    c.resume(returning: str)
                }
            }
        }
    }

    // MARK: - Command I/O

    func sendCommand(_ command: String) {
        guard let port = connectedPort, port.isOpen else { return }
        let cmd = command.hasSuffix("\n") ? command : command + "\n"
        port.send(cmd.data(using: .utf8)!)
        log.append("> \(command.trimmingCharacters(in: .newlines))")
    }

    func runCommandCollectingOutput(_ command: String,
                                    perLineTimeout: TimeInterval = 0.5,
                                    maxLines: Int = 200) async -> String {
        guard let port = connectedPort, port.isOpen else { return "" }
        let cmd = command.hasSuffix("\n") ? command : command + "\n"
        port.send(cmd.data(using: .utf8)!)
        log.append("> \(command.trimmingCharacters(in: .newlines))")

        var collected = ""
        var lineCount = 0
        while lineCount < maxLines {
            let line = await readLine(from: port, timeout: perLineTimeout)
            if line.isEmpty { break }
            collected += line
            lineCount += 1
        }
        return collected
    }

    // MARK: - Capability Discovery

    func runCapabilityDiscovery() async {
        log.append("Discovery: starting…")
        capabilitiesDiscovered = false

        await loadGlobalConfig()

        if firmwareVersion.isEmpty {
            let status = await runCommandCollectingOutput("status", perLineTimeout: 0.6, maxLines: 50)
            if let v = CLIOutputParser.parseStatus(status).firmwareVersion {
                firmwareVersion = v
            }
        }

        if patchCount <= 0 {
            await queryPatchCount()
        }

        let idx: Int
        if let n = discoveredCurrentPatch {
            idx = n
        } else {
            let showOut = await runCommandCollectingOutput("config show", perLineTimeout: 0.6, maxLines: 200)
            idx = CLIOutputParser.extractCurrentPatchIndex(from: showOut) ?? 0
        }
        currentPatchIndex = idx

        await loadPatchConfig(index: idx)
        await loadPipelineConfig()

        capabilitiesDiscovered = true
        log.append("Discovery: complete — patches=\(patchCount), fw=\(firmwareVersion), patch=\(idx)")
    }

    // MARK: - Config Loaders

    func loadGlobalConfig() async {
        log.append("Loading global config…")
        let raw = await runCommandCollectingOutput("config export global", perLineTimeout: 0.8, maxLines: 200)
        guard let export = CLIOutputParser.parseGlobalExport(from: raw) else {
            log.append("Failed to parse global config.")
            return
        }
        currentGlobalConfig = export.global
        if let v = export.firmwareVersion, !v.isEmpty { firmwareVersion = v }
        if let n = export.patchCount, n > 0 { patchCount = n }
        discoveredCurrentPatch = export.currentPatch
        log.append("Global config loaded.")
    }

    func loadPatchConfig(index: Int) async {
        log.append("Loading patch \(index) config…")
        _ = await runCommandCollectingOutput("config select \(index)", perLineTimeout: 0.5, maxLines: 10)
        let raw = await runCommandCollectingOutput("config export patch \(index)", perLineTimeout: 0.8, maxLines: 500)
        guard let config = CLIOutputParser.parsePatchExport(from: raw) else {
            log.append("Failed to parse patch \(index) config.")
            return
        }
        currentPatchConfig = config
        currentPatchIndex = index
        log.append("Patch \(index) loaded: \(config.patchName)")
    }

    func loadPipelineConfig() async {
        log.append("Loading pipeline config…")
        let raw = await runCommandCollectingOutput("pipeline json", perLineTimeout: 0.6, maxLines: 50)
        guard let config = CLIOutputParser.parsePipelineJson(from: raw) else {
            log.append("Failed to parse pipeline config.")
            return
        }
        currentPipelineConfig = config
        log.append("Pipeline loaded: rho=\(config.rhoDegrees)° theta=\(config.thetaDegrees)° cc=\(config.midiCC) fn=\(config.conversion.typeName)")
    }

    // MARK: - Patch Selection

    func selectPatch(_ index: Int) async {
        currentPatchIndex = index
        await loadPatchConfig(index: index)
        await loadPipelineConfig()
    }

    // MARK: - Pipeline Write-back

    func applyPipelineConfig(_ config: PipelineConfig) async -> Bool {
        guard isConnected else { return false }

        let wasMonitoring = isMonitoring
        stopMonitoring()
        // Allow any in-flight monitor poll (max one readLine timeout) to drain before sending commands.
        try? await Task.sleep(for: .milliseconds(200))

        isSaving = true
        defer {
            isSaving = false
            if wasMonitoring { startMonitoring() }
        }

        let cmd = String(format: "pipeline set %.4f %.4f %d %@",
                         config.rhoDegrees, config.thetaDegrees,
                         config.midiCC, config.conversion.cliFragment)
        let out = await runCommandCollectingOutput(cmd, perLineTimeout: 0.6, maxLines: 20)
        let clean = CLIOutputParser.stripANSI(out).lowercased()

        if clean.contains("error") || clean.contains("unknown") || clean.contains("invalid") {
            log.append("Pipeline apply failed: \(clean)")
            return false
        }

        _ = await runCommandCollectingOutput("config save", perLineTimeout: 1.0, maxLines: 10)
        currentPipelineConfig = config
        log.append("Pipeline applied and saved.")
        return true
    }

    // MARK: - Patch Count Query

    private func queryPatchCount() async {
        let output = await runCommandCollectingOutput("config list", perLineTimeout: 0.6, maxLines: 100)
        let count = output.components(separatedBy: .newlines)
            .filter { $0.range(of: #"\bPatch\s+\d+"#, options: .regularExpression) != nil }
            .count
        patchCount = max(count, 0)
        log.append("Patch count: \(patchCount)")
    }

    // MARK: - Monitor

    /// Start polling `monitor json` at ~4 Hz. Skips cycles when a save is in progress.
    func startMonitoring() {
        guard isConnected, !isMonitoring else { return }
        isMonitoring = true
        monitorSnapshot = nil
        log.append("Monitor: started")
        monitorTask = Task {
            while !Task.isCancelled && isMonitoring && isConnected {
                if !isSaving {
                    let raw = await runCommandCollectingOutput(
                        "monitor json", perLineTimeout: 0.15, maxLines: 5)
                    if let snap = CLIOutputParser.parseMonitorSnapshot(from: raw) {
                        monitorSnapshot = snap
                    }
                }
                try? await Task.sleep(for: .milliseconds(250))
            }
            isMonitoring = false
            monitorSnapshot = nil
        }
    }

    func stopMonitoring() {
        guard isMonitoring else { return }
        isMonitoring = false
        monitorTask?.cancel()
        monitorTask = nil
        monitorSnapshot = nil
        log.append("Monitor: stopped")
    }

    // MARK: - MIDI Stats

    func loadMidiRxStats() async -> MidiRxStats {
        let raw = await runCommandCollectingOutput("midi rx_stats", perLineTimeout: 0.6, maxLines: 50)
        return CLIOutputParser.parseMidiRxStats(raw)
    }

    func resetMidiRxStats() {
        sendCommand("midi rx_reset")
    }

    // MARK: - Terminal

    func sendRawCommand(_ text: String) {
        guard let port = connectedPort, port.isOpen else { return }
        terminalOutput += text + "\r\n"
        let cmd = text.hasSuffix("\r\n") ? text : text + "\r\n"
        port.send(cmd.data(using: .utf8)!)
        log.append("> \(text)")
    }

    func clearTerminalOutput() { terminalOutput = "" }

    // MARK: - ORSSerialPortDelegate

    func serialPortWasRemovedFromSystem(_ serialPort: ORSSerialPort) {
        serialPortWasRemoved(fromSystem: serialPort)
    }

    func serialPort(_ serialPort: ORSSerialPort, didReceive data: Data) {
        if let str = String(data: data, encoding: .utf8) {
            terminalOutput += CLIOutputParser.stripANSI(str)
        }
        readBuffer.append(data)
        if let cont = pendingReadContinuation,
           let str = String(data: readBuffer, encoding: .utf8),
           str.contains("\n") {
            pendingReadContinuation = nil
            if let range = str.range(of: "\n") {
                let line = String(str[..<range.upperBound])
                let remaining = String(str[range.upperBound...])
                readBuffer = Data(remaining.utf8)
                cont.resume(returning: line)
            } else {
                readBuffer.removeAll(keepingCapacity: true)
                cont.resume(returning: str)
            }
        }
    }

    func serialPortWasOpened(_ serialPort: ORSSerialPort) {
        log.append("Port opened: \(serialPort.path)")
    }

    func serialPort(_ serialPort: ORSSerialPort, didEncounterError error: Error) {
        let e = error as NSError
        log.append("Serial error: \(e.localizedDescription) (\(e.domain) \(e.code))")
        if e.domain == NSPOSIXErrorDomain && e.code == 1 {
            log.append("Hint: Permission denied. Prefer /dev/cu.* over /dev/tty.* ports.")
        }
    }

    func serialPortWasRemoved(fromSystem serialPort: ORSSerialPort) {
        if let cont = pendingReadContinuation {
            pendingReadContinuation = nil
            cont.resume(returning: "")
        }
        if connectedPort === serialPort {
            isConnected = false
            connectedPort = nil
            resetState()
            log.append("Device removed from system.")
        }
    }

    func serialPortWasClosed(_ serialPort: ORSSerialPort) {
        if let cont = pendingReadContinuation {
            pendingReadContinuation = nil
            cont.resume(returning: "")
        }
        if connectedPort === serialPort {
            isConnected = false
            connectedPort = nil
            resetState()
            log.append("Port closed.")
        }
    }

    // MARK: - Helpers

    private func resetState() {
        stopMonitoring()
        patchCount = 0
        firmwareVersion = ""
        capabilitiesDiscovered = false
        currentPatchIndex = 0
        currentPatchConfig = nil
        currentGlobalConfig = nil
        currentPipelineConfig = nil
        discoveredCurrentPatch = nil
    }
}
