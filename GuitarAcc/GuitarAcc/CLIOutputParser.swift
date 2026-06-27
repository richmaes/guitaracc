// CLIOutputParser.swift
// Pure static parsing of Zephyr Shell CLI output from the GuitarAcc basestation.
// No serial port dependencies — all functions take a plain String and return structured values.

import Foundation

struct CLIOutputParser {

    // MARK: - ANSI

    /// Strip ANSI escape sequences (e.g. ESC[1;32m, ESC[m) from raw serial output.
    static func stripANSI(_ s: String) -> String {
        s.replacingOccurrences(of: "\u{1b}\\[[0-9;]*[A-Za-z]", with: "", options: .regularExpression)
    }

    // MARK: - Status

    struct StatusInfo {
        let configArea: String?
        let configAreaSeq: Int?
        let connectedDevices: Int?
        let midiOutputActive: Bool?
        let firmwareVersion: String?
    }

    static func parseStatus(_ raw: String) -> StatusInfo {
        let text = stripANSI(raw)
        var configArea: String?
        var configAreaSeq: Int?
        var connectedDevices: Int?
        var midiOutputActive: Bool?
        var firmwareVersion: String?

        for line in text.components(separatedBy: .newlines) {
            if configArea == nil,
               let r = line.range(of: #"(?i)config\s+area\s*:\s*([A-Za-z])"#, options: .regularExpression) {
                let m = String(line[r])
                if let lr = m.range(of: #"[A-Za-z]$"#, options: .regularExpression) {
                    configArea = String(m[lr]).uppercased()
                }
            }
            if configAreaSeq == nil,
               let r = line.range(of: #"seq\s*=\s*(\d+)"#, options: .regularExpression) {
                let m = String(line[r])
                if let nr = m.range(of: #"\d+"#, options: .regularExpression) {
                    configAreaSeq = Int(m[nr])
                }
            }
            if connectedDevices == nil,
               let r = line.range(of: #"(?i)connected\s+devices\s*:\s*(\d+)"#, options: .regularExpression) {
                let m = String(line[r])
                if let nr = m.range(of: #"\d+"#, options: .regularExpression) {
                    connectedDevices = Int(m[nr])
                }
            }
            if midiOutputActive == nil, line.lowercased().contains("midi output") {
                midiOutputActive = line.lowercased().contains("active") && !line.lowercased().contains("inactive")
            }
            if firmwareVersion == nil,
               let r = line.range(of: #"(?i)(?:fw|firmware|version)\s*[:=]\s*v?(\S+)"#, options: .regularExpression) {
                let m = String(line[r])
                if let cr = m.range(of: #"[:=]\s*v?"#, options: .regularExpression) {
                    let v = String(m[cr.upperBound...]).trimmingCharacters(in: .whitespaces)
                    if !v.isEmpty { firmwareVersion = v }
                }
            }
        }

        return StatusInfo(
            configArea: configArea,
            configAreaSeq: configAreaSeq,
            connectedDevices: connectedDevices,
            midiOutputActive: midiOutputActive,
            firmwareVersion: firmwareVersion
        )
    }

    // MARK: - MIDI Rx Stats

