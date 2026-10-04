import QtQuick

Rectangle {
    id: clock
    width: 60
    height: parent.height
    color: "transparent"
    radius: height / 2

    property string timeText: clockService.timeText
    property string dateText: clockService.dateText

    Text {
        anchors.centerIn: parent
        text: clock.timeText
        color: theme.barTextColor
        Behavior on color { ColorAnimation { duration: 300 } }
        font.family: theme.font
        font.pixelSize: 13
        font.weight: Font.Medium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    MouseArea {
        anchors.fill: parent
        // No in-app calendar or toast UI: on Windows 11 the notification centre and
        // the calendar are one flyout with no public API, so the bar delegates to it.
        onClicked: barManager.openNotificationCenter()
    }
}
