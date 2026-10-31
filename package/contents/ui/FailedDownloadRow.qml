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
    property string url: ""
    property real progress: 0
    property string filePath: ""
    property string outputDir: ""
    property string formatLabel: ""
    property string errorLog: ""

    signal retry()
    signal dismiss()
    signal openFolder()
    signal copyLog()

    property bool logExpanded: false

    readonly property string headline: Options.errorHeadline((errorLog || "") + "\n" + (status || ""))

    Layout.fillWidth: true
    width: parent ? parent.width : implicitWidth
    height: col.implicitHeight
    implicitHeight: col.implicitHeight

    readonly property string displayTitle: {
        const base = title || url
        return formatLabel.length > 0 ? (base + " · " + formatLabel) : base
    }
    readonly property string displayPath: filePath.length > 0 ? filePath : outputDir

    ColumnLayout {
        id: col
        width: root.width
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                Layout.alignment: Qt.AlignTop
                source: "dialog-error"
                Layout.preferredWidth: Kirigami.Units.iconSizes.medium
                Layout.preferredHeight: Kirigami.Units.iconSizes.medium
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

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
                    visible: root.headline.length > 0
                    text: root.headline
                    font.bold: true
                    wrapMode: Text.WordWrap
                    color: Kirigami.Theme.negativeTextColor
                }

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: status
                    wrapMode: Text.WordWrap
                    maximumLineCount: 3
                    opacity: 0.85
                    font: Kirigami.Theme.smallFont
                    color: Kirigami.Theme.negativeTextColor
                }

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    visible: displayPath.length > 0
                    opacity: 0.55
                    font: Kirigami.Theme.smallFont
                    text: displayPath
                    elide: Text.ElideMiddle
                }

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    visible: progress > 0
                    opacity: 0.55
                    font: Kirigami.Theme.smallFont
                    text: i18n("Stopped at %1%", Math.round(progress))
                }
            }

            ColumnLayout {
                Layout.alignment: Qt.AlignTop
                spacing: Kirigami.Units.smallSpacing / 2

                PlasmaComponents.Button {
                    icon.name: "view-refresh"
                    text: i18n("Retry")
                    onClicked: root.retry()
                    HoverTip { label: i18n("Try this download again") }
                }

                PlasmaComponents.ToolButton {
                    icon.name: logExpanded ? "go-up" : "utilities-terminal"
                    display: PlasmaComponents.AbstractButton.IconOnly
                    enabled: errorLog.length > 0
                    onClicked: root.logExpanded = !root.logExpanded
                    HoverTip { label: i18n("Error log") }
                }

                PlasmaComponents.ToolButton {
                    icon.name: "edit-copy"
                    display: PlasmaComponents.AbstractButton.IconOnly
                    enabled: errorLog.length > 0 || status.length > 0
                    onClicked: root.copyLog()
                    HoverTip { label: i18n("Copy error") }
                }

                PlasmaComponents.ToolButton {
                    visible: displayPath.length > 0
                    icon.name: "folder-open"
                    display: PlasmaComponents.AbstractButton.IconOnly
                    onClicked: root.openFolder()
                    HoverTip { label: i18n("Open folder") }
                }

                Item { Layout.preferredHeight: Kirigami.Units.smallSpacing }

                PlasmaComponents.ToolButton {
                    icon.name: "window-close"
                    display: PlasmaComponents.AbstractButton.IconOnly
                    onClicked: root.dismiss()
                    HoverTip { label: i18n("Dismiss") }
                }
            }
        }

        PlasmaComponents.TextArea {
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 8
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
