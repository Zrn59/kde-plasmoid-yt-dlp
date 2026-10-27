/*
 * SPDX-License-Identifier: MIT
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs

import org.kde.plasma.plasmoid
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami

import "../qml/com/zrn59/ytdlp"

import "options.js" as Options

PlasmaComponents.Page {
    id: page

    required property YtdlpBackend backend

    readonly property var cfg: plasmoid.configuration

    implicitWidth: Kirigami.Units.gridUnit * 26
    implicitHeight: Kirigami.Units.gridUnit * 32

    property string url: ""
    property bool previewMode: cfg.previewMode
    property int downloadMode: Number(cfg.downloadMode)
    property int videoQuality: Number(cfg.videoQuality)
    property int videoCodec: Number(cfg.videoCodec)
    property int videoContainer: Number(cfg.videoContainer)
    property int audioQuality: Number(cfg.audioQuality)
    property int audioCodec: Number(cfg.audioCodec)
    property int subtitleOption: Number(cfg.subtitleOption)
    property int subtitleLanguage: Number(cfg.subtitleLanguage)
    property int subtitleFormat: Number(cfg.subtitleFormat)
    property int speedLimit: Number(cfg.speedLimit)
    property bool embedMetadata: cfg.embedMetadata
    property bool embedThumbnail: cfg.embedThumbnail
    property bool allowPlaylist: cfg.allowPlaylist
    property bool showOptions: true
    property bool showAdvanced: cfg.showAdvanced
    property bool useCookies: cfg.useCookies
    property string cookiesFile: cfg.cookiesFile || backend.defaultCookiesFile()
    property string videoFolder: cfg.videoFolder || backend.defaultVideoFolder()
    property string audioFolder: cfg.audioFolder || backend.defaultAudioFolder()
    property string customArgs: cfg.customArgs || ""
    property string downloadSections: cfg.downloadSections || ""
    property string lastDownloadPath: ""
    property string successMessage: ""

    function saveConfig() {
        cfg.downloadMode = downloadMode
        cfg.videoQuality = videoQuality
        cfg.videoCodec = videoCodec
        cfg.videoContainer = videoContainer
        cfg.audioQuality = audioQuality
        cfg.audioCodec = audioCodec
        cfg.subtitleOption = subtitleOption
        cfg.subtitleLanguage = subtitleLanguage
        cfg.subtitleFormat = subtitleFormat
        cfg.speedLimit = speedLimit
        cfg.embedMetadata = embedMetadata
        cfg.embedThumbnail = embedThumbnail
        cfg.allowPlaylist = allowPlaylist
        cfg.previewMode = previewMode
        cfg.useCookies = useCookies
        cfg.cookiesFile = cookiesFile
        cfg.showAdvanced = showAdvanced
        cfg.videoFolder = videoFolder
        cfg.audioFolder = audioFolder
        cfg.customArgs = customArgs
        cfg.downloadSections = downloadSections
    }

    function currentFolder() {
        return downloadMode === 2 ? audioFolder : videoFolder
    }

    function buildOptions() {
        return {
            url: url,
            outputDir: currentFolder(),
            downloadMode: downloadMode,
            videoQuality: videoQuality,
            videoCodec: videoCodec,
            videoContainer: videoContainer,
            audioQuality: audioQuality,
            audioCodec: audioCodec,
            subtitleOption: subtitleOption,
            subtitleLanguage: subtitleLanguage,
            subtitleFormat: subtitleFormat,
            speedLimit: speedLimit,
            embedMetadata: embedMetadata,
            embedThumbnail: embedThumbnail,
            allowPlaylist: allowPlaylist,
            previewMode: previewMode,
            useCookies: useCookies,
            cookiesFile: cookiesFile,
            customArgs: customArgs,
            downloadSections: downloadSections,
        }
    }

    function pathToUrl(path) {
        if (!path || path.length === 0)
            return ""
        const parts = path.split("/")
        let encoded = ""
        for (let i = 0; i < parts.length; ++i) {
            if (parts[i].length === 0)
                continue
            encoded += "/" + encodeURIComponent(parts[i])
        }
        return "file://" + encoded
    }

    function folderUrlToPath(folderUrl) {
        let s = folderUrl.toString()
        if (s.startsWith("file://")) {
            s = s.substring(7)
            if (s.startsWith("localhost/"))
                s = s.substring(9)
        }
        try {
            return decodeURIComponent(s)
        } catch (e) {
            return s
        }
    }

    function startDownload() {
        if (!url.trim()) {
            backend.setError(i18n("Enter a URL"))
            return
        }
        successMessage = ""
        if (backend.startDownload(buildOptions())) {
            url = ""
            urlField.text = ""
            saveConfig()
        }
    }

    Component.onCompleted: {
        if (!cfg.cookiesFile) {
            cookiesFile = backend.defaultCookiesFile()
        }
    }

    Connections {
        target: backend
        function onDownloadFinished(id, success, title, filePath) {
            if (success && filePath) {
                lastDownloadPath = filePath
                successMessage = i18n("Saved: %1", filePath.split("/").pop())
            }
        }
        function onLastErrorChanged() {
            if (backend.lastError.length > 0) {
                successMessage = ""
            }
        }
    }

    FolderDialog {
        id: folderDialog
        title: i18nc("@title:window", "Select download folder")
        onAccepted: {
            const path = page.folderUrlToPath(selectedFolder)
            if (path.length === 0) {
                return
            }
            if (downloadMode === 2) {
                audioFolder = path
            } else {
                videoFolder = path
            }
            saveConfig()
        }
    }

    Timer {
        id: successTimer
        interval: 8000
        onTriggered: successMessage = ""
    }

    onSuccessMessageChanged: if (successMessage.length > 0) successTimer.restart()

    PlasmaComponents.ScrollView {
        id: scroll
        anchors.fill: parent
        focus: true
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            id: mainLayout
            width: scroll.availableWidth
            spacing: Kirigami.Units.smallSpacing

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.gridUnit
                spacing: Kirigami.Units.smallSpacing

                Image {
                    source: Qt.resolvedUrl("yt-dlp.svg")
                    sourceSize: Qt.size(Kirigami.Units.iconSizes.large, Kirigami.Units.iconSizes.large)
                    width: Kirigami.Units.iconSizes.large
                    height: Kirigami.Units.iconSizes.large
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    asynchronous: true
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    PlasmaComponents.Label {
                        text: i18n("yt-dlp")
                        font.bold: true
                    }

                    PlasmaComponents.Label {
                        opacity: 0.65
                        font: Kirigami.Theme.smallFont
                        text: {
                            if (backend.updatingYtdlp)
                                return i18n("Updating…")
                            if (backend.ytdlpVersion.length === 0)
                                return i18n("Checking…")
                            if (backend.ytdlpPath.length === 0)
                                return backend.ytdlpVersion
                            return i18n("v%1", backend.ytdlpVersion)
                        }
                        PlasmaComponents.ToolTip.text: backend.ytdlpPath.length > 0
                            ? backend.ytdlpPath
                            : i18n("yt-dlp is not installed. Click refresh to download it.")
                    }
                }

                PlasmaComponents.ToolButton {
                    icon.name: "view-refresh"
                    display: PlasmaComponents.AbstractButton.IconOnly
                    enabled: !backend.updatingYtdlp
                    onClicked: backend.updateYtdlp()
                    PlasmaComponents.ToolTip.text: i18n("Update yt-dlp")
                }
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.gridUnit
                Layout.rightMargin: Kirigami.Units.gridUnit
                visible: backend.lastError.length > 0
                type: Kirigami.MessageType.Error
                text: backend.lastError
                actions: [
                    Kirigami.Action {
                        icon.name: "window-close"
                        text: i18n("Dismiss")
                        onTriggered: backend.clearError()
                    }
                ]
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.gridUnit
                Layout.rightMargin: Kirigami.Units.gridUnit
                visible: successMessage.length > 0 && lastDownloadPath.length === 0
                type: Kirigami.MessageType.Positive
                text: successMessage
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.gridUnit
                Layout.rightMargin: Kirigami.Units.gridUnit
                visible: successMessage.length > 0 && lastDownloadPath.length > 0
                type: Kirigami.MessageType.Positive
                text: successMessage
                actions: [
                    Kirigami.Action {
                        icon.name: "folder-open"
                        text: i18n("Open")
                        onTriggered: backend.openFolder(lastDownloadPath)
                    }
                ]
            }

            Kirigami.Separator {
                Layout.fillWidth: true
            }

            // URL + download
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.gridUnit
                Layout.rightMargin: Kirigami.Units.gridUnit
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    PlasmaComponents.TextField {
                        id: urlField
                        Layout.fillWidth: true
                        placeholderText: i18n("Paste link…")
                        onTextChanged: page.url = text
                        onAccepted: page.startDownload()
                    }

                    PlasmaComponents.ToolButton {
                        icon.name: "edit-paste"
                        display: PlasmaComponents.AbstractButton.IconOnly
                        onClicked: {
                            const clip = backend.pasteFromClipboard()
                            if (backend.isValidUrl(clip)) {
                                urlField.text = clip
                            } else if (clip.length > 0) {
                                backend.setError(i18n("Clipboard does not contain a valid URL"))
                            }
                        }
                        PlasmaComponents.ToolTip.text: i18n("Paste")
                    }

                    PlasmaComponents.Button {
                        icon.name: "download"
                        text: i18n("Go")
                        enabled: url.trim().length > 0 && backend.activeCount < 5
                        onClicked: page.startDownload()
                    }
                }

                    PlasmaComponents.TabBar {
                    id: modeBar
                    Layout.fillWidth: true
                    currentIndex: {
                        for (let i = 0; i < Options.downloadModes.length; ++i) {
                            if (Options.downloadModes[i].value === page.downloadMode)
                                return i
                        }
                        return 0
                    }
                    onCurrentIndexChanged: {
                        const mode = Options.downloadModes[currentIndex]
                        if (!mode || page.downloadMode === mode.value)
                            return
                        page.downloadMode = mode.value
                        page.saveConfig()
                    }

                    Repeater {
                        model: Options.downloadModes
                        delegate: PlasmaComponents.TabButton {
                            required property var modelData
                            text: modelData.label
                            icon.name: modelData.icon
                        }
                    }
                }
            }

            Kirigami.Separator {
                Layout.fillWidth: true
            }

            // Quick options
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.gridUnit
                Layout.rightMargin: Kirigami.Units.gridUnit
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    PlasmaExtras.Heading {
                        Layout.fillWidth: true
                        text: i18n("Options")
                        level: 3
                    }
                    PlasmaComponents.ToolButton {
                        icon.name: showOptions ? "go-up" : "go-down"
                        display: PlasmaComponents.AbstractButton.IconOnly
                        onClicked: showOptions = !showOptions
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: Kirigami.Units.gridUnit
                    rowSpacing: Kirigami.Units.smallSpacing
                    visible: showOptions

                    PlasmaComponents.Label {
                        visible: downloadMode !== 2
                        text: i18n("Quality")
                        opacity: 0.7
                        Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                    }
                    OptionCombo {
                        visible: downloadMode !== 2
                        Layout.fillWidth: true
                        model: Options.videoQualities
                        currentValue: videoQuality
                        onActivated: (value) => { videoQuality = value; saveConfig() }
                    }

                    PlasmaComponents.Label {
                        visible: downloadMode !== 1
                        text: i18n("Audio")
                        opacity: 0.7
                        Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                    }
                    OptionCombo {
                        visible: downloadMode !== 1
                        Layout.fillWidth: true
                        model: Options.audioQualities
                        currentValue: audioQuality
                        onActivated: (value) => { audioQuality = value; saveConfig() }
                    }

                    PlasmaComponents.Label {
                        visible: downloadMode !== 2
                        text: i18n("Format")
                        opacity: 0.7
                        Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                    }
                    OptionCombo {
                        visible: downloadMode !== 2
                        Layout.fillWidth: true
                        model: Options.videoContainers
                        currentValue: videoContainer
                        onActivated: (value) => { videoContainer = value; saveConfig() }
                    }

                    PlasmaComponents.Label {
                        visible: downloadMode !== 1
                        text: i18n("Codec")
                        opacity: 0.7
                        Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                    }
                    OptionCombo {
                        visible: downloadMode !== 1
                        Layout.fillWidth: true
                        model: Options.audioCodecs
                        currentValue: audioCodec
                        onActivated: (value) => { audioCodec = value; saveConfig() }
                    }

                    PlasmaComponents.Label {
                        text: i18n("Folder")
                        opacity: 0.7
                        Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        PlasmaComponents.Label {
                            Layout.fillWidth: true
                            text: currentFolder()
                            elide: Text.ElideMiddle
                            font: Kirigami.Theme.smallFont
                        }

                        PlasmaComponents.ToolButton {
                            icon.name: "folder-open"
                            display: PlasmaComponents.AbstractButton.IconOnly
                            onClicked: {
                                const folder = page.pathToUrl(page.currentFolder())
                                if (folder.length > 0)
                                    folderDialog.currentFolder = folder
                                folderDialog.open()
                            }
                            PlasmaComponents.ToolTip.text: i18n("Browse")
                        }
                    }

                    PlasmaComponents.CheckBox {
                        Layout.columnSpan: 2
                        Layout.leftMargin: Kirigami.Units.gridUnit * 2
                        visible: downloadMode !== 2
                        text: i18n("Watch while downloading")
                        checked: previewMode
                        onToggled: { previewMode = checked; saveConfig() }
                    }

                    PlasmaComponents.Label {
                        Layout.columnSpan: 2
                        Layout.leftMargin: Kirigami.Units.gridUnit * 2
                        visible: previewMode && downloadMode !== 2
                        wrapMode: Text.WordWrap
                        opacity: 0.55
                        font: Kirigami.Theme.smallFont
                        text: i18n("Video and audio download in parallel; a .watch.mkv preview is rebuilt every few seconds. Press ▶ to open in your default player.")
                    }

                    PlasmaComponents.CheckBox {
                        Layout.columnSpan: 2
                        Layout.leftMargin: Kirigami.Units.gridUnit * 2
                        text: i18n("Use cookies (YouTube)")
                        checked: useCookies
                        onToggled: { useCookies = checked; saveConfig() }
                    }

                    RowLayout {
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        Layout.leftMargin: Kirigami.Units.gridUnit * 2
                        visible: useCookies
                        spacing: Kirigami.Units.smallSpacing

                        PlasmaComponents.Label {
                            Layout.fillWidth: true
                            text: cookiesFile
                            elide: Text.ElideMiddle
                            font: Kirigami.Theme.smallFont
                        }

                        PlasmaComponents.ToolButton {
                            icon.name: "document-open"
                            display: PlasmaComponents.AbstractButton.IconOnly
                            onClicked: {
                                const path = backend.selectCookiesFile(cookiesFile)
                                if (path) {
                                    cookiesFile = path
                                    saveConfig()
                                }
                            }
                        }
                    }

                    PlasmaComponents.ToolButton {
                        Layout.columnSpan: 2
                        Layout.leftMargin: Kirigami.Units.gridUnit * 2
                        icon.name: showAdvanced ? "go-up" : "go-down"
                        text: i18n("Advanced")
                        display: PlasmaComponents.AbstractButton.TextBesideIcon
                        onClicked: showAdvanced = !showAdvanced
                    }

                    GridLayout {
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        columns: 2
                        visible: showAdvanced
                        columnSpacing: Kirigami.Units.gridUnit
                        rowSpacing: Kirigami.Units.smallSpacing

                        PlasmaComponents.Label {
                            text: i18n("Video codec")
                            opacity: 0.7
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                        }
                        OptionCombo {
                            Layout.fillWidth: true
                            model: Options.videoCodecs
                            currentValue: videoCodec
                            onActivated: (v) => { videoCodec = v; saveConfig() }
                        }

                        PlasmaComponents.Label {
                            text: i18n("Subtitles")
                            opacity: 0.7
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                        }
                        OptionCombo {
                            Layout.fillWidth: true
                            model: Options.subtitleOptions
                            currentValue: subtitleOption
                            onActivated: (v) => { subtitleOption = v; saveConfig() }
                        }

                        PlasmaComponents.Label {
                            visible: subtitleOption !== 0
                            text: i18n("Language")
                            opacity: 0.7
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                        }
                        OptionCombo {
                            visible: subtitleOption !== 0
                            Layout.fillWidth: true
                            model: Options.subtitleLanguages
                            currentValue: subtitleLanguage
                            onActivated: (v) => { subtitleLanguage = v; saveConfig() }
                        }

                        PlasmaComponents.Label {
                            visible: subtitleOption !== 0
                            text: i18n("Sub format")
                            opacity: 0.7
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                        }
                        OptionCombo {
                            visible: subtitleOption !== 0
                            Layout.fillWidth: true
                            model: Options.subtitleFormats
                            currentValue: subtitleFormat
                            onActivated: (v) => { subtitleFormat = v; saveConfig() }
                        }

                        PlasmaComponents.Label {
                            text: i18n("Speed limit")
                            opacity: 0.7
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                        }
                        OptionCombo {
                            Layout.fillWidth: true
                            model: Options.speedLimits
                            currentValue: speedLimit
                            onActivated: (v) => { speedLimit = v; saveConfig() }
                        }

                        PlasmaComponents.CheckBox {
                            Layout.columnSpan: 2
                            Layout.leftMargin: Kirigami.Units.gridUnit * 2
                            text: i18n("Embed metadata")
                            checked: embedMetadata
                            onToggled: { embedMetadata = checked; saveConfig() }
                        }
                        PlasmaComponents.CheckBox {
                            Layout.columnSpan: 2
                            Layout.leftMargin: Kirigami.Units.gridUnit * 2
                            text: i18n("Embed thumbnail")
                            checked: embedThumbnail
                            onToggled: { embedThumbnail = checked; saveConfig() }
                        }
                        PlasmaComponents.CheckBox {
                            Layout.columnSpan: 2
                            Layout.leftMargin: Kirigami.Units.gridUnit * 2
                            text: i18n("Download playlist")
                            checked: allowPlaylist
                            onToggled: { allowPlaylist = checked; saveConfig() }
                        }

                        PlasmaComponents.Label {
                            text: i18n("Clip")
                            opacity: 0.7
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                        }
                        PlasmaComponents.TextField {
                            Layout.fillWidth: true
                            placeholderText: i18n("10:00-15:00")
                            text: downloadSections
                            onTextChanged: downloadSections = text
                            onEditingFinished: saveConfig()
                        }

                        PlasmaComponents.Label {
                            Layout.columnSpan: 2
                            Layout.leftMargin: Kirigami.Units.gridUnit * 2
                            wrapMode: Text.WordWrap
                            opacity: 0.55
                            font: Kirigami.Theme.smallFont
                            text: i18n("Optional time range. You can write 10:00-15:00 — * is added automatically.")
                        }

                        PlasmaComponents.Label {
                            text: i18n("Extra args")
                            opacity: 0.7
                            Layout.alignment: Qt.AlignTop | Qt.AlignRight
                        }
                        PlasmaComponents.TextField {
                            Layout.fillWidth: true
                            placeholderText: i18n("--sponsorblock-remove all")
                            text: customArgs
                            onTextChanged: customArgs = text
                            onEditingFinished: saveConfig()
                        }

                        PlasmaComponents.Label {
                            Layout.columnSpan: 2
                            Layout.leftMargin: Kirigami.Units.gridUnit * 2
                            wrapMode: Text.WordWrap
                            opacity: 0.55
                            font: Kirigami.Theme.smallFont
                            text: i18n("Raw yt-dlp flags. The global yt-dlp config is not used. Dangerous options (-o, --exec, …) are ignored.")
                        }
                    }
                }
            }

            // Active downloads
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.gridUnit
                Layout.rightMargin: Kirigami.Units.gridUnit
                visible: backend.activeCount > 0
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    PlasmaExtras.Heading {
                        Layout.fillWidth: true
                        text: i18ncp("@info", "1 download", "%1 downloads", backend.activeCount)
                        level: 3
                    }
                    PlasmaComponents.ToolButton {
                        icon.name: "process-stop"
                        display: PlasmaComponents.AbstractButton.IconOnly
                        onClicked: backend.cancelAll()
                        PlasmaComponents.ToolTip.text: i18n("Cancel all")
                    }
                }

                Repeater {
                    model: backend.downloadModel
                    delegate: DownloadRow {
                        required property var model

                        downloadId: model.downloadId
                        title: model.title
                        status: model.status
                        filePath: model.filePath
                        outputDir: model.outputDir
                        formatLabel: model.formatLabel
                        previewFile: model.previewFile
                        errorLog: model.errorLog
                        progress: model.progress
                        speed: model.speed
                        eta: model.eta
                        failed: model.failed
                        paused: model.paused
                        previewEnabled: model.previewEnabled
                        hasPreview: model.hasPreview

                        Layout.fillWidth: true
                        Layout.preferredHeight: implicitHeight

                        onCancel: page.backend.cancelDownload(downloadId)
                        onPause: page.backend.pauseDownload(downloadId)
                        onResume: page.backend.resumeDownload(downloadId)
                        onPreview: page.backend.openPreview(downloadId)
                        onOpenFile: page.backend.openFile(previewFile.length > 0 ? previewFile : filePath)
                        onOpenFolder: page.backend.openFolder(filePath.length > 0 ? filePath : (outputDir || previewFile))
                    }
                }
            }

            // Failed
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.gridUnit
                Layout.rightMargin: Kirigami.Units.gridUnit
                visible: backend.failedDownloads.length > 0
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    PlasmaExtras.Heading {
                        Layout.fillWidth: true
                        text: i18n("Failed")
                        level: 3
                    }
                    PlasmaComponents.ToolButton {
                        icon.name: "edit-clear"
                        display: PlasmaComponents.AbstractButton.IconOnly
                        onClicked: backend.dismissAllFailed()
                    }
                }

                Repeater {
                    model: backend.failedDownloads
                    delegate: FailedDownloadRow {
                        required property var modelData
                        downloadId: modelData.id
                        title: modelData.title
                        status: modelData.status
                        url: modelData.url
                        progress: modelData.progress
                        filePath: modelData.filePath || ""
                        outputDir: modelData.outputDir || ""
                        formatLabel: modelData.formatLabel || ""
                        errorLog: modelData.errorLog || ""
                        onRetry: backend.retryDownload(downloadId)
                        onDismiss: backend.dismissFailed(downloadId)
                        onOpenFolder: backend.openFolder(modelData.filePath || modelData.outputDir || "")
                    }
                }
            }

            Kirigami.Separator {
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.smallSpacing
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.gridUnit
                Layout.rightMargin: Kirigami.Units.gridUnit
                Layout.bottomMargin: Kirigami.Units.smallSpacing
                spacing: Kirigami.Units.smallSpacing

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font: Kirigami.Theme.smallFont
                    opacity: 0.7
                    text: i18n("Custom bot and web development, Docker hosting")
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    PlasmaComponents.ToolButton {
                        Layout.fillWidth: true
                        icon.name: "globe-symbolic"
                        text: i18n("zrn59.me")
                        onClicked: Qt.openUrlExternally("https://zrn59.me")
                        PlasmaComponents.ToolTip.text: i18n("Custom Bot & Web Development / Docker Hosting")
                    }

                    PlasmaComponents.ToolButton {
                        Layout.fillWidth: true
                        icon.name: "favorite-symbolic"
                        text: i18n("☕ Support / Donate")
                        onClicked: Qt.openUrlExternally("https://www.donationalerts.com/r/zrn59")
                        PlasmaComponents.ToolTip.text: i18n("Support this widget")
                    }
                }
            }
        }
    }
}
