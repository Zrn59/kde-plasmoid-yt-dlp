/*
 * SPDX-License-Identifier: MIT
 */

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.SimpleKCM {
    // Values
    property string cfg_videoFolder: ""
    property string cfg_audioFolder: ""
    property string cfg_cookiesFile: ""
    property int cfg_downloadMode: 0
    property int cfg_videoQuality: 7
    property int cfg_videoCodec: 0
    property int cfg_videoContainer: 0
    property int cfg_audioQuality: 0
    property int cfg_audioCodec: 0
    property int cfg_subtitleOption: 0
    property int cfg_subtitleLanguage: 0
    property int cfg_subtitleFormat: 0
    property int cfg_speedLimit: 0
    property bool cfg_embedMetadata: true
    property bool cfg_embedThumbnail: true
    property bool cfg_allowPlaylist: false
    property bool cfg_previewMode: false
    property bool cfg_useCookies: false
    property bool cfg_showAdvanced: false
    property string cfg_customArgs: ""
    property string cfg_downloadSections: ""

    // Plasma also injects *Default flags and geometry keys
    property bool cfg_videoFolderDefault: true
    property bool cfg_audioFolderDefault: true
    property bool cfg_cookiesFileDefault: true
    property bool cfg_downloadModeDefault: true
    property bool cfg_videoQualityDefault: true
    property bool cfg_videoCodecDefault: true
    property bool cfg_videoContainerDefault: true
    property bool cfg_audioQualityDefault: true
    property bool cfg_audioCodecDefault: true
    property bool cfg_subtitleOptionDefault: true
    property bool cfg_subtitleLanguageDefault: true
    property bool cfg_subtitleFormatDefault: true
    property bool cfg_speedLimitDefault: true
    property bool cfg_embedMetadataDefault: true
    property bool cfg_embedThumbnailDefault: true
    property bool cfg_allowPlaylistDefault: true
    property bool cfg_previewModeDefault: true
    property bool cfg_useCookiesDefault: true
    property bool cfg_showAdvancedDefault: true
    property bool cfg_customArgsDefault: true
    property bool cfg_downloadSectionsDefault: true
    property bool cfg_expanding: false
    property int cfg_length: 0

    Kirigami.FormLayout {
        Kirigami.Heading {
            Kirigami.FormData.label: i18n("kde-plasmoid-yt-dlp")
            text: i18n("Settings are in the widget popup.")
        }
    }
}
