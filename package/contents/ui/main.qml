/*
 * SPDX-License-Identifier: MIT
 */

pragma ComponentBehavior: Bound

import QtQuick
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.draganddrop as DragDrop
import org.kde.kirigami as Kirigami

import "../qml/com/zrn59/ytdlp"

PlasmoidItem {
    id: root

    switchWidth: Kirigami.Units.gridUnit * 20
    switchHeight: Kirigami.Units.gridUnit * 24

    property bool iconDrag: false
    property bool popupDrag: false
    property bool openedByDrag: false

    YtdlpBackend {
        id: backend
    }

    readonly property bool hasActiveDownloads: backend.activeCount > 0
    readonly property url plasmoidIcon: Qt.resolvedUrl("yt-dlp.svg")

    function linkFromEvent(event) {
        if (!event || !event.mimeData)
            return ""
        return backend.httpUrlFromMime(event.mimeData)
    }

    function syncDrag() {
        if (iconDrag || popupDrag) {
            hideOnWindowDeactivate = false
            dragReleaseTimer.stop()
            if (!expanded)
                dragOpenTimer.restart()
            return
        }
        dragOpenTimer.stop()
        dragReleaseTimer.restart()
    }

    function releaseDrag() {
        if (iconDrag || popupDrag)
            return
        hideOnWindowDeactivate = true
        if (openedByDrag) {
            openedByDrag = false
            expanded = false
        }
    }

    property string queuedUrl: ""

    function acceptDroppedUrl(link) {
        if (!link)
            return
        dragOpenTimer.stop()
        dragReleaseTimer.stop()
        openedByDrag = false
        expanded = true
        queuedUrl = ""
        queuedUrl = link
        dropSettleTimer.restart()
    }

    Timer {
        id: dragOpenTimer
        interval: 450
        onTriggered: {
            if (root.iconDrag || root.popupDrag) {
                if (!root.expanded)
                    root.openedByDrag = true
                root.hideOnWindowDeactivate = false
                root.expanded = true
            }
        }
    }

    Timer {
        id: dragReleaseTimer
        interval: 600
        onTriggered: root.releaseDrag()
    }

    Timer {
        id: dropSettleTimer
        interval: 300
        onTriggered: {
            if (root.iconDrag || root.popupDrag)
                return
            root.hideOnWindowDeactivate = true
        }
    }

    toolTipMainText: i18n("kde-plasmoid-yt-dlp")
    toolTipSubText: hasActiveDownloads
        ? i18ncp("@info:tooltip", "1 active download", "%1 active downloads", backend.activeCount)
        : i18nc("@info:tooltip", "Download video and audio")

    Plasmoid.icon: hasActiveDownloads ? "folder-download-symbolic" : "folder-download"
    Plasmoid.status: hasActiveDownloads ? PlasmaCore.Types.ActiveStatus : PlasmaCore.Types.PassiveStatus

    compactRepresentation: DragDrop.DropArea {
        id: iconDrop
        preventStealing: true
        implicitWidth: Kirigami.Units.iconSizes.medium
        implicitHeight: Kirigami.Units.iconSizes.medium

        onContainsDragChanged: {
            root.iconDrag = containsDrag
            root.syncDrag()
        }

        onDrop: event => {
            const link = root.linkFromEvent(event)
            event.accept(Qt.CopyAction)
            root.acceptDroppedUrl(link)
        }

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

    fullRepresentation: DragDrop.DropArea {
        id: popupDrop
        preventStealing: true
        implicitWidth: Kirigami.Units.gridUnit * 26
        implicitHeight: Kirigami.Units.gridUnit * 32

        onContainsDragChanged: {
            root.popupDrag = containsDrag
            root.syncDrag()
        }

        onDrop: event => {
            const link = root.linkFromEvent(event)
            event.accept(Qt.CopyAction)
            root.acceptDroppedUrl(link)
        }

        FullRepresentation {
            id: page
            anchors.fill: parent
            backend: backend
            queuedUrl: root.queuedUrl
            dropReady: root.popupDrag
        }
    }
}
