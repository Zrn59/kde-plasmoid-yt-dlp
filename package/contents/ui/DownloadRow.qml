/*
 * SPDX-License-Identifier: MIT
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami

Item {
    id: root

    property int downloadId: -1
    property string title: ""
    property string status: ""
    property string filePath: ""
    property string outputDir: ""
    property string formatLabel: ""
    property string previewFile: ""
    property string errorLog: ""
    property real progress: 0
    property string speed: ""
    property string eta: ""
    property bool failed: false
    property bool paused: false
    property bool previewEnabled: false
    property bool hasPreview: false

    signal cancel()
    signal pause()
    signal resume()
    signal preview()
    signal openFile()
    signal openFolder()

    property bool logExpanded: false

    Layout.fillWidth: true
    width: parent ? parent.width : implicitWidth
    height: col.implicitHeight
    implicitHeight: col.implicitHeight

    readonly property string displayTitle: {
        const base = title || i18n("Downloading…")
        return formatLabel.length > 0 ? (base + " · " + formatLabel) : base
    }
    readonly property string displayPath: {
        if (filePath.length > 0)
            return filePath
        if (outputDir.length > 0)
            return outputDir
        return ""
    }

    ColumnLayout {
        id: col
        width: root.width
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                Layout.alignment: Qt.AlignTop
                source: {
                    if (failed) return "dialog-error"
                    if (paused) return "media-playback-pause"
                    if (progress >= 100) return "emblem-ok-symbolic"
                    if (progress > 0) return "download"
                    return "content-loading-symbolic"
                }
                Layout.preferredWidth: Kirigami.Units.iconSizes.medium
                Layout.preferredHeight: Kirigami.Units.iconSizes.medium
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing / 2

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: root.displayTitle
                    font.bold: true
                    elide: Text.ElideRight
                    maximumLineCount: 2
                    wrapMode: Text.WordWrap
                }

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    visible: root.displayPath.length > 0
                    opacity: 0.55
                    font: Kirigami.Theme.smallFont
                    text: root.displayPath
                    elide: Text.ElideMiddle
                    maximumLineCount: 1
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    PlasmaComponents.ProgressBar {
                        Layout.fillWidth: true
                        from: 0
                        to: 100
                        value: progress
                        indeterminate: progress <= 0 && !failed && !paused
                    }

                    PlasmaComponents.Label {
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 2.2
                        horizontalAlignment: Text.AlignRight
                        text: progress > 0 ? (Math.round(progress) + "%") : ""
                        font.bold: true
                        opacity: paused ? 0.5 : 0.85
                    }
                }

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    visible: status.length > 0 || speed.length > 0
                    opacity: 0.65
                    font: Kirigami.Theme.smallFont
                    text: {
                        let parts = []
                        if (status) parts.push(status)
                        if (!paused && speed) parts.push(speed)
                        if (!paused && eta) parts.push("ETA " + eta)
                        return parts.join(" · ")
                    }
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }
            }

            ColumnLayout {
                Layout.alignment: Qt.AlignTop
                spacing: Kirigami.Units.smallSpacing / 2

                PlasmaComponents.ToolButton {
                    visible: hasPreview
                    icon.name: "media-playback-start"
                    display: PlasmaComponents.AbstractButton.IconOnly
                    onClicked: root.preview()
                    PlasmaComponents.ToolTip.text: previewEnabled
                        ? i18n("Open preview")
                        : i18n("Open file")
                }

                PlasmaComponents.ToolButton {
                    icon.name: "folder-open"
                    display: PlasmaComponents.AbstractButton.IconOnly
                    enabled: displayPath.length > 0
                    onClicked: root.openFolder()
                    PlasmaComponents.ToolTip.text: i18n("Open folder")
                }

                PlasmaComponents.ToolButton {
                    icon.name: logExpanded ? "go-up" : "utilities-terminal"
                    display: PlasmaComponents.AbstractButton.IconOnly
                    enabled: errorLog.length > 0
                    onClicked: root.logExpanded = !root.logExpanded
                    PlasmaComponents.ToolTip.text: i18n("Log")
                }

                PlasmaComponents.ToolButton {
                    enabled: !failed && !paused
                    opacity: enabled ? 1 : 0.35
                    icon.name: "media-playback-pause"
                    display: PlasmaComponents.AbstractButton.IconOnly
                    onClicked: root.pause()
                    PlasmaComponents.ToolTip.text: i18n("Pause")
                }

                PlasmaComponents.ToolButton {
                    enabled: paused
                    opacity: enabled ? 1 : 0.35
                    icon.name: "media-playback-start"
                    display: PlasmaComponents.AbstractButton.IconOnly
                    onClicked: root.resume()
                    PlasmaComponents.ToolTip.text: i18n("Resume")
                }

                Item { Layout.preferredHeight: Kirigami.Units.smallSpacing }

                PlasmaComponents.ToolButton {
                    icon.name: "process-stop"
                    display: PlasmaComponents.AbstractButton.IconOnly
                    onClicked: root.cancel()
                    PlasmaComponents.ToolTip.text: i18n("Cancel")
                }
            }
        }

        PlasmaComponents.TextArea {
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 6
            visible: root.logExpanded && errorLog.length > 0
            readOnly: true
            wrapMode: TextEdit.Wrap
            text: errorLog
            font.family: "monospace"
        }

        Kirigami.Separator {
            Layout.fillWidth: true
            opacity: 0.4
        }
    }
}
