import SwiftUI

/// Draggable wireframe sphere for setting rho/theta orientation.
/// Drag horizontally to change theta, vertically to change rho.
/// Blue arrow = rotation axis. Orange arrow = live acceleration vector (when monitoring).
struct OrientationSphereView: View {
    @Binding var rho: Double    // degrees 0–360
    @Binding var theta: Double  // degrees 0–360
    var snapshot: MonitorSnapshot? = nil

    @State private var dragBaseRho: Double = 0
    @State private var dragBaseTheta: Double = 0
    @State private var isDragging: Bool = false

    var body: some View {
        Canvas { ctx, size in
            let c = CGPoint(x: size.width / 2, y: size.height / 2)
            let r = (min(size.width, size.height) / 2) - 6

            // Sphere boundary circle
            ctx.stroke(
                Path(ellipseIn: CGRect(x: c.x - r, y: c.y - r, width: r * 2, height: r * 2)),
                with: .color(.white.opacity(0.3)), lineWidth: 1.5
            )

            // Latitude lines
            for lat in stride(from: -60.0, through: 60.0, by: 30.0) {
                ctx.stroke(latPath(lat, c: c, r: r),
                           with: .color(.white.opacity(lat == 0 ? 0.4 : 0.18)),
                           lineWidth: lat == 0 ? 1.0 : 0.5)
            }

            // Longitude lines (great circles through poles)
            for lon in stride(from: 0.0, through: 150.0, by: 30.0) {
                ctx.stroke(lonPath(lon, c: c, r: r),
                           with: .color(.white.opacity(lon == 0 ? 0.4 : 0.18)),
                           lineWidth: lon == 0 ? 1.0 : 0.5)
            }

            // Rotation axis arrow (accent color, from center to sphere surface)
            let axisEnd = proj(rotate3((0, 0, 1)), c: c, r: r)
            var axisLine = Path()
            axisLine.move(to: c)
            axisLine.addLine(to: axisEnd)
            ctx.stroke(axisLine, with: .color(.accentColor), lineWidth: 2.5)
            ctx.fill(
                Path(ellipseIn: CGRect(x: axisEnd.x - 5, y: axisEnd.y - 5, width: 10, height: 10)),
                with: .color(.accentColor)
            )

            // Live acceleration vector from monitor (orange)
            if let snap = snapshot {
                let v = snap.rotatedVector
                let len = sqrt(v.x * v.x + v.y * v.y + v.z * v.z)
                if len > 0.01 {
                    let tip = CGPoint(x: c.x + CGFloat(v.x / len) * r,
                                     y: c.y - CGFloat(v.y / len) * r)
                    var vLine = Path()
                    vLine.move(to: c)
                    vLine.addLine(to: tip)
                    ctx.stroke(vLine, with: .color(.orange), lineWidth: 2)
                    ctx.fill(
                        Path(ellipseIn: CGRect(x: tip.x - 4, y: tip.y - 4, width: 8, height: 8)),
                        with: .color(.orange)
                    )
                }
            }
        }
        .frame(width: 200, height: 200)
        .background(Color.black.opacity(0.2), in: Circle())
        .gesture(
            DragGesture(minimumDistance: 1)
                .onChanged { value in
                    if !isDragging {
                        isDragging = true
                        dragBaseRho = rho
                        dragBaseTheta = theta
                    }
                    theta = mod360(dragBaseTheta + Double(value.translation.width)  * 0.5)
                    rho   = mod360(dragBaseRho   + Double(value.translation.height) * 0.5)
                }
                .onEnded { _ in isDragging = false }
        )
    }

    // MARK: - 3D helpers

    /// Apply Ry(theta) ∘ Rx(rho) to a point on the unit sphere.
    private func rotate3(_ p: (Double, Double, Double)) -> (Double, Double, Double) {
        let rr = rho   * .pi / 180
        let tt = theta * .pi / 180
        let (x, y, z) = p
        let y1 = y * cos(rr) - z * sin(rr)
        let z1 = y * sin(rr) + z * cos(rr)
        let x2 = x * cos(tt) + z1 * sin(tt)
        return (x2, y1, -x * sin(tt) + z1 * cos(tt))
    }

    /// Orthographic projection (Y up, X right).
    private func proj(_ p: (Double, Double, Double), c: CGPoint, r: CGFloat) -> CGPoint {
        CGPoint(x: c.x + CGFloat(p.0) * r,
                y: c.y - CGFloat(p.1) * r)
    }

    private func latPath(_ lat: Double, c: CGPoint, r: CGFloat) -> Path {
        let latR = lat * .pi / 180
        var path = Path()
        for i in 0...64 {
            let lonR = Double(i) / 64.0 * 2 * .pi
            let pt = proj(rotate3((cos(latR) * cos(lonR), sin(latR), cos(latR) * sin(lonR))), c: c, r: r)
            if i == 0 { path.move(to: pt) } else { path.addLine(to: pt) }
        }
        return path
    }

    private func lonPath(_ lon: Double, c: CGPoint, r: CGFloat) -> Path {
        let lonR = lon * .pi / 180
        var path = Path()
        for i in 0...64 {
            let a = Double(i) / 64.0 * 2 * .pi
            let pt = proj(rotate3((cos(a) * cos(lonR), sin(a), cos(a) * sin(lonR))), c: c, r: r)
            if i == 0 { path.move(to: pt) } else { path.addLine(to: pt) }
        }
        return path
    }

    private func mod360(_ v: Double) -> Double {
        let r = v.truncatingRemainder(dividingBy: 360)
        return r < 0 ? r + 360 : r
    }
}

#Preview {
    struct Wrapper: View {
        @State var rho: Double = 30
        @State var theta: Double = 45
        var body: some View {
            OrientationSphereView(rho: $rho, theta: $theta)
                .padding()
                .background(.black)
        }
    }
    return Wrapper()
}
