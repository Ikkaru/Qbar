import QtQuick

Rectangle {
    id: windowStatus
    width: 220
    height: parent.height
    color: "transparent"
    radius: height / 2

    property string appName: windowStatusService.appName
    property string windowTitle: windowStatusService.windowTitle
    property bool isDesktop: windowStatusService.isDesktop

    // Two-line layout: small app name on top, window title below.
    // Poppins has a tall default line height, so lineHeight is pinned and a
        // negative spacing pulls the two lines together.
        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: -4

            Text {
                width: parent.width
                text: windowStatus.appName
                color: theme.barTextColor
                Behavior on color { ColorAnimation { duration: 300 } }
                font.family: theme.font
                font.pixelSize: 10
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
                lineHeight: 1.0
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignLeft
            }

            Text {
                width: parent.width
                text: windowStatus.windowTitle
                color: theme.barTextColor
                Behavior on color { ColorAnimation { duration: 300 } }
                opacity: 0.85
                font.family: theme.font
                font.pixelSize: 12
                font.weight: Font.Medium
                lineHeight: 1.0
                elide: Text.ElideRight
                maximumLineCount: 1
                horizontalAlignment: Text.AlignLeft
            }
        }
}