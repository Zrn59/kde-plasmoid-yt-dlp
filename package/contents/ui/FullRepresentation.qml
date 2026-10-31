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

    readonly property real textLimit: Math.max(Kirigami.Units.gridUnit * 16, (scroll.width > 0 ? scroll.width : implicitWidth) - Kirigami.Units.gridUnit * 2)
    property bool dropReady: false
    property string queuedUrl: ""
    onQueuedUrlChanged: {
        if (queuedUrl.length > 0)
            insertUrl(queuedUrl)
    }
    property bool showHistory: false

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
    property int cookiesBrowser: Number(cfg.cookiesBrowser) || 0
    property string cookiesFile: cfg.cookiesFile || backend.defaultCookiesFile()
    property string videoFolder: cfg.videoFolder || backend.defaultVideoFolder()
    property string audioFolder: cfg.audioFolder || backend.defaultAudioFolder()
    property string customArgs: cfg.customArgs || ""
    property bool allowUnsafeArgs: cfg.allowUnsafeArgs === true
    property string downloadSections: cfg.downloadSections || ""
    property bool sponsorBlock: cfg.sponsorBlock
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
        cfg.cookiesBrowser = cookiesBrowser
        cfg.cookiesFile = cookiesFile
        cfg.showAdvanced = showAdvanced
        cfg.videoFolder = videoFolder
        cfg.audioFolder = audioFolder
        cfg.customArgs = customArgs
        cfg.allowUnsafeArgs = allowUnsafeArgs
        cfg.downloadSections = downloadSections
        cfg.sponsorBlock = sponsorBlock
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
            cookiesBrowser: cookiesBrowser,
            cookiesFile: cookiesFile,
            customArgs: customArgs,
            allowUnsafeArgs: allowUnsafeArgs,
            downloadSections: downloadSections,
            outputTemplate: cfg.outputTemplate || "",
            sponsorBlock: sponsorBlock,
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

    function insertUrl(link) {
        const clean = Options.httpUrlFromText(link)
        if (!backend.isValidUrl(clean))
            return false
        url = clean
        urlField.text = clean
        urlField.forceActiveFocus()
        return true
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
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: footerLine.top
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
                        text: i18n("kde-plasmoid-yt-dlp")
                        font.bold: true
                    }

                    PlasmaComponents.Label {
                        opacity: 0.65
                        font: Kirigami.Theme.smallFont
                        text: {
                            const version = Plasmoid.metaData.version || ""
                            if (backend.checkingPlasmoidUpdate)
                                return i18n("widget %1: checking…", version)
                            if (backend.plasmoidUpdateNote.length > 0)
                                return i18n("widget %1: %2", version, backend.plasmoidUpdateNote)
                            return i18n("widget %1", version)
                        }
                        HoverTip {
                            label: i18n("Version of this widget")
                            show: widgetVersionHover.hovered && label.length > 0
                        }
                        HoverHandler {
                            id: widgetVersionHover
                        }
                    }

                    PlasmaComponents.Label {
                        opacity: 0.65
                        font: Kirigami.Theme.smallFont
                        text: {
                            if (backend.updatingYtdlp)
                                return i18n("yt-dlp version: updating…")
                            if (backend.ytdlpVersion.length === 0)
                                return i18n("yt-dlp version: checking…")
                            return i18n("yt-dlp version: %1", backend.ytdlpVersion)
                        }
                        HoverTip {
                            label: backend.ytdlpPath.length > 0 ? backend.ytdlpPath : i18n("yt-dlp is not installed. Click refresh to download it.")
                            show: ytdlpVersionHover.hovered && label.length > 0
                        }
                        HoverHandler {
                            id: ytdlpVersionHover
                        }
                    }
                }

                PlasmaComponents.ToolButton {
                    icon.name: "system-software-update"
                    display: PlasmaComponents.AbstractButton.IconOnly
                    enabled: !backend.checkingPlasmoidUpdate
                    onClicked: backend.checkPlasmoidUpdate(Plasmoid.metaData.version || "")
                    HoverTip { label: i18n("Check for a newer widget version") }
                }

                PlasmaComponents.ToolButton {
                    icon.name: "view-refresh"
                    display: PlasmaComponents.AbstractButton.IconOnly
                    enabled: !backend.updatingYtdlp
                    onClicked: backend.updateYtdlp()
                    HoverTip { label: i18n("Download a newer yt-dlp for this widget") }
                }
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.gridUnit
                Layout.rightMargin: Kirigami.Units.gridUnit
                visible: backend.plasmoidUpdateAvailable && backend.plasmoidReleaseUrl.length > 0
                type: Kirigami.MessageType.Information
                text: i18n("Widget %1 is available.", backend.plasmoidLatestVersion)
                actions: [
                    Kirigami.Action {
                        icon.name: "document-open-remote"
                        text: i18n("Open release")
                        onTriggered: Qt.openUrlExternally(backend.plasmoidReleaseUrl)
                    }
                ]
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.gridUnit
                Layout.rightMargin: Kirigami.Units.gridUnit
                visible: backend.lastError.length > 0
                type: Kirigami.MessageType.Error
                text: {
                    const head = Options.errorHeadline(backend.lastError)
                    return head.length > 0 ? (head + "\n" + backend.lastError) : backend.lastError
                }
                actions: [
                    Kirigami.Action {
                        icon.name: "edit-copy"
                        text: i18n("Copy")
                        onTriggered: backend.copyText(backend.lastError)
                    },
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
                        placeholderText: page.dropReady ? i18n("Drop the link here") : i18n("Paste link…")
                        onTextChanged: {
                            page.url = text
                            backend.clearLinkInfo()
                        }
                        onAccepted: page.startDownload()
                    }

                    PlasmaComponents.ToolButton {
                        icon.name: "edit-paste"
                        display: PlasmaComponents.AbstractButton.IconOnly
                        onClicked: {
                            const raw = backend.pasteFromClipboard()
                            const clip = Options.httpUrlFromText(raw)
                            if (backend.isValidUrl(clip)) {
                                urlField.text = clip
                            } else if (raw.length > 0) {
                                backend.setError(i18n("Clipboard does not contain a valid URL"))
                            }
                        }
                        HoverTip { label: i18n("Paste a link from the clipboard") }
                    }

                    PlasmaComponents.ToolButton {
                        icon.name: "document-preview"
                        text: i18n("Check")
                        enabled: url.trim().length > 0 && !backend.linkBusy
                        onClicked: backend.inspectUrl(page.buildOptions())
                        HoverTip { label: i18n("Show the title and duration") }
                    }

                    PlasmaComponents.Button {
                        icon.name: "download"
                        text: i18n("Go")
                        enabled: url.trim().length > 0 && backend.activeCount < 5
                        onClicked: page.startDownload()
                        HoverTip { label: i18n("Start the download") }
                    }
                }

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    visible: backend.linkBusy || backend.linkInfo.length > 0
                    wrapMode: Text.WordWrap
                    font: Kirigami.Theme.smallFont
                    color: backend.linkInfoError ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
                    text: backend.linkBusy ? i18n("Checking link…") : backend.linkInfo
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
                            HoverTip { label: modelData.tip }
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
                        HoverTip { label: page.showOptions ? i18n("Hide options") : i18n("Show options") }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    Layout.maximumWidth: page.textLimit
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
                        tip: i18n("Largest picture size to keep. Best uses the highest one available.")
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
                        tip: i18n("Audio bitrate. With a chosen codec, this is the bitrate after conversion.")
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
                        tip: i18n("Container of the finished file. Watching while it downloads always saves MKV.")
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
                        tip: i18n("Audio codec. Any keeps the published stream and does not convert it.")
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
                            HoverTip { label: i18n("Choose the download folder") }
                        }
                    }

                    PlasmaComponents.CheckBox {
                        Layout.columnSpan: 2
                        Layout.leftMargin: Kirigami.Units.gridUnit * 2
                        visible: downloadMode !== 2
                        text: i18n("Watch while downloading")
                        checked: previewMode
                        onToggled: { previewMode = checked; saveConfig() }
                        HoverTip { label: i18n("Build a playable MKV with sound while the download is still running.") }
                    }

                    PlasmaComponents.Label {
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        Layout.maximumWidth: page.textLimit
                        visible: previewMode && downloadMode !== 2
                        wrapMode: Text.WordWrap
                        opacity: 0.55
                        font: Kirigami.Theme.smallFont
                        text: i18n("A playable copy with sound appears under the finished filename and is updated as more of the video arrives. Press ▶ to open it. When the download ends, that same file becomes the full video. This mode saves MKV, because a growing MP4 cannot be played.")
                    }

                    PlasmaComponents.CheckBox {
                        Layout.columnSpan: 2
                        Layout.leftMargin: Kirigami.Units.gridUnit * 2
                        text: i18n("Use cookies (YouTube)")
                        checked: useCookies
                        onToggled: { useCookies = checked; saveConfig() }
                        HoverTip { label: i18n("Send cookies so YouTube can treat this as a signed-in browser. A browser profile stays fresh; a file goes stale.") }
                    }

                    OptionCombo {
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        Layout.leftMargin: Kirigami.Units.gridUnit * 2
                        visible: useCookies
                        model: Options.cookieBrowsers
                        currentValue: cookiesBrowser
                        tip: i18n("Where to read the cookies from.")
                        onActivated: (value) => { cookiesBrowser = value; saveConfig() }
                    }

                    RowLayout {
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        Layout.leftMargin: Kirigami.Units.gridUnit * 2
                        visible: useCookies && cookiesBrowser === 0
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
                            HoverTip { label: i18n("Choose a cookies file") }
                        }
                    }

                    PlasmaComponents.ToolButton {
                        Layout.columnSpan: 2
                        Layout.leftMargin: Kirigami.Units.gridUnit * 2
                        icon.name: showAdvanced ? "go-up" : "go-down"
                        text: i18n("Advanced")
                        display: PlasmaComponents.AbstractButton.TextBesideIcon
                        onClicked: {
                            showAdvanced = !showAdvanced
                            saveConfig()
                        }
                        HoverTip { label: i18n("Show extra settings") }
                    }

                    GridLayout {
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        Layout.maximumWidth: page.textLimit
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
                            tip: i18n("Video codec. MP4 with Any prefers AV1 or H.264, because VP9 breaks the opening seconds in VLC.")
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
                            tip: i18n("Which subtitles to save. Embed also puts them inside the video.")
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
                            tip: i18n("Subtitle language. Auto uses the language of this system.")
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
                            tip: i18n("Subtitle file type.")
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
                            tip: i18n("Maximum download speed.")
                            onActivated: (v) => { speedLimit = v; saveConfig() }
                        }

                        PlasmaComponents.CheckBox {
                            Layout.columnSpan: 2
                            Layout.leftMargin: Kirigami.Units.gridUnit * 2
                            text: i18n("Embed metadata")
                            checked: embedMetadata
                            onToggled: { embedMetadata = checked; saveConfig() }
                            HoverTip { label: i18n("Write the title and other details into the file.") }
                        }
                        PlasmaComponents.CheckBox {
                            Layout.columnSpan: 2
                            Layout.leftMargin: Kirigami.Units.gridUnit * 2
                            text: i18n("Embed thumbnail")
                            checked: embedThumbnail
                            onToggled: { embedThumbnail = checked; saveConfig() }
                            HoverTip { label: i18n("Store the cover image inside the file.") }
                        }
                        PlasmaComponents.CheckBox {
                            Layout.columnSpan: 2
                            Layout.leftMargin: Kirigami.Units.gridUnit * 2
                            text: i18n("Download playlist")
                            checked: allowPlaylist
                            onToggled: { allowPlaylist = checked; saveConfig() }
                            HoverTip { label: i18n("Download every item in a playlist. Off keeps only this link.") }
                        }
                        PlasmaComponents.CheckBox {
                            Layout.columnSpan: 2
                            Layout.leftMargin: Kirigami.Units.gridUnit * 2
                            text: i18n("Skip sponsors")
                            checked: sponsorBlock
                            onToggled: { sponsorBlock = checked; saveConfig() }
                            HoverTip { label: i18n("Cut sponsor segments out of the video.") }
                        }

                        PlasmaComponents.Label {
                            text: i18n("Clip")
                            opacity: 0.7
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                        }
                        PlasmaComponents.TextField {
                            Layout.fillWidth: true
                            hoverEnabled: true
                            placeholderText: i18n("10:00-15:00")
                            text: downloadSections
                            onTextChanged: downloadSections = text
                            onEditingFinished: saveConfig()
                            HoverTip { label: i18n("Download only this part of the video. A leading * is added for you.") }
                        }

                        PlasmaComponents.Label {
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            Layout.maximumWidth: page.textLimit
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
                            hoverEnabled: true
                            placeholderText: i18n("--sponsorblock-remove all")
                            text: customArgs
                            onTextChanged: customArgs = text
                            onEditingFinished: saveConfig()
                            HoverTip { label: allowUnsafeArgs
                                ? i18n("Extra yt-dlp flags, including ones that run programs or choose the output path.")
                                : i18n("Extra yt-dlp flags. Risky ones are ignored until the checkbox below is on.") }
                        }

                        PlasmaComponents.CheckBox {
                            Layout.columnSpan: 2
                            Layout.leftMargin: Kirigami.Units.gridUnit * 2
                            text: i18n("Allow risky flags")
                            checked: allowUnsafeArgs
                            onToggled: { allowUnsafeArgs = checked; saveConfig() }
                            HoverTip { label: i18n("Pass output path, shell commands, plugins, and a custom downloader through. Off, those flags are dropped.") }
                        }

                        PlasmaComponents.Label {
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            Layout.maximumWidth: page.textLimit
                            wrapMode: Text.WordWrap
                            opacity: 0.55
                            font: Kirigami.Theme.smallFont
                            text: i18n("Raw yt-dlp flags. The global yt-dlp config is not used. Risky flags can run another program or write files outside the download folder.")
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
                        HoverTip { label: i18n("Cancel all") }
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
                        onCopyLog: page.backend.copyText(errorLog)
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
                        HoverTip { label: i18n("Clear failed downloads") }
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
                        onCopyLog: backend.copyText(errorLog.length > 0 ? errorLog : status)
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.gridUnit
                Layout.rightMargin: Kirigami.Units.gridUnit
                Layout.maximumWidth: page.textLimit
                visible: backend.history.length > 0
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    Layout.maximumWidth: page.textLimit

                    PlasmaComponents.ToolButton {
                        Layout.fillWidth: true
                        icon.name: page.showHistory ? "go-up" : "go-down"
                        text: i18n("Recent (%1)", backend.history.length)
                        display: PlasmaComponents.AbstractButton.TextBesideIcon
                        onClicked: page.showHistory = !page.showHistory
                        HoverTip { label: i18n("Show finished downloads") }
                    }

                    PlasmaComponents.ToolButton {
                        visible: page.showHistory
                        icon.name: "edit-clear"
                        display: PlasmaComponents.AbstractButton.IconOnly
                        onClicked: {
                            backend.clearHistory()
                            page.showHistory = false
                        }
                        HoverTip { label: i18n("Clear history") }
                    }
                }

                Repeater {
                    model: page.showHistory ? backend.history : []
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        PlasmaComponents.Label {
                            Layout.fillWidth: true
                            text: modelData.title || modelData.url
                            elide: Text.ElideRight
                            maximumLineCount: 1
                        }

                        PlasmaComponents.ToolButton {
                            icon.name: "document-open"
                            display: PlasmaComponents.AbstractButton.IconOnly
                            enabled: (modelData.filePath || "").length > 0
                            onClicked: backend.openFile(modelData.filePath)
                            HoverTip { label: i18n("Open file") }
                        }

                        PlasmaComponents.ToolButton {
                            icon.name: "folder-open"
                            display: PlasmaComponents.AbstractButton.IconOnly
                            enabled: (modelData.filePath || modelData.outputDir || "").length > 0
                            onClicked: backend.openFolder(modelData.filePath || modelData.outputDir)
                            HoverTip { label: i18n("Open folder") }
                        }

                        PlasmaComponents.ToolButton {
                            icon.name: "window-close"
                            display: PlasmaComponents.AbstractButton.IconOnly
                            onClicked: backend.dismissHistory(modelData.id)
                            HoverTip { label: i18n("Dismiss") }
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: Kirigami.Units.smallSpacing }
        }
    }

    Kirigami.Separator {
        id: footerLine
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: footer.top
        anchors.bottomMargin: Kirigami.Units.smallSpacing
    }

    RowLayout {
        id: footer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Kirigami.Units.smallSpacing
        anchors.rightMargin: Kirigami.Units.smallSpacing
        anchors.bottomMargin: Kirigami.Units.smallSpacing
        anchors.topMargin: Kirigami.Units.smallSpacing
        spacing: Kirigami.Units.smallSpacing

        PlasmaComponents.ToolButton {
            icon.name: "globe-symbolic"
            text: i18n("zrn59.me")
            flat: true
            onClicked: Qt.openUrlExternally("https://zrn59.me")
            HoverTip { label: i18n("Open the site") }
        }

        PlasmaComponents.ToolButton {
            icon.name: "folder-git"
            text: i18n("GitHub")
            flat: true
            onClicked: Qt.openUrlExternally("https://github.com/Zrn59/kde-plasmoid-yt-dlp")
            HoverTip { label: i18n("Open the project page") }
        }

        Item { Layout.fillWidth: true }

        PlasmaComponents.ToolButton {
            icon.name: "favorite-symbolic"
            text: i18n("☕ Support / Donate")
            flat: true
            onClicked: Qt.openUrlExternally("https://www.donationalerts.com/r/zrn59")
            HoverTip { label: i18n("Support the project") }
        }
    }
}
