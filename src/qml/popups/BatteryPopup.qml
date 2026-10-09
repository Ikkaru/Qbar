import QtQuick

// Hover readout for the battery: charge percentage plus the two states that are
// not visible from the icon alone - charging and economy mode. Lives in its own
// window because the bar is only 40px tall and anything drawn below the icon in
// the bar's own QML tree is clipped away.
Window {
    id: root

    // The tooltip hugs its text instead of using a fixed width. implicitWidth is
    // the natural width of the lines and is known as soon as they are
    // instantiated, so PopupHost can position the popup straight away.
    width: Math.max(Math.ceil(tipContent.implicitWidth) + 24, 132)
    height: Math.ceil(tipContent.implicitHeight) + 14
    visible: true
    // Same flags as the volume popup. Without FramelessWindowHint the tooltip
    // comes up as an ordinary decorated window with a title bar.
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"

    readonly property var battery: indicatorsService.state.battery
    readonly property bool present: !!(battery && battery.present)

    // `charging` from the service means "plugged in AND current is flowing", so
    // it goes false on a held-at-full battery. Deriving the label from it alone
    // reported a machine sitting at 100% on AC as Unplugged, which is the one
    // thing it demonstrably is not. The cable state is read separately.
    readonly property bool plugged: present && battery.plugged
    readonly property bool holding: present && battery.holding
    readonly property bool saver: present && battery.saver
    // Must exist on the root: fillColor below reads it unqualified. Without it
    // the binding throws ReferenceError on first evaluation, QML discards the
    // binding, and fillColor stays at its default #000000 - so the bar went
    // black whether or not the cable was in, which is the reported bug.
    readonly property bool charging: present && battery.charging

    // One status line. Power saving is checked before smart charge, matching the
    // icon order in Indicators.qml: holding is true at any full battery on AC, so
    // letting it win here meant power saving never reached the label either.
    readonly property string status: saver ? "Power Saving Mode"
                                  : holding ? "Fully Smart Charged"
                                  : plugged ? "Plugged In"
                                  : "Unplugged"

    // Time estimate. The service reports seconds, or -1 when the OS would not
    // say - and it can be -1 for only one of the two, since the two come from
    // different APIs: RemainingDischargeTime is reported by the OS, while
    // time-to-full has to be derived from the charge rate. A charge-limit
    // feature also stops charging, which leaves a real "left" but no "to full".
    readonly property int secondsLeft: battery.secondsLeft !== undefined ? battery.secondsLeft : -1
    readonly property int secondsToFull: battery.secondsToFull !== undefined ? battery.secondsToFull : -1

    function formatDuration(seconds) {
        if (seconds < 0) return ""
        if (seconds < 60) return "< 1 min"
        const totalMinutes = Math.round(seconds / 60)
        const h = Math.floor(totalMinutes / 60)
        const m = totalMinutes % 60
        if (h <= 0) return m + " min"
        if (m === 0) return h + " h"
        return h + " h " + m + " min"
    }

    // Fill colour of the charge bar. Low is checked first so a nearly empty
    // battery stays red whatever else is true - the same rule the bar icon
    // follows, so the two never disagree.
    //
    // "Fully charged" covers both a full battery and one being held at full by
    // a charge limit, since they look the same to the user: 100% and no longer
    // moving.
    //
    // Gated on plugged. Unplugged has to reach the white fallback below, and
    // without the gate a battery unplugged a moment after reaching 100% read
    // percent >= 100 and came out deep blue - which is not a state the machine
    // is in, since with the cable out it is draining. holding already implies
    // plugged, so it is left alone.
    readonly property bool full: plugged && (battery.percent >= 100 || holding)

    readonly property color fillColor: battery.percent <= 20 ? theme.error
                                  : saver ? theme.tertiary
                                  : full ? theme.full
                                  : charging ? theme.charging
                                  : "#FFFFFF"

    readonly property string timeLeft: secondsLeft > 0 ? formatDuration(secondsLeft) : ""
    readonly property string timeToFull: secondsToFull > 0 ? formatDuration(secondsToFull) : ""

    // Only one at a time: while charging there is no "time left" to speak of,
    // and while unplugged there is nothing filling.
    readonly property string timeText: plugged ? timeToFull : timeLeft

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
            spacing: 3

            // Charge level: bar plus number, so the coarse 3-step icon is backed
            // by an exact reading here.
            //
            // The row sizes itself from the bar and the percentage rather than
            // filling the Column. Inheriting the Column width made the bar a
            // few pixels wide, because the labels were narrower than the space
            // the bar was meant to occupy - it collapsed into what looked like a
            // stray white dot.
            Item {
                implicitWidth: track.width + 6 + percentText.width
                height: 22

                Rectangle {
                    id: track
                    anchors.verticalCenter: parent.verticalCenter
                    x: 0
                    width: 86
                    height: 6
                    radius: 3
                    // Unfilled part stays translucent white, so the fill colour
                    // is the only thing carrying the state.
                    color: "#33FFFFFF"

                    Rectangle {
                        id: fill
                        width: track.width * Math.min(1, Math.max(0, battery.percent / 100))
                        height: parent.height
                        radius: parent.radius
                        color: root.fillColor
                        Behavior on color { ColorAnimation { duration: 300 } }
                    }
                }

                Text {
                    id: percentText
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: track.right
                    anchors.leftMargin: 6
                    text: Math.round(battery.percent) + "%"
                    color: "#FFFFFF"
                    font.family: theme.font
                    font.pixelSize: 11
                    font.weight: Font.Medium
                }

            }

            // Plug state / power mode. Text only: the icon subset has no leaf or
            // bolt-with-text pairing, and an emoji would fall back to a different
            // font and look nothing like the rest of the bar.
            //
            // Width comes from tipContent.implicitWidth, never from
            // tipContent.width: the latter makes the Column depend on its own
            // children, which sizes the window, which sizes the Column again.
            // That cycle settled on the fixed floor instead of the real text and
            // clipped the labels.
            Text {
                text: root.status
                color: "#FFFFFF"
                font.family: theme.font
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                width: tipContent.implicitWidth
            }

            // Time estimate, on its own line under the status. Same size and
            // colour as the status line above it: it is part of the same
            // sentence, and a dimmer or smaller line read as a footnote.
            Text {
                visible: root.timeText.length > 0
                text: root.timeText
                color: "#FFFFFF"
                font.family: theme.font
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                width: tipContent.implicitWidth
            }
        }
    }
}