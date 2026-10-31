/*
 * SPDX-License-Identifier: MIT
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami

import "options.js" as Options

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
    signal copyLog()

    property bool logExpanded: false

    readonly property string headline: Options.errorHeadline(errorLog || "")

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
        if (previewFile.length > 0)
            return previewFile
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
                    visible: root.headline.length > 0
                    text: root.headline
                    wrapMode: Text.WordWrap
                    color: Kirigami.Theme.negativeTextColor
                    font.bold: true
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                }

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
                    HoverTip { label: root.previewEnabled ? i18n("Open preview") : i18n("Open file") }
                }

                PlasmaComponents.ToolButton {
                    icon.name: "folder-open"
                    display: PlasmaComponents.AbstractButton.IconOnly
                    enabled: displayPath.length > 0
                    onClicked: root.openFolder()
                    HoverTip { label: i18n("Open folder") }
                }

                PlasmaComponents.ToolButton {
                    icon.name: logExpanded ? "go-up" : "utilities-terminal"
                    display: PlasmaComponents.AbstractButton.IconOnly
                    enabled: errorLog.length > 0
                    onClicked: root.logExpanded = !root.logExpanded
                    HoverTip { label: i18n("Log") }
                }

                PlasmaComponents.ToolButton {
                    icon.name: "edit-copy"
                    display: PlasmaComponents.AbstractButton.IconOnly
                    enabled: errorLog.length > 0
                    onClicked: root.copyLog()
                    HoverTip { label: i18n("Copy log") }
                }

                PlasmaComponents.ToolButton {
                    enabled: !failed && !paused
                    opacity: enabled ? 1 : 0.35
                    icon.name: "media-playback-pause"
                    display: PlasmaComponents.AbstractButton.IconOnly
                    onClicked: root.pause()
                    HoverTip { label: i18n("Pause") }
                }

                PlasmaComponents.ToolButton {
                    enabled: paused
                    opacity: enabled ? 1 : 0.35
                    icon.name: "media-playback-start"
                    display: PlasmaComponents.AbstractButton.IconOnly
                    onClicked: root.resume()
                    HoverTip { label: i18n("Resume") }
                }

                Item { Layout.preferredHeight: Kirigami.Units.smallSpacing }

                PlasmaComponents.ToolButton {
                    icon.name: "process-stop"
                    display: PlasmaComponents.AbstractButton.IconOnly
                    onClicked: root.cancel()
                    HoverTip { label: i18n("Cancel") }
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
