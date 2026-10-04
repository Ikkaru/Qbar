import QtQuick

Rectangle {
    id: graph
    width: 96
    height: parent.height
    color: "transparent"
    radius: height / 2

    // Bar text colour follows the wallpaper behind the bar. Animated so a
    // wallpaper switch fades instead of snapping.
    property color textColor: theme.barTextColor
    Behavior on textColor { ColorAnimation { duration: 300 } }

    // Canvas cannot animate a stroke, so the ring jumps to the new colour
    // directly rather than fading.
    property color arcColor: theme.barTextColor
    property string trackColor: Qt.rgba(arcColor.r, arcColor.g, arcColor.b, 0.25)

    // One icon + progress ring per metric. No text at rest; hover reveals detail.
    readonly property var metrics: [
        { key: "cpu", icon: "\uE30D", label: "CPU" },
        { key: "ram", icon: "\uE322", label: "RAM" },
        { key: "net", icon: "\uE1AF", label: "NETWORK" }
    ]

    Row {
        anchors.centerIn: parent
        anchors.horizontalCenterOffset: 18
        spacing: 6

        Repeater {
            model: graph.metrics
            delegate: Item {
                id: metricItem
                width: 26
                height: 26

                property string label: modelData.label
                property string icon: modelData.icon
                property double value: {
                    const key = modelData.key;
                    if (key === "cpu") return graphService.cpu;
                    if (key === "ram") return graphService.ram;
                    return graphService.net;
                }

                onValueChanged: ring.requestPaint()

                // The ring is Canvas-drawn, so a plain colour binding is not
                // enough: it has to be re-painted when the text colour changes.
                Connections {
                    target: contrastService
                    function onChanged() { ring.requestPaint(); }
                }

                // Canvas must match the delegate Item exactly. At 34 inside a 30 Item it
                    // overflowed the parent and clipped against the next ring.
                    Canvas {
                    id: ring
                    width: 26
                    height: 26
                    antialiasing: true

                    onPaint: {
                        const ctx = getContext("2d");
                        ctx.reset();
                        ctx.clearRect(0, 0, width, height);

                        const cx = width / 2;
                        const cy = height / 2;
                        const radius = width / 2 - 1.5;

                        // Track
                        ctx.beginPath();
                        ctx.arc(cx, cy, radius, 0, Math.PI * 2);
                        ctx.strokeStyle = graph.trackColor;
                        ctx.lineWidth = 2;
                        ctx.stroke();

                        // Value arc from 12 o'clock
                        const pct = Math.max(0, Math.min(1, metricItem.value / 100.0));
                        if (pct > 0) {
                            ctx.beginPath();
                            ctx.arc(cx, cy, radius, -Math.PI / 2, -Math.PI / 2 + pct * Math.PI * 2);
                            ctx.strokeStyle = graph.arcColor;
                            ctx.lineWidth = 2;
                            ctx.lineCap = "round";
                            ctx.stroke();
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: metricItem.icon
                    color: graph.textColor
                    font.family: iconFont
                    font.pixelSize: 12
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: {
                        graphTip.dLabel = metricItem.label;
                        graphTip.dValue = Math.round(metricItem.value) + "%";
                        graphTip.dDetail = metricItem.label === "CPU"
                            ? graphService.cpuCores + " cores"
                            : metricItem.label === "RAM"
                            ? graphService.ramUsed + " / " + graphService.ramTotal
                            : graphService.netDown + " / " + graphService.netUp;
                        graphTip.visible = true;
                        graphTip.opacity = 1;
                    }
                    onExited: {
                        graphTip.opacity = 0;
                        graphTip.visible = false;
                    }
                }
            }
        }
    }

    // Hover detail tooltip
    Rectangle {
        id: graphTip
        width: 150
        height: 56
        color: theme.surfaceContainer
        radius: 8
        border.width: 1
        border.color: theme.outline
        visible: false
        opacity: 0
        anchors.top: parent.bottom
        anchors.topMargin: 6
        anchors.horizontalCenter: parent.horizontalCenter

        Behavior on opacity { NumberAnimation { duration: 120 } }

        property string dLabel: ""
        property string dValue: ""
        property string dDetail: ""

        Column {
            anchors.centerIn: parent
            spacing: 2

            Text {
                text: graphTip.dLabel
                color: theme.onSurfaceContainer
                font.family: theme.font
                font.pixelSize: 9
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
            }
            Text {
                text: graphTip.dValue
                color: theme.onSurfaceContainer
                font.family: theme.font
                font.pixelSize: 13
                font.weight: Font.Medium
            }
            Text {
                text: graphTip.dDetail
                color: theme.onSurfaceContainer
                opacity: 0.6
                font.family: theme.font
                font.pixelSize: 9
                width: graphTip.width - 16
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }
        }
    }
}