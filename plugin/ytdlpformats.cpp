// SPDX-License-Identifier: MIT

#include "ytdlpformats.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>

#include <dlfcn.h>

namespace YtdlpFormats {

static QStringList searchDirs()
{
    QStringList dirs;
    dirs << runtimeBinDir();
    dirs << QDir::homePath() + QStringLiteral("/.local/bin");
    dirs << QStringLiteral("/usr/local/bin");
    dirs << QStringLiteral("/usr/bin");
    return dirs;
}

static QString findExecutable(const QString &name, const QStringList &fallbacks)
{
    for (const QString &dir : searchDirs()) {
        const QString candidate = dir + QLatin1Char('/') + name;
        if (QFileInfo(candidate).isExecutable()) {
            return candidate;
        }
    }
    const QString path = QStandardPaths::findExecutable(name);
    if (!path.isEmpty()) {
        return path;
    }
    for (const QString &fallback : fallbacks) {
        if (QFileInfo(fallback).isExecutable()) {
            return fallback;
        }
    }
    return {};
}

static bool isZipapp(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    return file.read(2) == "#!";
}

QString runtimeRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/com.zrn59.ytdlp");
}

QString runtimeBinDir()
{
    return runtimeRoot() + QStringLiteral("/bin");
}

QString localYtdlpPath()
{
    return runtimeBinDir() + QStringLiteral("/yt-dlp");
}

static QString packagedUpdateScript()
{
    Dl_info info{};
    if (dladdr(reinterpret_cast<void *>(&packagedUpdateScript), &info) == 0 || !info.dli_fname) {
        return {};
    }
    QDir dir(QFileInfo(QString::fromLocal8Bit(info.dli_fname)).absolutePath());
    // libytdlpplugin.so -> .../contents/qml/com/zrn59/ytdlp
    if (!dir.cdUp() || !dir.cdUp() || !dir.cdUp()) {
        return {};
    }
    const QString script = dir.filePath(QStringLiteral("scripts/yt-dlp-update.sh"));
    return QFileInfo(script).isExecutable() ? script : QString();
}

QString updateScriptPath()
{
    const QString packaged = packagedUpdateScript();
    if (!packaged.isEmpty()) {
        return packaged;
    }
    const QString userScript = runtimeBinDir() + QStringLiteral("/yt-dlp-update.sh");
    if (QFileInfo(userScript).isExecutable()) {
        return userScript;
    }
    return QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                  QStringLiteral("com.zrn59.ytdlp/bin/yt-dlp-update.sh"));
}

QString findYtdlp()
{
    const QString local = localYtdlpPath();
    if (QFileInfo(local).isExecutable()) {
        return local;
    }
    return {};
}

QString findFfmpeg()
{
    return findExecutable(QStringLiteral("ffmpeg"),
                          {QStringLiteral("/usr/bin/ffmpeg"),
                           QStringLiteral("/usr/local/bin/ffmpeg")});
}

QString findMediaPlayer()
{
    const QStringList candidates = {QStringLiteral("haruna"),
                                    QStringLiteral("mpv"),
                                    QStringLiteral("vlc"),
                                    QStringLiteral("celluloid"),
                                    QStringLiteral("smplayer")};
    for (const QString &name : candidates) {
        const QString path = findExecutable(name, {});
        if (!path.isEmpty()) {
            return path;
        }
    }
    return {};
}

QString findDeno()
{
    return findExecutable(QStringLiteral("deno"), {});
}

QString jsRuntimeSpec()
{
    const QString deno = findDeno();
    if (!deno.isEmpty()) {
        return QStringLiteral("deno:") + deno;
    }
    const QString node = findExecutable(QStringLiteral("node"), {});
    if (!node.isEmpty()) {
        return QStringLiteral("node:") + node;
    }
    const QString bun = findExecutable(QStringLiteral("bun"), {});
    if (!bun.isEmpty()) {
        return QStringLiteral("bun:") + bun;
    }
    return {};
}

