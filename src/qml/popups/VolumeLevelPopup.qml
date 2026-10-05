import QtQuick

// Hover readout for the volume icon: the master level in percent, plus a bar so
// the value is readable at a glance without pinning a number.
//
// Deliberately read-only. Clicking the icon still opens VolumePopup.qml, which is
// the full mixer with sliders, so a hover tooltip that also changed volume would
// be two controls fighting over the same gesture.
Window {
    id: root

    // Hugs its content, same as the other hover readouts.
    width: Math.ceil(tipContent.implicitWidth) + 24
    height: Math.ceil(tipContent.implicitHeight) + 14
    visible: true
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"

    readonly property bool muted: volumeService.masterMuted

    // masterLevel is 0.0-1.0, not 0-100 - it comes straight from
    // GetMasterVolumeLevelScalar. Comparing it against a percentage threshold
    // makes the low-volume branch permanently true, so scale it here instead.
    readonly property int percent: Math.round(volumeService.masterLevel * 100)

    // A muted device has no meaningful level, so the bar shows empty rather than
    // whatever the last non-muted value happened to be.
    readonly property real shownLevel: muted ? 0.0 : volumeService.masterLevel

    Item {
        id: popupRoot
        anchors.fill: parent
        opacity: 0.0

        // Readout, so it fades only. No scale, unlike the volume mixer.
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
            // The bar itself can hide the popup by losing hover, and a click on
            // the icon swaps in the mixer, which closes this one.
            function onCloseRequested() {
                fadeOut.start()
            }
            function onFinishClose() {
                Qt.callLater(root.close)
            }
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
        }

        Column {
            id: tipContent
            anchors.centerIn: parent
            spacing: 6

            Text {
                text: root.muted ? "Muted" : root.percent + "%"
                color: "#FFFFFF"
                font.family: theme.font
                font.pixelSize: 11
                font.weight: Font.Medium
                horizontalAlignment: Text.AlignHCenter
            }

            // Track with a filled portion, drawn rather than themed so the ratio
            // is exact. Rounded so it matches the pill shape of the other popups.
            Rectangle {
                width: 108
                height: 4
                radius: height / 2
                color: "#FFFFFF"
                opacity: 0.25

                Rectangle {
                    width: Math.max(root.shownLevel, 0) * parent.width
                    height: parent.height
                    radius: height / 2
                    // Behaviour, so dragging the mixer slider under the tooltip
                    // animates instead of snapping.
                    Behavior on width { NumberAnimation { duration: 90 } }
                    color: root.muted ? "#e5484d" : "#FFFFFF"
                }
            }
        }
    }
}
