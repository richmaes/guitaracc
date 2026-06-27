// DeviceModels.swift
// Codable model types for GuitarAcc basestation data.

import Foundation

// MARK: - Global Config

struct GlobalConfig: Codable, Equatable {
    var defaultPatch: Int
    var midiChannel: Int
    var maxGuitars: Int
    var bleScanIntervalMs: Int
    var ledBrightness: Int
    var accelScale: [Int]          // 6 values [X,Y,Z,Roll,Pitch,Yaw] in milli-g
    var accelOffset: [Int]         // 6 values in milli-g
    var runningAverageEnable: Bool
    var runningAverageDepth: Int
}

// MARK: - Patch Config

struct PatchConfig: Codable, Equatable {
    var patchNum: Int
    var patchName: String
    var ledMode: Int
    var midiDeadzone: Int?         // firmware v2+ field name
    var accelDeadzone: Int?        // older firmware field name
    var velocityCurve: Int?        // legacy
    var ccMapping: [Int]?          // 6 values, legacy
    var accelMin: [Int]?           // legacy
    var accelMax: [Int]?           // legacy
    var accelInvert: Int?          // legacy bitmask
}

// MARK: - Pipeline Config

struct PipelineConfig: Equatable {
    var patch: Int
    var rhoDegrees: Double
    var thetaDegrees: Double
    var midiCC: Int
    var conversion: ConversionFunction
}

enum ConversionFunction: Equatable {
    case linear(scale: Double, offset: Double)
    case exponential(exponent: Double)
    case scurve(steepness: Double)
    case lookup(values: [Int])     // exactly 5 MIDI values (0-127)

    var typeName: String {
        switch self {
        case .linear:      return "linear"
        case .exponential: return "exponential"
        case .scurve:      return "scurve"
        case .lookup:      return "lookup"
        }
    }

    /// Formats the CLI fragment that follows `pipeline set <rho> <theta> <midi_cc>`.
    var cliFragment: String {
        switch self {
        case .linear(let scale, let offset):
            return String(format: "linear %.4f %.4f", scale, offset)
        case .exponential(let exp):
            return String(format: "exponential %.4f", exp)
        case .scurve(let steep):
            return String(format: "scurve %.4f", steep)
        case .lookup(let vals):
            return "lookup " + vals.prefix(5).map(String.init).joined(separator: " ")
        }
    }

    static var defaultLinear: ConversionFunction { .linear(scale: 1.0, offset: 0.0) }
}

// MARK: - MIDI Rx Stats

struct MidiRxStats {
    var totalBytes: Int
    var clockMessages: Int
    var startMessages: Int
    var continueMessages: Int
    var stopMessages: Int
    var otherMessages: Int
    var clockIntervalUs: Int?
    var estimatedBpm: Int?
}

// MARK: - Monitor Snapshot (from `monitor json`)

struct MonitorSnapshot: Decodable {
    struct RawAxis: Decodable {
        let x: Int
        let y: Int
        let z: Int
    }
    struct Vec3: Decodable {
        let x: Double
        let y: Double
        let z: Double
    }
    struct MidiOut: Decodable {
        let cc: Int
        let value: Int
    }
    let timestampMs: Int
    let rawAxis: RawAxis
    let inputVector: Vec3
    let rotatedVector: Vec3
    let normalizedVector: Vec3
    let scalarProjection: Double
    let functionType: String
    let midiOutput: MidiOut
}

// MARK: - Axis Names

enum AccelAxis: Int, CaseIterable {
    case x = 0, y, z, roll, pitch, yaw

    var label: String {
        switch self {
        case .x:     return "X"
        case .y:     return "Y"
        case .z:     return "Z"
        case .roll:  return "Roll"
        case .pitch: return "Pitch"
        case .yaw:   return "Yaw"
        }
    }
}
