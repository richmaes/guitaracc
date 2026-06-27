import SwiftUI

@main
struct GuitarAccApp: App {
    @StateObject private var serialManager = USBSerialManager()

    init() {
        DebugLog.clear()
    }

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(serialManager)
        }
        .commands {
            CommandGroup(replacing: .newItem) {}
        }
    }
}
