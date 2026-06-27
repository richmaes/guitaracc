import SwiftUI

struct PatchSelectionArea: View {
    @EnvironmentObject var serial: USBSerialManager
    @State private var searchText = ""

    var filteredIndices: [Int] {
        let count = max(serial.patchCount, 0)
        guard count > 0 else { return [] }
        if searchText.isEmpty { return Array(0..<count) }
        return (0..<count).filter { i in
            "Patch \(i)".localizedCaseInsensitiveContains(searchText)
        }
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            if serial.isConnected && serial.patchCount > 0 {
                TextField("Search patches", text: $searchText)
                    .textFieldStyle(.roundedBorder)
                    .frame(maxWidth: 200)
                    .padding(.horizontal, 12)
                    .accessibilityIdentifier("PatchSelectionArea.SearchField")

                ScrollView(.horizontal) {
                    HStack(spacing: 6) {
                        ForEach(filteredIndices, id: \.self) { i in
                            PatchButton(index: i, isSelected: serial.currentPatchIndex == i) {
                                Task { await serial.selectPatch(i) }
                            }
                        }
                    }
                    .padding(.horizontal, 12)
                    .padding(.vertical, 4)
                }
                .scrollIndicators(.visible)
            } else if serial.isConnected {
                Text("Discovering patches…")
                    .foregroundStyle(.secondary)
                    .padding(.horizontal, 12)
                    .padding(.vertical, 8)
            } else {
                Text("No basestation connected")
                    .foregroundStyle(.secondary)
                    .padding(.horizontal, 12)
                    .padding(.vertical, 8)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(.vertical, 4)
    }
}

private struct PatchButton: View {
    let index: Int
    let isSelected: Bool
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Text("Patch \(index)")
                .font(.caption)
                .padding(.horizontal, 10)
                .padding(.vertical, 5)
        }
        .buttonStyle(.bordered)
        .tint(isSelected ? .accentColor : .secondary)
        .accessibilityIdentifier("PatchSelectionArea.Patch\(index)Button")
    }
}
