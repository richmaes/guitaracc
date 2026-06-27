// DebugLog.swift
// File-based debug logger. Cleared on every app launch.
// Tail in terminal: tail -f ~/GuitarAcc_debug.log

import Foundation

enum DebugLog {
    static let fileURL = FileManager.default.homeDirectoryForCurrentUser
        .appendingPathComponent("GuitarAcc_debug.log")

    private static let df: DateFormatter = {
        let f = DateFormatter()
        f.dateFormat = "HH:mm:ss.SSS"
        return f
    }()

    /// Call once at app startup — overwrites the previous session.
    static func clear() {
        let header = "=== GuitarAcc session started \(Date()) ===\nLog: \(fileURL.path)\n\n"
        try? header.write(to: fileURL, atomically: true, encoding: .utf8)
    }

    /// Append a timestamped line. Noop if the file can't be opened.
    static func write(_ message: String) {
        let line = "[\(df.string(from: Date()))] \(message)\n"
        guard let data = line.data(using: .utf8) else { return }
        guard let handle = try? FileHandle(forWritingTo: fileURL) else { return }
        defer { try? handle.close() }
        try? handle.seekToEnd()
        try? handle.write(contentsOf: data)
    }
}
