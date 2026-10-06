import QtQuick

Rectangle {
    id: indicators

    property var state: indicatorsService.state
    property color textColor: theme.barTextColor

    // No battery on a desktop, so the row collapses to the volume icon alone.
    readonly property bool hasBattery: !!(state.battery && state.battery.present)

    // Shared icon metrics. Every icon in every module uses these same numbers,
    // which is what makes them line up: glyphs from one font sit on the font's
    // baseline, and a baseline only lands at a predictable y when the cell
    // height, the pixel size and the vertical alignment all agree.
    readonly property int iconCell: 22
    readonly property int iconSize: 19

    // The android battery glyph is a wide, flat shape: at the same pixel size
    // as a speaker or wifi glyph it only covers about two thirds of their height,
    // so it reads as noticeably smaller. It gets a bigger pixel size to match
    // them optically, and a wider cell to hold the extra width.
    readonly property int batterySize: 25
    readonly property int batteryCell: 29

    // Width must match its content exactly. A wider box leaves dead space that
    // the centring Row adds on both sides, which shows up as an oversized gap to
    // the neighbouring module.
    width: hasBattery ? batteryCell + 6 + iconCell : iconCell
    height: parent.height
    color: "transparent"
    radius: height / 2

    // Verified Material Symbols codepoints: volume_up "\uE050", volume_down
    // "\uE04D", volume_off "\uE04F".
    readonly property string volumeIcon: volumeService.masterMuted ? "\uE04F"
                                    : volumeService.masterLevel < 0.35 ? "\uE04D"
                                    : "\uE050"

    // --- battery state -----------------------------------------------------
    readonly property var battery: state.battery || ({})

    // Below this the icon turns red whatever else is true. Checked before the
    // glyph is chosen so it overrides the charging, saver and holding variants:
    // a battery this low is the story, whatever else the system is doing.
    readonly property bool low: battery.present && battery.percent < 20

    // On AC power but not filling: a charge limit or smart-charge feature has
    // paused it at full. Still plugged in, so the status stays "Plugged In".
    readonly property bool holding: !!(battery.holding)
    readonly property bool charging: !!(battery.charging)
    readonly property bool saver: !!(battery.saver)

    // Six frame levels for the plain case. The + and bolt families have no
    // fill variants, which is why they stand in for the low-charge states.
    //
    // Power saving is tested before holding on purpose. Holding is true whenever
    // the battery sits at 100% on AC, because Windows stops filling there by
    // itself - so with holding first it shadowed the saver variants almost
    // always and power saving never reached the bar.
    readonly property string batteryIcon:
        saver ? (low ? "\uF303" : "\uF24E")
        : holding ? "\uF24B"
        : charging ? (low ? "\uF305" : "\uF250")
        : battery.percent >= 100 ? "\uF24F"
        : battery.percent >= 83 ? "\uF252"
        : battery.percent >= 66 ? "\uF253"
        : battery.percent >= 50 ? "\uF254"
        : battery.percent >= 33 ? "\uF255"
        : battery.percent >= 16 ? "\uF256"
        : "\uF257"

    // Red when low, otherwise the bar's own adaptive colour. Power saving
    // deliberately does not tint the icon any more - the glyph itself already
    // carries a plus, and tinting it on top made both signals hard to read.
    readonly property color batteryColor: low ? theme.error : textColor

    Row {
        anchors.centerIn: parent
        spacing: 6

        Item {
            id: batterySlot
            visible: indicators.hasBattery
            width: indicators.batteryCell
            height: indicators.iconCell

            Text {
                anchors.fill: parent
                text: indicators.batteryIcon
                color: indicators.batteryColor
                Behavior on color { ColorAnimation { duration: 300 } }
                font.family: iconFont
                font.pixelSize: indicators.batterySize
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            // Hover opens the readout: percentage, charging and battery saver.
            // None of those fit in a 40px bar, and the icon alone cannot show
            // them. It has to be a separate window, since anything drawn below
            // the icon in the bar's own QML tree lands outside the window rect
            // and is clipped away.
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                // Guarded on which popup is open rather than on whether one is:
                // see the note on the volume icon below. Skipping the open while
                // another tooltip was still visible left the pointer inside this
                // hover area with no further onEntered, so nothing ever appeared.
                onEntered: {
                    popupHost.anchor = batterySlot;
                    if (popupHost.currentUrl !== "qrc:/qml/popups/BatteryPopup.qml")
                        popupHost.open("qrc:/qml/popups/BatteryPopup.qml")
                }
                onExited: popupHost.closeIfTransient()
            }
        }

        Text {
            id: volumeIcon
            width: indicators.iconCell
            height: indicators.iconCell
            text: indicators.volumeIcon
            color: indicators.textColor
            Behavior on color { ColorAnimation { duration: 300 } }
            font.family: iconFont
            font.pixelSize: indicators.iconSize
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                // Hover gives the level readout; click gives the full mixer.
                //
                // The guard is on which popup is open, not on whether one is. A
                // plain "if (!popupHost.isOpen)" skipped the open whenever another
                // tooltip happened to be up: sliding from the battery straight onto
                // this icon while that one was still fading left the pointer
                // already inside here with onEntered long gone, so nothing appeared
                // until the pointer left and came back.
                //
                // Checking the URL is what makes both cases behave. Leaving the
                // mixer with the pointer still over the icon re-enters without a
                // movement event, and it must not spring the tooltip back up.
                onEntered: {
                    popupHost.anchor = volumeIcon;
                    if (popupHost.currentUrl !== "qrc:/qml/popups/VolumeLevelPopup.qml")
                        popupHost.open("qrc:/qml/popups/VolumeLevelPopup.qml")
                }
                // closeIfTransient, not close: the mixer is interactive, and
                // reaching for a slider starts by leaving the icon.
                onExited: popupHost.closeIfTransient()
                onClicked: {
                    popupHost.anchor = volumeIcon;
                    // interactive = true. Without it the popup is treated as a
                    // readout and closes the moment the pointer leaves the icon on
                    // its way down to the controls.
                    popupHost.open("qrc:/qml/popups/VolumePopup.qml", true)
                }
            }
        }
    }
}
