/*
 * SPDX-License-Identifier: MIT
 */

import QtQuick
import org.kde.plasma.configuration

ConfigModel {
    ConfigCategory {
        name: i18nc("@title", "General")
        icon: "settings"
        source: "configGeneral.qml"
    }
}
