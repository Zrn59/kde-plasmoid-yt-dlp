/*
 * SPDX-License-Identifier: MIT
 */

pragma ComponentBehavior: Bound

import QtQuick
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami

import "../qml/com/zrn59/ytdlp"

PlasmoidItem {
    id: root

    switchWidth: Kirigami.Units.gridUnit * 20
    switchHeight: Kirigami.Units.gridUnit * 24

    YtdlpBackend {
        id: backend
    }

    readonly property bool hasActiveDownloads: backend.activeCount > 0

    readonly property url plasmoidIcon: Qt.resolvedUrl("yt-dlp.svg")

    toolTipMainText: i18n("kde-plasmoid-yt-dlp")
    toolTipSubText: hasActiveDownloads
        ? i18ncp("@info:tooltip", "1 active download", "%1 active downloads", backend.activeCount)
        : i18nc("@info:tooltip", "Download video and audio")

    Plasmoid.icon: hasActiveDownloads ? "folder-download-symbolic" : "folder-download"
    Plasmoid.status: hasActiveDownloads ? PlasmaCore.Types.ActiveStatus : PlasmaCore.Types.PassiveStatus

    compactRepresentation: Item {
        implicitWidth: Kirigami.Units.iconSizes.medium
        implicitHeight: Kirigami.Units.iconSizes.medium

        Kirigami.Icon {
            anchors.fill: parent
            source: root.plasmoidIcon
            fallback: "folder-download"
            smooth: true
        }
        MouseArea {
            anchors.fill: parent
            onClicked: root.expanded = !root.expanded
        }
    }

    fullRepresentation: FullRepresentation {
        backend: backend
    }
}
