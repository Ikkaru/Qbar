import QtQuick

// Hover readout for the network module: SSID plus the reachability wording the
// shell itself uses. Lives in its own window because the bar is only 40px tall
// and anything drawn below the icon in the bar's own QML tree is clipped away.
Window {
    id: root

    // The tooltip hugs its text instead of using a fixed width. implicitWidth is
    // the natural width of the lines and is known as soon as they are
    // instantiated, so PopupHost can position the popup straight away. Long
    // SSIDs are capped and elided.
    width: Math.min(Math.ceil(tipContent.implicitWidth) + 24, 240)
    height: Math.ceil(tipContent.implicitHeight) + 14
    visible: true
    // Same flags as the volume popup. Without FramelessWindowHint the tooltip
    // comes up as an ordinary decorated window with a title bar.
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"

    // Background and content share this item so the tooltip fades as one. A
    // plain opacity fade, matching how the Windows taskbar dissolves its own
    // popups - no scale, this is a readout rather than a control surface.
    Item {
        id: popupRoot
        anchors.fill: parent
        opacity: 0.0

        PropertyAnimation on opacity {
            id: fadeIn
            to: 1.0
            duration: 120
            easing.type: Easing.OutCubic
        }
        PropertyAnimation on opacity {
            id: fadeOut
            to: 0.0
            duration: 100
            easing.type: Easing.InCubic
        }

        Component.onCompleted: Qt.callLater(startOpen)

        function startOpen() {
            fadeIn.start()
        }

        Connections {
            target: popupHost
            function onCloseRequested() {
                fadeOut.start()
            }
        }

        Rectangle {
            anchors.fill: parent
            color: theme.surfaceContainer
            radius: 12
            border.width: 0

            // Swallow presses so a click inside the tooltip does not fall through
            // to whatever is behind it.
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
            }
        }

        Column {
            id: tipContent
            anchors.centerIn: parent
            spacing: 2

            Text {
                text: networkService.ssid.length > 0 ? networkService.ssid : "Not connected"
                color: "#FFFFFF"
                font.family: theme.font
                font.pixelSize: 11
                font.weight: Font.Medium
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                // Constrained to the popup so a long SSID elides; implicitWidth
                // stays the natural width, which is what the popup sizes to.
                width: root.width - 24
            }

            Text {
                text: networkService.reachability
                color: "#FFFFFF"
                font.family: theme.font
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}