QString processPath()
{
    QStringList prefixes;
    const QString home = QDir::homePath();
    if (isZipapp(localYtdlpPath())) {
        const QString pyenvShims = home + QStringLiteral("/.pyenv/shims");
        const QString pyenvBin = home + QStringLiteral("/.pyenv/bin");
        if (QFileInfo(pyenvShims).isDir()) {
            prefixes << pyenvShims;
        }
        if (QFileInfo(pyenvBin).isDir()) {
            prefixes << pyenvBin;
        }
    }
    prefixes << runtimeBinDir();
    prefixes << home + QStringLiteral("/.local/bin");

    QString path = prefixes.join(QLatin1Char(':'));
    const QByteArray current = qgetenv("PATH");
    if (!current.isEmpty()) {
        path += QLatin1Char(':') + QString::fromLocal8Bit(current);
    } else {
        path += QStringLiteral(":/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin");
    }
    return path;
}

static QString videoCodecFilter(VideoCodec codec)
{
    switch (codec) {
    case VideoCodec::AV1:
        return QStringLiteral("[vcodec^=av01]");
    case VideoCodec::H265:
        return QStringLiteral("[vcodec~='^(hev1|hvc1|hevc)']");
    case VideoCodec::H264:
        return QStringLiteral("[vcodec~='^(avc1|avc3|h264)']");
    case VideoCodec::VP9:
        return QStringLiteral("[vcodec~='^(vp09|vp9)']");
    case VideoCodec::VP8:
        return QStringLiteral("[vcodec^=vp8]");
    default:
        return {};
    }
}

static int qualityHeight(VideoQuality quality)
{
    switch (quality) {
    case VideoQuality::UHD4K:
        return 2160;
    case VideoQuality::QHD:
        return 1440;
    case VideoQuality::FHD:
        return 1080;
    case VideoQuality::HD:
        return 720;
    case VideoQuality::SD:
        return 480;
    case VideoQuality::Low:
        return 360;
    case VideoQuality::Lowest:
        return 240;
    default:
        return -1;
    }
}

static QString containerExt(VideoContainer container)
{
    switch (container) {
    case VideoContainer::MKV:
        return QStringLiteral("mkv");
    case VideoContainer::WebM:
        return QStringLiteral("webm");
    case VideoContainer::AVI:
        return QStringLiteral("avi");
    default:
        return QStringLiteral("mp4");
    }
}

static QString audioCodecExt(AudioCodec codec)
{
    switch (codec) {
    case AudioCodec::MP3:
        return QStringLiteral("mp3");
    case AudioCodec::Opus:
        return QStringLiteral("opus");
    case AudioCodec::Vorbis:
        return QStringLiteral("ogg");
    case AudioCodec::FLAC:
        return QStringLiteral("flac");
    default:
        return QStringLiteral("m4a");
    }
}

static QString subtitleExt(SubtitleFormat format)
{
    switch (format) {
    case SubtitleFormat::VTT:
        return QStringLiteral("vtt");
    case SubtitleFormat::ASS:
        return QStringLiteral("ass");
    case SubtitleFormat::LRC:
        return QStringLiteral("lrc");
    default:
        return QStringLiteral("srt");
    }
}

static QString subtitleLang(SubtitleLanguage lang)
{
    switch (lang) {
    case SubtitleLanguage::English:
        return QStringLiteral("en");
    case SubtitleLanguage::Russian:
        return QStringLiteral("ru");
    case SubtitleLanguage::Spanish:
        return QStringLiteral("es");
    case SubtitleLanguage::German:
        return QStringLiteral("de");
    case SubtitleLanguage::French:
        return QStringLiteral("fr");
    case SubtitleLanguage::Japanese:
        return QStringLiteral("ja");
    case SubtitleLanguage::Korean:
        return QStringLiteral("ko");
    case SubtitleLanguage::Chinese:
        return QStringLiteral("zh");
    case SubtitleLanguage::All:
        return QStringLiteral("all");
    default:
        return {};
    }
}

static QString buildVideoAudioFormat(VideoQuality quality, VideoCodec codec)
{
    if (quality == VideoQuality::Best) {
        return QStringLiteral("bestvideo+bestaudio/best");
    }
    if (quality == VideoQuality::Worst) {
        return QStringLiteral("worstvideo+worstaudio/worst");
    }
    const int height = qualityHeight(quality);
    const QString codecFilter = videoCodecFilter(codec);
    return QStringLiteral("bestvideo[height<=%1]%2+bestaudio/bestvideo[height<=%1]+bestaudio/best")
        .arg(height)
        .arg(codecFilter);
}

