/*
 * SPDX-License-Identifier: MIT
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami

RowLayout {
    id: root

    required property var model
    required property int currentValue
    signal activated(int value)

    spacing: Kirigami.Units.smallSpacing

    PlasmaComponents.ComboBox {
        id: combo
        Layout.fillWidth: true
        model: root.model
        textRole: "label"

        Component.onCompleted: syncIndex()

        function syncIndex() {
            for (let i = 0; i < root.model.length; ++i) {
                if (root.model[i].value === root.currentValue) {
                    currentIndex = i
                    return
                }
            }
        }

        onActivated: (index) => {
            if (index >= 0 && index < root.model.length) {
                root.activated(root.model[index].value)
            }
        }

        Connections {
            target: root
            function onCurrentValueChanged() {
                combo.syncIndex()
            }
        }
    }
}
