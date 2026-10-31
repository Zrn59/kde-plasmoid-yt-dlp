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
    { label: qsTr("Video + Audio"), icon: "video-x-generic", value: 0, tip: qsTr("Save the picture and the sound together.") },
    { label: qsTr("Video only"), icon: "video-x-generic", value: 1, tip: qsTr("Save the picture without the sound.") },
    { label: qsTr("Audio only"), icon: "audio-x-generic", value: 2, tip: qsTr("Save only the sound.") },
]

const cookieBrowsers = [
    { label: qsTr("Cookies file"), value: 0 },
    { label: qsTr("Firefox"), value: 1 },
    { label: qsTr("Chrome"), value: 2 },
    { label: qsTr("Chromium"), value: 3 },
    { label: qsTr("Brave"), value: 4 },
    { label: qsTr("Edge"), value: 5 },
    { label: qsTr("Vivaldi"), value: 6 },
]

function labelForValue(list, value) {
    for (let i = 0; i < list.length; ++i) {
        if (list[i].value === value) {
            return list[i].label
        }
    }
    return ""
}

function httpUrlFromText(raw) {
    const match = String(raw || "").match(/https?:\/\/[^\s<>"']+/)
    if (!match)
        return ""
    return match[0].replace(/[)\].,>]+$/, "")
}

function urlFromDrop(drop) {
    if (!drop)
        return ""
    const chunks = []
    if (drop.hasUrls && drop.urls) {
        for (let i = 0; i < drop.urls.length; ++i)
            chunks.push(String(drop.urls[i]))
    }
    if (drop.hasText && drop.text)
        chunks.push(String(drop.text))
    if (drop.hasHtml && drop.html)
        chunks.push(String(drop.html))
    const formats = drop.formats || []
    for (let i = 0; i < formats.length; ++i) {
        const fmt = formats[i]
        try {
            if (drop.getDataAsArrayBuffer && (fmt === "text/x-moz-url" || fmt === "text/uri-list")) {
                const buf = drop.getDataAsArrayBuffer(fmt)
                if (buf) {
                    const bytes = new Uint8Array(buf)
                    let decoded = ""
                    const wide = fmt === "text/x-moz-url"
                    for (let n = 0; n + (wide ? 1 : 0) < bytes.length; n += wide ? 2 : 1) {
                        const code = wide ? (bytes[n] | (bytes[n + 1] << 8)) : bytes[n]
                        if (code === 0)
                            break
                        decoded += String.fromCharCode(code)
                    }
                    if (decoded.length > 0)
                        chunks.push(decoded)
                }
            }
            if (drop.getDataAsString) {
                const value = drop.getDataAsString(fmt)
                if (value)
                    chunks.push(value)
            }
        } catch (e) {
        }
    }
    for (let i = 0; i < chunks.length; ++i) {
        const link = httpUrlFromText(chunks[i])
        if (link.length > 0)
            return link
    }
    return ""
}

function errorHeadline(text) {
    const lines = String(text || "").split("\n").map((line) => line.trim()).filter((line) => line.length > 0)
    let focus = ""
    for (let i = lines.length - 1; i >= 0; --i) {
        if (lines[i].toLowerCase().indexOf("error") !== -1) {
            focus = lines[i]
            break
        }
    }
    if (focus.length === 0) {
        for (let i = lines.length - 1; i >= 0; --i) {
            if (lines[i].indexOf("--") === -1 && lines[i].indexOf("yt-dlp") === -1) {
                focus = lines[i]
                break
            }
        }
    }
    if (focus.length === 0)
        return ""
    const s = focus.toLowerCase()
    const rules = [
        ["sign in to confirm", qsTr("Sign-in or cookies required")],
        ["login required", qsTr("Sign-in or cookies required")],
        ["use --cookies", qsTr("Sign-in or cookies required")],
        ["confirm you're not a bot", qsTr("Sign-in or cookies required")],
        ["private video", qsTr("Video is unavailable")],
        ["video unavailable", qsTr("Video is unavailable")],
        ["this video is unavailable", qsTr("Video is unavailable")],
        ["age-restricted", qsTr("Age-restricted video")],
        ["confirm your age", qsTr("Age-restricted video")],
        ["not available in your country", qsTr("Blocked in this country")],
        ["geo restricted", qsTr("Blocked in this country")],
        ["no space left", qsTr("Not enough disk space")],
        ["http error 403", qsTr("Access denied (403)")],
        ["403: forbidden", qsTr("Access denied (403)")],
        ["http error 404", qsTr("Not found (404)")],
        ["requested format is not available", qsTr("No matching format")],
        ["unsupported url", qsTr("This link is not supported")],
        ["unable to download api page", qsTr("Network error")],
        ["unable to download webpage", qsTr("Network error")],
        ["timed out", qsTr("Network error")],
        ["connection reset", qsTr("Network error")],
        ["temporary failure in name resolution", qsTr("Network error")],
        ["ffmpeg", qsTr("ffmpeg could not finish the file")],
        ["ffprobe", qsTr("ffmpeg could not finish the file")],
    ]
    for (let i = 0; i < rules.length; ++i) {
        if (s.indexOf(rules[i][0]) !== -1)
            return rules[i][1]
    }
    return ""
}