    static func parseMidiRxStats(_ raw: String) -> MidiRxStats {
        let text = stripANSI(raw)
        func intAfter(_ pattern: String, in source: String) -> Int? {
            guard let r = source.range(of: pattern, options: .regularExpression) else { return nil }
            let m = String(source[r])
            guard let nr = m.range(of: #"\d+"#, options: .regularExpression) else { return nil }
            return Int(m[nr])
        }
        return MidiRxStats(
            totalBytes:      intAfter(#"(?i)total\s+bytes.*?:\s*(\d+)"#, in: text) ?? 0,
            clockMessages:   intAfter(#"(?i)clock\s+messages.*?:\s*(\d+)"#, in: text) ?? 0,
            startMessages:   intAfter(#"(?i)start\s+messages.*?:\s*(\d+)"#, in: text) ?? 0,
            continueMessages: intAfter(#"(?i)continue\s+messages.*?:\s*(\d+)"#, in: text) ?? 0,
            stopMessages:    intAfter(#"(?i)stop\s+messages.*?:\s*(\d+)"#, in: text) ?? 0,
            otherMessages:   intAfter(#"(?i)other\s+messages.*?:\s*(\d+)"#, in: text) ?? 0,
            clockIntervalUs: intAfter(#"(?i)clock\s+interval.*?:\s*(\d+)"#, in: text),
            estimatedBpm:    intAfter(#"~\s*(\d+)\s*BPM"#, in: text)
        )
    }

    // MARK: - JSON Extraction

    /// Extract the outermost JSON object from raw serial output, stripping ANSI and command echo.
    static func extractJSON(from raw: String) -> String? {
        let text = stripANSI(raw)
        guard let startIdx = text.firstIndex(of: "{") else { return nil }
        var depth = 0
        var endIdx: String.Index?
        var i = startIdx
        while i < text.endIndex {
            switch text[i] {
            case "{": depth += 1
            case "}":
                depth -= 1
                if depth == 0 { endIdx = i }
            default: break
            }
            if depth == 0 { break }
            text.formIndex(after: &i)
        }
        guard let endIdx else { return nil }
        return String(text[startIdx...endIdx])
    }

    // MARK: - Patch Export

    static func parsePatchExport(from raw: String) -> PatchConfig? {
        guard let jsonStr = extractJSON(from: raw),
              let data = jsonStr.data(using: .utf8) else { return nil }
        let decoder = JSONDecoder()
        decoder.keyDecodingStrategy = .convertFromSnakeCase
        guard let envelope = try? decoder.decode(PatchExportEnvelope.self, from: data),
              let patch = envelope.config.patches.first else { return nil }
        return patch
    }

    // MARK: - Global Export

    struct GlobalExport: Equatable {
        let global: GlobalConfig
        let firmwareVersion: String?
        let patchCount: Int?
        let currentPatch: Int?
    }

    static func parseGlobalExport(from raw: String) -> GlobalExport? {
        guard let jsonStr = extractJSON(from: raw),
              let data = jsonStr.data(using: .utf8) else { return nil }
        let decoder = JSONDecoder()
        decoder.keyDecodingStrategy = .convertFromSnakeCase
        guard let envelope = try? decoder.decode(GlobalExportEnvelope.self, from: data) else { return nil }
        return GlobalExport(
            global: envelope.config.global,
            firmwareVersion: envelope.firmwareVersion,
            patchCount: envelope.patchCount,
            currentPatch: envelope.currentPatch
        )
    }

    // MARK: - Pipeline JSON

    /// Parse output of `pipeline json` into a PipelineConfig.
    static func parsePipelineJson(from raw: String) -> PipelineConfig? {
        guard let jsonStr = extractJSON(from: raw),
              let data = jsonStr.data(using: .utf8),
              let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else { return nil }

        let patch = obj["patch"] as? Int ?? 0

        guard let rotation = obj["rotation"] as? [String: Any],
              let rho   = rotation["rho_degrees"]   as? Double,
              let theta = rotation["theta_degrees"]  as? Double,
              let output = obj["output"] as? [String: Any],
              let midiCC = output["midi_cc"] as? Int,
              let conv = obj["conversion"] as? [String: Any],
              let funcType = conv["function_type"] as? String else { return nil }

        let params = conv["parameters"] as? [String: Any] ?? [:]
        let conversion: ConversionFunction
        switch funcType.lowercased() {
        case "linear":
            let scale  = params["scale"]  as? Double ?? 1.0
            let offset = params["offset"] as? Double ?? 0.0
            conversion = .linear(scale: scale, offset: offset)
        case "exponential":
            let exp = params["exponent"] as? Double ?? 1.0
            conversion = .exponential(exponent: exp)
        case "scurve":
            let steep = params["steepness"] as? Double ?? 5.0
            conversion = .scurve(steepness: steep)
        case "lookup":
            let vals = (0..<5).map { params["v\($0)"] as? Int ?? 0 }
            conversion = .lookup(values: vals)
        default:
            conversion = .linear(scale: 1.0, offset: 0.0)
        }

        return PipelineConfig(patch: patch, rhoDegrees: rho, thetaDegrees: theta,
                              midiCC: midiCC, conversion: conversion)
    }

    // MARK: - Monitor Snapshot

    /// Parse output of `monitor json` (which may include command echo and prompt) into a MonitorSnapshot.
    static func parseMonitorSnapshot(from raw: String) -> MonitorSnapshot? {
        guard let jsonStr = extractJSON(from: raw),
              let data = jsonStr.data(using: .utf8) else { return nil }
        let decoder = JSONDecoder()
        decoder.keyDecodingStrategy = .convertFromSnakeCase
        return try? decoder.decode(MonitorSnapshot.self, from: data)
    }

    // MARK: - Patch Index Extraction

    /// Attempt to extract a current-patch index from arbitrary CLI text (config show / status).
    static func extractCurrentPatchIndex(from text: String) -> Int? {
        let patterns = [
            #"(?i)active\s+patch\s*[:=]\s*(\d+)"#,
            #"(?i)selected\s+patch\s*[:=]\s*(\d+)"#,
            #"(?i)patch\s*[:=]\s*(\d+)"#
        ]
        let clean = stripANSI(text)
        for pattern in patterns {
            if let r = clean.range(of: pattern, options: .regularExpression) {
                let m = String(clean[r])
                if let nr = m.range(of: #"\d+"#, options: .regularExpression),
                   let val = Int(m[nr]) { return val }
            }
        }
        return nil
    }

    // MARK: - Private Envelope Types

    private struct PatchExportEnvelope: Codable {
        let version: Int
        let config: PatchExportConfig
        struct PatchExportConfig: Codable {
            let patches: [PatchConfig]
        }
    }

    private struct GlobalExportEnvelope: Codable {
        let version: Int
        let firmwareVersion: String?
        let patchCount: Int?
        let currentPatch: Int?
        let config: GlobalExportConfig
        struct GlobalExportConfig: Codable {
            let global: GlobalConfig
        }
    }
}
