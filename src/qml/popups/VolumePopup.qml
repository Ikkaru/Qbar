import QtQuick
import QtQuick.Controls

Window {
    id: volumePopup
    width: 300
    // Grows with the session list instead of scrolling: a Flickable breaks
    // slider dragging even when non-interactive, because it still grabs the
    // gesture and the drag dies one step after the press.
    height: Math.min(420, 96 + sessionsList.implicitHeight + 20)
    // Qt.Tool, not Qt.Dialog: Dialog implies modality and the popup stopped
    // receiving mouse input entirely.
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"
    visible: true

    // The background and the content are children of this one item so the whole
    // popup animates together. Animating the background on its own left the
    // content sitting on a transparent window and made the open look like two
    // mismatched steps.
    Item {
        id: popupRoot
        anchors.fill: parent
        opacity: 0.0
        // Pop anchored at the top-left, so the popup grows out of the bar rather
        // than expanding symmetrically. yScale does most of the work, which is
        // what reads as top-to-bottom.
        transform: Scale {
            id: pop
            origin.x: 0
            origin.y: 0
            xScale: 0.97
            yScale: 0.7
        }

        PropertyAnimation on opacity {
            id: fadeIn
            to: 1.0
            duration: 120
            easing.type: Easing.OutCubic
        }
        PropertyAnimation on opacity {
            id: fadeOut
            to: 0.0
            duration: 90
            easing.type: Easing.InCubic
        }
        PropertyAnimation {
            id: popXIn
            target: pop
            property: "xScale"
            to: 1.0
            duration: 140
            easing.type: Easing.OutCubic
        }
        PropertyAnimation {
            id: popYIn
            target: pop
            property: "yScale"
            to: 1.0
            duration: 170
            easing.type: Easing.OutCubic
        }
        PropertyAnimation {
            id: popXOut
            target: pop
            property: "xScale"
            to: 0.97
            duration: 90
            easing.type: Easing.InCubic
        }
        PropertyAnimation {
            id: popYOut
            target: pop
            property: "yScale"
            to: 0.7
            duration: 100
            easing.type: Easing.InCubic
        }

        // Deferred one event-loop turn so the animation starts after the window
        // is actually on screen; starting it during Component.onCompleted loses
        // the first frames to the show.
        Component.onCompleted: Qt.callLater(startOpen)

        function startOpen() {
            fadeIn.start();
            popXIn.start();
            popYIn.start();
        }

        Connections {
            target: popupHost
            function onCloseRequested() {
                fadeOut.start();
                popXOut.start();
                popYOut.start()
            }
        }

        Rectangle {
            anchors.fill: parent
            color: theme.surfaceContainer
            radius: 12
            border.width: 0

            // Swallow presses on the background so they do not reach the bar and
            // dismiss the popup mid-interaction.
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
            }
        }

        // Themed slider. Qt's default is a blue accent that clashes with the theme.
        component QbarSlider: Slider {
            id: control
            implicitHeight: 18
            padding: 0

            background: Rectangle {
                x: control.leftPadding
                y: control.topPadding + control.availableHeight / 2 - height / 2
                width: control.availableWidth
                height: 4
                radius: 2
                color: "#33FFFFFF"

                Rectangle {
                    width: control.visualPosition * parent.width
                    height: parent.height
                    radius: parent.radius
                    color: "#FFFFFF"
                    opacity: control.pressed ? 1.0 : 0.85
                }
            }

            handle: Rectangle {
                x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
                y: control.topPadding + control.availableHeight / 2 - height / 2
                width: 14
                height: 14
                radius: 7
                color: "#FFFFFF"
            }
        }

        Column {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            // Master section
            Column {
                spacing: 4
                width: parent.width

                Text {
                    text: "MASTER VOLUME"
                    color: theme.onSurfaceContainer
                    opacity: 0.55
                    font.family: theme.font
                    font.pixelSize: 9
                    font.weight: Font.DemiBold
                    font.capitalization: Font.AllUppercase
                }

                Row {
                    spacing: 8
                    width: parent.width

                    Text {
                        text: volumeService.masterMuted ? "\uE04F"
                                : volumeService.masterLevel < 0.35 ? "\uE04D"
                            : "\uE050"
                        color: "#FFFFFF"
                        font.family: iconFont
                        font.pixelSize: 18
                        width: 22; height: 22
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    QbarSlider {
                        width: parent.parent.width - 76
                        from: 0
                        to: 1
                        value: volumeService.masterLevel
                        anchors.verticalCenter: parent.verticalCenter
                        onMoved: volumeService.setMasterLevel(value)

                        onPressedChanged: volumeService.setPaused(pressed)
                    }

                    Text {
                        text: Math.round(volumeService.masterLevel * 100) + "%"
                        color: theme.onSurfaceContainer
                        font.family: theme.font
                        font.pixelSize: 11
                        width: 40
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            // Per-app section header
            Text {
                text: "VOLUME MIXER"
                color: theme.onSurfaceContainer
                opacity: 0.55
                font.family: theme.font
                font.pixelSize: 9
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
            }

            Column {
                id: sessionsList
                spacing: 6
                width: parent.width

                Repeater {
                    model: volumeService.sessions

                    delegate: Row {
                        spacing: 8
                        width: sessionsList.width

                        // App icon; click to mute. Falls back to a speaker glyph when
                        // the executable exposes no icon.
                        Item {
                            width: 22
                            height: 22
                            anchors.verticalCenter: parent.verticalCenter

                            Image {
                                anchors.fill: parent
                                anchors.margins: 2
                                visible: modelData.icon !== undefined && modelData.icon !== ""
                                source: modelData.icon !== undefined ? modelData.icon : ""
                                sourceSize.width: 18
                                sourceSize.height: 18
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                opacity: modelData.muted ? 0.4 : 1.0
                            }

                            Text {
                                anchors.fill: parent
                                visible: modelData.icon === undefined || modelData.icon === ""
                                text: modelData.muted ? "\uE04F" : "\uE050"
                                color: modelData.muted ? "#E05555" : "#FFFFFF"
                                font.family: iconFont
                                font.pixelSize: 16
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: volumeService.setSessionMuted(modelData.pid, !modelData.muted)
                            }
                        }

                        Text {
                            text: modelData.name
                            color: theme.onSurfaceContainer
                            font.family: theme.font
                            font.pixelSize: 11
                            width: 80
                            elide: Text.ElideRight
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        QbarSlider {
                            width: sessionsList.width - 142
                            from: 0
                            to: 1
                            value: modelData.level
                            anchors.verticalCenter: parent.verticalCenter
                            onMoved: volumeService.setSessionLevel(modelData.pid, value)

                            // Pause the model refresh while dragging. A rebuild
                            // destroys this delegate and drops the mouse grab.
                            onPressedChanged: volumeService.setPaused(pressed)
                        }

                        Text {
                            text: Math.round(modelData.level * 100) + "%"
                            color: theme.onSurfaceContainer
                            opacity: 0.6
                            font.family: theme.font
                            font.pixelSize: 10
                            width: 36
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }
        }
    }
}