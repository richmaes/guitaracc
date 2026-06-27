import SwiftUI

struct ContentView: View {
    @EnvironmentObject var serial: USBSerialManager

    var body: some View {
        PatchView()
            .frame(minWidth: 700, minHeight: 500)
    }
}

#Preview {
    ContentView()
        .environmentObject(USBSerialManager())
}