static QString buildVideoOnlyFormat(VideoQuality quality, VideoCodec codec)
{
    if (quality == VideoQuality::Best) {
        return QStringLiteral("bestvideo");
    }
    if (quality == VideoQuality::Worst) {
        return QStringLiteral("worstvideo");
    }
    const int height = qualityHeight(quality);
    const QString codecFilter = videoCodecFilter(codec);
    return QStringLiteral("bestvideo[height<=%1]%2/bestvideo[height<=%1]/bestvideo")
        .arg(height)
        .arg(codecFilter);
}

static QString buildAudioFormat(AudioQuality quality)
{
    switch (quality) {
    case AudioQuality::Worst:
        return QStringLiteral("worstaudio/worst");
    case AudioQuality::High320:
        return QStringLiteral("bestaudio[abr<=320]/bestaudio");
    case AudioQuality::High256:
        return QStringLiteral("bestaudio[abr<=256]/bestaudio");
    case AudioQuality::Medium192:
        return QStringLiteral("bestaudio[abr<=192]/bestaudio");
    case AudioQuality::Medium128:
        return QStringLiteral("bestaudio[abr<=128]/bestaudio");
    case AudioQuality::Low96:
        return QStringLiteral("bestaudio[abr<=96]/bestaudio");
    case AudioQuality::Low64:
        return QStringLiteral("bestaudio[abr<=64]/bestaudio");
    default:
        return QStringLiteral("bestaudio/best");
    }
}

static void addSubtitleArgs(QStringList &args, const DownloadOptions &options)
{
    if (options.subtitleOption == SubtitleOption::None) {
        return;
    }

    const QString ext = subtitleExt(options.subtitleFormat);
    args << QStringLiteral("--convert-subs") << ext;
    args << QStringLiteral("--sub-format") << ext + QStringLiteral("/best");

    switch (options.subtitleOption) {
    case SubtitleOption::AutoGenerated:
        args << QStringLiteral("--write-auto-subs");
        break;
    case SubtitleOption::Manual:
        args << QStringLiteral("--write-subs");
        break;
    case SubtitleOption::All:
        args << QStringLiteral("--write-subs") << QStringLiteral("--write-auto-subs");
        break;
    case SubtitleOption::Embed:
        args << QStringLiteral("--write-subs") << QStringLiteral("--write-auto-subs") << QStringLiteral("--embed-subs");
        break;
    default:
        break;
    }

    const QString lang = subtitleLang(options.subtitleLanguage);
    if (!lang.isEmpty() && lang != QStringLiteral("all")) {
        args << QStringLiteral("--sub-langs") << lang;
    }
}

static qint64 speedLimitBytes(SpeedLimit limit)
{
    switch (limit) {
    case SpeedLimit::Slow1M:
        return 1'000'000;
    case SpeedLimit::Medium5M:
        return 5'000'000;
    case SpeedLimit::Fast10M:
        return 10'000'000;
    case SpeedLimit::Custom50M:
        return 50'000'000;
    default:
        return 0;
    }
}

QString normalizeDownloadSection(const QString &raw)
{
    QString section = raw.trimmed();
    if (section.isEmpty()) {
        return {};
    }
    // Allow "10:00-15:00" — yt-dlp needs "*10:00-15:00" for time ranges
    if (!section.startsWith(QLatin1Char('*')) && !section.startsWith(QLatin1Char('%'))) {
        section.prepend(QLatin1Char('*'));
    }
    return section;
}

QStringList sanitizeCustomArgs(const QString &raw)
{
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const QStringList parts = QProcess::splitCommand(trimmed);
    static const QSet<QString> blocked = {
        QStringLiteral("-o"),
        QStringLiteral("--output"),
        QStringLiteral("--exec"),
        QStringLiteral("--exec-before-download"),
        QStringLiteral("--config-location"),
        QStringLiteral("--config-locations"),
        QStringLiteral("-a"),
        QStringLiteral("--batch-file"),
        QStringLiteral("--load-info-json"),
        QStringLiteral("--write-pages"),
        QStringLiteral("--print-to-file"),
    };

    QStringList safe;
    for (int i = 0; i < parts.size(); ++i) {
        const QString &arg = parts.at(i);
        const QString key = arg.section(QLatin1Char('='), 0, 0);
        if (blocked.contains(key)) {
            // skip flag and its value if separate
            if (!arg.contains(QLatin1Char('=')) && i + 1 < parts.size() && !parts.at(i + 1).startsWith(QLatin1Char('-'))) {
                ++i;
            }
            continue;
        }
        safe.append(arg);
    }
    return safe;
}

