import QtQuick
import QtQuick.Layouts
import QtQuick.Effects

Rectangle {
    id: bar
    anchors.fill: parent
    color: "transparent"

    // Backdrop: clear = nothing, acrylic = DWM blur + tint, solid = flat color.
    // A DWM system backdrop always fills the whole window rectangle, so it is
    // only enabled for square; rounded shapes fall back to the flat fill.
    Rectangle {
        id: surface
        anchors.fill: parent
        radius: barWindow.effectiveRadius

        color: {
            if (barWindow.backdropMode === "solid")
                return Qt.rgba(barWindow.backdropColor.r, barWindow.backdropColor.g,
                               barWindow.backdropColor.b, barWindow.backdropOpacity)
            // Acrylic's blur comes from the window manager, but Windows 11 renders
            // the material at its own density and ignores the requested tint, so
            // this layer is the only real density control. It is deliberately
            // faint: the point of the mode is that the wallpaper stays visible
            // through the blur. At backdropStrength 0 the bar is pure glass.
            // Acrylic's blur comes from the window manager, but Windows renders
            // the material at its own density and ignores the requested tint, so
            // this layer is the only real density control. It is deliberately
            // faint: the point of the mode is that the wallpaper stays visible
            // through the blur. Measured average red with a mid-tone wallpaper:
            // strength 0 -> 183, 0.35 -> 151, 1 -> 92.
            if (barWindow.backdropMode === "acrylic")
                return Qt.rgba(barWindow.backdropColor.r, barWindow.backdropColor.g,
                               barWindow.backdropColor.b,
                               barWindow.backdropStrength * 0.55)
            // "clear" is still a dark scrim, not zero alpha. Pure transparency
            // leaves white text sitting straight on the wallpaper, which reads as
            // a halo on light backgrounds. surfaceOpacity controls how much of
            // the wallpaper shows through.
            return Qt.rgba(0.06, 0.06, 0.09, barWindow.surfaceOpacity)
        }

        border.width: barWindow.borderWidth
        border.color: barWindow.borderColor
    }

    // macOS-style separation. Two layers working together:
    //   - the hairline, the crisp 1px rule that does the actual separating
    //   - a soft gradient underneath it, the "almost not noticed" part
    //
    // Both live inside the bar's own height on purpose. Growing the window so a
    // real drop shadow could escape would put a transparent strip over the top
    // of every maximised window, and that strip would swallow clicks meant for
    // them.
    Rectangle {
        id: edgeShadow
        visible: barWindow.shadowEnabled && barWindow.shadowHeight > 0
        // Inset by the corner radius so the band stops at the flat part of the
        // bottom edge. Filling the full width would paint into the corner voids
        // of a pill or rounded bar and leave dark triangles there.
        anchors.left: surface.left
        anchors.right: surface.right
        anchors.leftMargin: barWindow.effectiveRadius
        anchors.rightMargin: barWindow.effectiveRadius
        anchors.bottom: parent.bottom
        height: barWindow.shadowHeight
        // A Rectangle gradient replaces color entirely, so the fill lives here.
        // The falloff is quadratic rather than linear: macOS thins the shadow
        // out gradually, so a straight ramp reads as a grey smear by comparison.
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: Qt.rgba(0, 0, 0, 0)
            }
            GradientStop {
                position: 0.25
                color: Qt.rgba(0, 0, 0, barWindow.shadowOpacity * 0.28)
            }
            GradientStop {
                position: 0.5
                color: Qt.rgba(0, 0, 0, barWindow.shadowOpacity * 0.52)
            }
            GradientStop {
                position: 0.75
                color: Qt.rgba(0, 0, 0, barWindow.shadowOpacity * 0.78)
            }
            GradientStop {
                position: 1.0
                color: Qt.rgba(0, 0, 0, barWindow.shadowOpacity)
            }
        }
    }

    // Hairline: the crisp 1px rule that reads against the desktop. Half alpha of
    // white, because the line has to survive both a dark and a light wallpaper —
    // black would vanish on a dark background and white would vanish on a light one.
    Rectangle {
        visible: barWindow.shadowEnabled && barWindow.shadowHairline
        anchors.left: surface.left
        anchors.right: surface.right
        anchors.leftMargin: barWindow.effectiveRadius
        anchors.rightMargin: barWindow.effectiveRadius
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 1
        height: 1
        color: Qt.rgba(1, 1, 1, barWindow.shadowHairlineOpacity)
    }

    // Module zones
    RowLayout {
        id: zones
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 8

        // Modules sit above the edge shadow, so the shadow never dims them.

        // Left zone
        Row {
            id: leftZone
            spacing: 4
            Layout.alignment: Qt.AlignLeft

            Repeater {
                model: moduleRegistry.leftModules
                delegate: Loader {
                    height: bar.height - 8
                    source: moduleRegistry.qmlUrlFor(modelData)
                }
            }
        }

        // Spacer
        Item {
            Layout.fillWidth: true
        }

        // Center zone
        Row {
            id: centerZone
            spacing: 4
            Layout.alignment: Qt.AlignHCenter

            Repeater {
                model: moduleRegistry.centerModules
                delegate: Loader {
                    height: bar.height - 8
                    source: moduleRegistry.qmlUrlFor(modelData)
                }
            }
        }

        // Spacer
        Item {
            Layout.fillWidth: true
        }

        // Right zone. spacing 10 matches the rhythm inside the indicators
        // module, so gaps between modules and gaps within a module look alike.
        Row {
            id: rightZone
            spacing: 10
            Layout.alignment: Qt.AlignRight

            Repeater {
                model: moduleRegistry.rightModules
                delegate: Loader {
                    height: bar.height - 8
                    source: moduleRegistry.qmlUrlFor(modelData)
                }
            }
        }
    }
}
