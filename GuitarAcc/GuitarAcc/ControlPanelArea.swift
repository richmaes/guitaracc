import SwiftUI

struct ControlPanelArea: View {
    @EnvironmentObject var serial: USBSerialManager

    var body: some View {
        ScrollView(.horizontal) {
            HStack(alignment: .top, spacing: 16) {
                PipelineConfigSection()
            }
            .padding(.vertical, 8)
            .padding(.horizontal, 4)
        }
        .scrollIndicators(.visible)
        .background(.ultraThinMaterial)
        .clipShape(RoundedRectangle(cornerRadius: 8))
        .padding(.vertical, 8)
    }
}