QStringList buildArgs(const DownloadOptions &options, const QString &ffmpegPath)
{
    QStringList args;

    args << QStringLiteral("--ignore-config");

    if (options.allowPlaylist) {
        args << QStringLiteral("--yes-playlist");
    } else {
        args << QStringLiteral("--no-playlist");
    }

    args << QStringLiteral("--no-mtime") << QStringLiteral("--newline") << QStringLiteral("--progress");

    if (options.resumePartial) {
        args << QStringLiteral("-c");
    }

    if (options.previewMode) {
        // Streams write directly to disk (no .part) so ffmpeg can read growing files
        args << QStringLiteral("--no-part");
    }

    if (ffmpegPath.startsWith(QLatin1Char('/'))) {
        args << QStringLiteral("--ffmpeg-location") << ffmpegPath;
    }

    const QString jsRuntime = jsRuntimeSpec();
    if (!jsRuntime.isEmpty()) {
        args << QStringLiteral("--js-runtimes") << jsRuntime;
    }

    if (options.useCookies && !options.cookiesFile.isEmpty()) {
        if (QFile::exists(options.cookiesFile)) {
            args << QStringLiteral("--cookies") << options.cookiesFile;
        }
    }

    // One absolute template. "audio:" / "video:" are not -o types, so yt-dlp
    // would save them as a folder named "audio:" inside the download directory.
    const QString baseName = QStringLiteral("%(title).200B");
    args << QStringLiteral("-o")
         << QDir(options.outputDir).filePath(baseName + QStringLiteral(".%(ext)s"));

    if (!options.resumePartial) {
        args << QStringLiteral("--no-overwrites");
    }

    // MKV handles incomplete playback best when preview mode is on
    VideoContainer effectiveContainer = options.videoContainer;
    if (options.previewMode && options.mode != DownloadMode::AudioOnly) {
        effectiveContainer = VideoContainer::MKV;
    }

    switch (options.mode) {
    case DownloadMode::VideoWithAudio:
        args << QStringLiteral("-f") << buildVideoAudioFormat(options.videoQuality, options.videoCodec);
        args << QStringLiteral("--merge-output-format") << containerExt(effectiveContainer);
        break;
    case DownloadMode::VideoOnly:
        args << QStringLiteral("-f") << buildVideoOnlyFormat(options.videoQuality, options.videoCodec);
        args << QStringLiteral("--remux-video") << containerExt(effectiveContainer);
        break;
    case DownloadMode::AudioOnly:
        args << QStringLiteral("-f") << buildAudioFormat(options.audioQuality);
        args << QStringLiteral("-x");
        if (options.audioCodec != AudioCodec::Any) {
            args << QStringLiteral("--audio-format") << audioCodecExt(options.audioCodec);
            if (options.audioCodec == AudioCodec::MP3) {
                args << QStringLiteral("--audio-quality") << QStringLiteral("0");
            }
        }
        break;
    }

    addSubtitleArgs(args, options);

    if (options.embedMetadata) {
        args << QStringLiteral("--embed-metadata");
    }
    if (options.embedThumbnail && options.mode != DownloadMode::VideoOnly) {
        args << QStringLiteral("--embed-thumbnail");
    }

    const qint64 limit = speedLimitBytes(options.speedLimit);
    if (limit > 0) {
        args << QStringLiteral("--limit-rate") << QString::number(limit / 1000) + QStringLiteral("K");
    }

    const QString section = normalizeDownloadSection(options.downloadSections);
    if (!section.isEmpty()) {
        args << QStringLiteral("--download-sections") << section;
    }

    args << QStringLiteral("--concurrent-fragments")
         << (options.previewMode ? QStringLiteral("8") : QStringLiteral("4"));
    args << QStringLiteral("--retries") << QStringLiteral("10");
    args << QStringLiteral("--fragment-retries") << QStringLiteral("10");

    args << sanitizeCustomArgs(options.customArgs);
    args << options.url;

    return args;
}

} // namespace YtdlpFormats
