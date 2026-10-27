/*
 * SPDX-License-Identifier: MIT
 */

.pragma library

const videoQualities = [
    { label: qsTr("Best"), value: 7 },
    { label: qsTr("4K"), value: 0 },
    { label: qsTr("1440p"), value: 1 },
    { label: qsTr("1080p"), value: 2 },
    { label: qsTr("720p"), value: 3 },
    { label: qsTr("480p"), value: 4 },
    { label: qsTr("360p"), value: 5 },
    { label: qsTr("240p"), value: 6 },
    { label: qsTr("Worst"), value: 8 },
]

const videoCodecs = [
    { label: qsTr("Any"), value: 0 },
    { label: qsTr("H.264"), value: 1 },
    { label: qsTr("H.265"), value: 2 },
    { label: qsTr("AV1"), value: 3 },
    { label: qsTr("VP9"), value: 4 },
    { label: qsTr("VP8"), value: 5 },
]

const videoContainers = [
    { label: qsTr("MP4"), value: 0 },
    { label: qsTr("MKV"), value: 1 },
    { label: qsTr("WebM"), value: 2 },
    { label: qsTr("AVI"), value: 3 },
]

const audioQualities = [
    { label: qsTr("Best"), value: 0 },
    { label: qsTr("320 kbps"), value: 1 },
    { label: qsTr("256 kbps"), value: 2 },
    { label: qsTr("192 kbps"), value: 3 },
    { label: qsTr("128 kbps"), value: 4 },
    { label: qsTr("96 kbps"), value: 5 },
    { label: qsTr("64 kbps"), value: 6 },
    { label: qsTr("Worst"), value: 7 },
]

const audioCodecs = [
    { label: qsTr("Any"), value: 0 },
    { label: qsTr("AAC"), value: 1 },
    { label: qsTr("MP3"), value: 2 },
    { label: qsTr("Opus"), value: 3 },
    { label: qsTr("Vorbis"), value: 4 },
    { label: qsTr("FLAC"), value: 5 },
]

const subtitleOptions = [
    { label: qsTr("None"), value: 0 },
    { label: qsTr("Auto-generated"), value: 1 },
    { label: qsTr("Manual"), value: 2 },
    { label: qsTr("All"), value: 3 },
    { label: qsTr("Embed"), value: 4 },
]

const subtitleLanguages = [
    { label: qsTr("Auto"), value: 0 },
    { label: qsTr("English"), value: 1 },
    { label: qsTr("Русский"), value: 2 },
    { label: qsTr("Español"), value: 3 },
    { label: qsTr("Deutsch"), value: 4 },
    { label: qsTr("Français"), value: 5 },
    { label: qsTr("日本語"), value: 6 },
    { label: qsTr("한국어"), value: 7 },
    { label: qsTr("中文"), value: 8 },
    { label: qsTr("All languages"), value: 9 },
]

const subtitleFormats = [
    { label: qsTr("SRT"), value: 0 },
    { label: qsTr("VTT"), value: 1 },
    { label: qsTr("ASS"), value: 2 },
    { label: qsTr("LRC"), value: 3 },
]

const speedLimits = [
    { label: qsTr("Unlimited"), value: 0 },
    { label: qsTr("1 MB/s"), value: 1 },
    { label: qsTr("5 MB/s"), value: 2 },
    { label: qsTr("10 MB/s"), value: 3 },
    { label: qsTr("50 MB/s"), value: 4 },
]

const downloadModes = [
    { label: qsTr("Video + Audio"), icon: "video-x-generic", value: 0 },
    { label: qsTr("Video only"), icon: "video-x-generic", value: 1 },
    { label: qsTr("Audio only"), icon: "audio-x-generic", value: 2 },
]

function labelForValue(list, value) {
    for (let i = 0; i < list.length; ++i) {
        if (list[i].value === value) {
            return list[i].label
        }
    }
    return ""
}
