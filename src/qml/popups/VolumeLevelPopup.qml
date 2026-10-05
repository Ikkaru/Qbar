import QtQuick

// Hover readout for the volume icon: the master level as a single line of text.
//
// Deliberately read-only and deliberately text only. Clicking the icon still
// opens VolumePopup.qml, which is the full mixer with sliders, so a hover
// tooltip that also changed volume would be two controls fighting over the same
// gesture - and the level is a number the user reads once, not a value they
// adjust here.
Window {
    id: root

    // Hugs its text, same as the other hover readouts. implicitWidth is known as
    // soon as the line is instantiated, so PopupHost can position it straight
    // away.
    width: Math.ceil(tipContent.implicitWidth) + 24
    height: Math.ceil(tipContent.implicitHeight) + 14
    visible: true
    // Same flags as the volume popup. Without FramelessWindowHint the tooltip
    // comes up as an ordinary decorated window with a title bar.
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"

    readonly property bool muted: volumeService.masterMuted

    // masterLevel is 0.0-1.0, not 0-100 - it comes straight from
    // GetMasterVolumeLevelScalar. Comparing it against a percentage threshold
    // makes the low-volume branch permanently true, so scale it here.
    readonly property string label: muted
        ? "Master Volume: Muted"
        : "Master Volume: " + Math.round(volumeService.masterLevel * 100) + "%"

    Item {
        id: popupRoot
        anchors.fill: parent
        opacity: 0.0

        // A readout, so it fades only. No scale, unlike the volume mixer.
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

        // Without this the tooltip has no surface of its own and renders as bare
        // text over whatever is behind it, which is unreadable over a busy
        // wallpaper. Matches the other hover readouts exactly.
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

            Text {
                text: root.label
                color: "#FFFFFF"
                font.family: theme.font
                font.pixelSize: 11
                font.weight: Font.Medium
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
