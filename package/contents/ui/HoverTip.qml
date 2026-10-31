/*
 * SPDX-License-Identifier: MIT
 */

import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

// Stays in the applet window, above the scrolling list. A separate tooltip
// window cannot be positioned on Wayland and lands at the top-left of the screen.
Item {
    id: root

    required property string label
    property bool show: parent ? parent.hovered === true && label.length > 0 : false

    readonly property int gap: 6
    readonly property int maxTextWidth: Kirigami.Units.gridUnit * 16
    // Reading contentY makes the position binding follow the scrolling list.
    readonly property real scrollY: {
        let item = parent
        while (item) {
            if (item.contentY !== undefined && item.contentHeight !== undefined && item.interactive !== undefined)
                return item.contentY
            item = item.parent
        }
        return 0
    }
    readonly property color panelColor: {
        const base = Kirigami.Theme.backgroundColor
        const luma = base.r * 0.3 + base.g * 0.59 + base.b * 0.11
        if (luma < 0.5) {
            return Qt.rgba(Math.min(base.r + 0.18, 1), Math.min(base.g + 0.18, 1), Math.min(base.b + 0.2, 1), 1)
        }
        return Qt.rgba(Math.max(base.r - 0.14, 0), Math.max(base.g - 0.14, 0), Math.max(base.b - 0.14, 0), 1)
    }

    width: 0
    height: 0

    onShowChanged: {
        if (show)
            delay.restart()
        else {
            delay.stop()
            hint.visible = false
        }
    }

    Timer {
        id: delay
        interval: 400
        onTriggered: {
            if (root.show)
                hint.visible = true
        }
    }

    PlasmaComponents.Popup {
        id: hint

        parent: Controls.Overlay.overlay
        popupType: PlasmaComponents.Popup.Item
        modal: false
        focus: false
        padding: Kirigami.Units.smallSpacing + 2
        margins: 0
        closePolicy: PlasmaComponents.Popup.NoAutoClose

        x: {
            const owner = root.parent
            if (!owner || !parent)
                return 0
            const scrolled = root.scrollY
            const origin = owner.mapToItem(parent, 0, scrolled - scrolled)
            let next = origin.x
            if (next + implicitWidth > parent.width - 8)
                next = Math.max(8, parent.width - 8 - implicitWidth)
            return Math.round(Math.max(8, next))
        }
        y: {
            const owner = root.parent
            if (!owner || !parent)
                return 0
            const scrolled = root.scrollY
            const below = owner.mapToItem(parent, 0, owner.height + root.gap + scrolled - scrolled)
            if (below.y + implicitHeight > parent.height - 8)
                return Math.round(owner.mapToItem(parent, 0, -implicitHeight - root.gap).y)
            return Math.round(below.y)
        }

        contentItem: Text {
            text: root.label
            wrapMode: Text.Wrap
            color: Kirigami.Theme.textColor
            font: Kirigami.Theme.smallFont
            width: Math.min(implicitWidth, root.maxTextWidth)
        }

        background: Kirigami.ShadowedRectangle {
            color: root.panelColor
            radius: Kirigami.Units.cornerRadius
            border.width: 1
            border.color: Kirigami.ColorUtils.linearInterpolation(root.panelColor, Kirigami.Theme.textColor, 0.55)
        }
    }
}
