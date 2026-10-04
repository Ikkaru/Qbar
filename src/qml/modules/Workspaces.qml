import QtQuick

Rectangle {
    id: workspaces
    width: 100
    height: parent.height
    color: "transparent"
    radius: height / 2

    property int activeIndex: workspacesService.activeIndex
    property int count: workspacesService.count
    property color pillColor: theme.barTextColor

    Row {
        anchors.centerIn: parent
        spacing: 4

        Repeater {
            model: workspaces.count
            delegate: Rectangle {
                width: 24
                height: 6
                radius: 3
                color: workspaces.pillColor
                Behavior on color { ColorAnimation { duration: 300 } }
                opacity: index === workspaces.activeIndex ? 1.0 : 0.3

                MouseArea {
                    anchors.fill: parent
                    onClicked: workspacesService.switchTo(index)
                }
            }
        }
    }
}
