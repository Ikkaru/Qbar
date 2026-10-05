import QtQuick

Rectangle {
    id: network
    // Icon metrics shared with Indicators.qml. Every bar icon uses the same cell
    // height, pixel size and vertical alignment, otherwise glyphs from the same
    // font still land on different y positions and the row looks ragged.
    readonly property int iconCell: 22
    readonly property int iconSize: 19

    width: iconCell
    height: parent.height
    color: "transparent"
    radius: height / 2

    property string ssid: networkService.ssid
    property string reachability: networkService.reachability
    property string kind: networkService.kind
    property color textColor: theme.barTextColor

    // Icon follows the transport in use, not just reachability. `connected` only
    // reflects WLAN, so an Ethernet link used to fall through to wifi_off.
    readonly property string icon: network.kind === "wifi" ? "\uE63E"
                                  : network.kind === "lan" ? "\uEB2F"
                                  : "\uE648"

    Text {
        id: networkIcon
        anchors.fill: parent
        text: network.icon
        color: network.textColor
        Behavior on color { ColorAnimation { duration: 300 } }
        font.family: iconFont
        font.pixelSize: network.iconSize
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    // Hover: SSID + reachability, matching the shell's own wording.
    //
    // This has to be its own window. The bar is exactly 40px tall, so anything
    // drawn below the icon in the bar's QML tree lands outside the window rect
    // and is clipped away - which is why the tooltip never appeared. The same
    // constraint is why the edge shadow cannot escape either.
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        // Guarded on which popup is open rather than on whether one is: sliding
        // straight from another tooltip onto this icon while it was still visible
        // used to skip the open, leaving the pointer inside this hover area with
        // no further onEntered, so nothing appeared until it left and came back.
        onEntered: {
            popupHost.anchor = networkIcon;
            if (popupHost.currentUrl !== "qrc:/qml/popups/NetworkPopup.qml")
                popupHost.open("qrc:/qml/popups/NetworkPopup.qml");
        }
        onExited: popupHost.close()
        onClicked: {
            popupHost.close();
            barManager.openQuickSettings();
        }
    }
}
