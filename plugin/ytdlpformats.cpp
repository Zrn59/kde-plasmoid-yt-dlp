// SPDX-License-Identifier: MIT

#include "ytdlpformats.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>

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
    default: {
        const QString name = QLocale::system().name();
        const QString lang = name.section(QLatin1Char('_'), 0, 0);
        if (lang.isEmpty() || lang == QLatin1String("C") || lang == QLatin1String("POSIX")) {
            return {};
        }
        return lang;
    }
    }
}

static QString videoPick(const QString &kind, VideoQuality quality, const QString &codecFilter)
{
    QString selector = kind;
    const int height = qualityHeight(quality);
    if (height >= 0) {
        selector += QStringLiteral("[height<=%1]").arg(height);
    }
    selector += codecFilter;
    return selector;
}

static QString singleStreamFallback(VideoQuality quality, bool worst)
{
    const int height = qualityHeight(quality);
    const QString kind = worst ? QStringLiteral("w") : QStringLiteral("b");
    if (height < 0) {
        return kind + QStringLiteral("[ext=mp4]/") + kind;
    }
    return kind + QStringLiteral("[height<=%1][ext=mp4]/").arg(height) + kind;
}

static QString buildMp4VideoOnlyFormat(VideoQuality quality, VideoCodec codec)
{
    if (codec == VideoCodec::Any) {
        if (quality == VideoQuality::Best) {
            return QStringLiteral("bv*[vcodec^=av01]/bv*[vcodec^=avc1]/bv*[ext=mp4]/b");
        }
        if (quality == VideoQuality::Worst) {
            return QStringLiteral("wv*[vcodec^=avc1]/wv*[ext=mp4]/w");
        }
        const int height = qualityHeight(quality);
        return QStringLiteral("bv*[height<=%1][vcodec^=av01]/bv*[height<=%1][vcodec^=avc1]/b[height<=%1][ext=mp4]/b")
            .arg(height);
    }

    const bool worst = quality == VideoQuality::Worst;
    const QString kind = worst ? QStringLiteral("wv*") : QStringLiteral("bv*");
    const QString video = videoPick(kind, quality, videoCodecFilter(codec));
    const QString plain = videoPick(kind, quality, {});
    return video + QLatin1Char('/') + plain + QStringLiteral("[ext=mp4]/") + singleStreamFallback(quality, worst);
}

static QString buildVideoAudioFormat(VideoQuality quality, VideoCodec codec, bool preview, bool mp4)
{
    if (preview) {
        // Audio first, and WebM video, so a playable file with sound can be
        // built while the rest of the video is still downloading.
        if (quality == VideoQuality::Best) {
            return QStringLiteral("bestaudio+bestvideo[ext=webm]/bestaudio+bestvideo/best");
        }
        if (quality == VideoQuality::Worst) {
            return QStringLiteral("worstaudio+worstvideo[ext=webm]/worstaudio+worstvideo/worst");
        }
        const int height = qualityHeight(quality);
        return QStringLiteral("bestaudio+bestvideo[height<=%1][ext=webm]/bestaudio+bestvideo[height<=%1]/best")
            .arg(height);
    }
    // VP9 inside MP4 makes VLC break the opening seconds. AV1+Opus and H.264+AAC do not.
    // An explicit codec still wins: the user asked for that codec.
    if (mp4 && codec == VideoCodec::Any) {
        if (quality == VideoQuality::Best) {
            return QStringLiteral("bv*[vcodec^=av01]+ba[acodec^=opus]/bv*[vcodec^=avc1]+ba[ext=m4a]/bv*[vcodec^=avc1]+ba/b[ext=mp4]/b");
        }
        if (quality == VideoQuality::Worst) {
            return QStringLiteral("wv*[vcodec^=avc1]+wa[ext=m4a]/wv*[vcodec^=avc1]+wa/w[ext=mp4]/w");
        }
        const int height = qualityHeight(quality);
        return QStringLiteral("bv*[height<=%1][vcodec^=av01]+ba[acodec^=opus]/bv*[height<=%1][vcodec^=avc1]+ba[ext=m4a]/bv*[height<=%1][vcodec^=avc1]+ba/b[height<=%1][ext=mp4]/b")
            .arg(height);
    }
    if (mp4) {
        const bool worst = quality == VideoQuality::Worst;
        const QString kind = worst ? QStringLiteral("wv*") : QStringLiteral("bv*");
        const QString video = videoPick(kind, quality, videoCodecFilter(codec));
        const QString audio = codec == VideoCodec::AV1 ? QStringLiteral("ba[acodec^=opus]") : QStringLiteral("ba[ext=m4a]");
        return video + QLatin1Char('+') + audio + QLatin1Char('/') + video + QStringLiteral("+ba/")
            + singleStreamFallback(quality, worst);
    }

    const bool worst = quality == VideoQuality::Worst;
    const QString videoKind = worst ? QStringLiteral("worstvideo") : QStringLiteral("bestvideo");
    const QString audioKind = worst ? QStringLiteral("worstaudio") : QStringLiteral("bestaudio");
    const QString tail = worst ? QStringLiteral("worst") : QStringLiteral("best");
    const QString filter = videoCodecFilter(codec);
    const QString plain = videoPick(videoKind, quality, {});
    if (filter.isEmpty()) {
        return plain + QLatin1Char('+') + audioKind + QLatin1Char('/') + tail;
    }
    const QString video = videoPick(videoKind, quality, filter);
    return video + QLatin1Char('+') + audioKind + QLatin1Char('/') + plain + QLatin1Char('+') + audioKind + QLatin1Char('/') + tail;
}

static QString buildVideoOnlyFormat(VideoQuality quality, VideoCodec codec)
{
    const bool worst = quality == VideoQuality::Worst;
    const QString kind = worst ? QStringLiteral("worstvideo") : QStringLiteral("bestvideo");
    const QString filter = videoCodecFilter(codec);
    if (filter.isEmpty() && (quality == VideoQuality::Best || quality == VideoQuality::Worst)) {
        return kind;
    }
    const QString plain = videoPick(kind, quality, {});
    if (filter.isEmpty()) {
        return plain + QLatin1Char('/') + plain + QLatin1Char('/') + kind;
    }
    return videoPick(kind, quality, filter) + QLatin1Char('/') + plain + QLatin1Char('/') + kind;
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
    if (!lang.isEmpty()) {
        args << QStringLiteral("--sub-langs") << lang;
    }
}

static QString speedLimitRate(SpeedLimit limit)
{
    switch (limit) {
    case SpeedLimit::Slow1M:
        return QStringLiteral("1M");
    case SpeedLimit::Medium5M:
        return QStringLiteral("5M");
    case SpeedLimit::Fast10M:
        return QStringLiteral("10M");
    case SpeedLimit::Custom50M:
        return QStringLiteral("50M");
    default:
        return {};
    }
}

static QString audioQualityValue(AudioQuality quality)
{
    switch (quality) {
    case AudioQuality::High320:
        return QStringLiteral("320K");
    case AudioQuality::High256:
        return QStringLiteral("256K");
    case AudioQuality::Medium192:
        return QStringLiteral("192K");
    case AudioQuality::Medium128:
        return QStringLiteral("128K");
    case AudioQuality::Low96:
        return QStringLiteral("96K");
    case AudioQuality::Low64:
        return QStringLiteral("64K");
    case AudioQuality::Worst:
        return QStringLiteral("10");
    default:
        return QStringLiteral("0");
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

QStringList sanitizeCustomArgs(const QString &raw, bool allowUnsafe)
{
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const QStringList parts = QProcess::splitCommand(trimmed);
    if (allowUnsafe) {
        return parts;
    }
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
        QStringLiteral("--netrc-cmd"),
        QStringLiteral("--plugin-dirs"),
        QStringLiteral("--alias"),
        QStringLiteral("--downloader"),
        QStringLiteral("--external-downloader"),
        QStringLiteral("--downloader-args"),
        QStringLiteral("--external-downloader-args"),
        QStringLiteral("--ffmpeg-location"),
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

QString outputBaseName(const QString &requested)
{
    QString name = requested.trimmed();
    name.replace(QLatin1Char('\n'), QString());
    name.replace(QLatin1Char('\r'), QString());
    if (name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\')) || name.contains(QStringLiteral(".."))) {
        name.clear();
    }
    if (name.endsWith(QStringLiteral(".%(ext)s"))) {
        name.chop(static_cast<int>(QStringLiteral(".%(ext)s").size()));
    }
    name.replace(QStringLiteral("%(ext)s"), QString());
    name = name.trimmed();
    if (name.isEmpty()) {
        name = QStringLiteral("%(title).200B");
    }
    return name;
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
    // Stable machine-readable progress. yt-dlp asks callers not to scrape the default bar.
    args << QStringLiteral("--progress-template")
         << QStringLiteral("download:@@%(progress._percent_str)s@@%(progress._speed_str)s@@%(progress._eta_str)s");
    args << QStringLiteral("--progress-delta") << QStringLiteral("0.2");

    if (options.resumePartial) {
        args << QStringLiteral("-c");
    }

    if (!options.outputDir.isEmpty()) {
        QDir().mkpath(options.outputDir);
        args << QStringLiteral("-P") << (QStringLiteral("home:") + options.outputDir);
    }
    const QString scratchDir = !options.tempDir.isEmpty() ? options.tempDir : options.previewWorkDir;
    if (!scratchDir.isEmpty()) {
        QDir().mkpath(scratchDir);
        // Fragments and thumbnail files stay out of the download folder.
        // An absolute -o would make this ignored.
        args << QStringLiteral("-P") << (QStringLiteral("temp:") + scratchDir);
    }
    if (options.previewMode && !scratchDir.isEmpty()) {
        args << QStringLiteral("--no-part");
    }

    if (ffmpegPath.startsWith(QLatin1Char('/'))) {
        args << QStringLiteral("--ffmpeg-location") << ffmpegPath;
    }

    const QString jsRuntime = jsRuntimeSpec();
    if (!jsRuntime.isEmpty()) {
        args << QStringLiteral("--js-runtimes") << jsRuntime;
    }

    if (options.useCookies) {
        const QString browser = cookiesBrowserName(options.cookiesBrowser);
        if (!browser.isEmpty()) {
            args << QStringLiteral("--cookies-from-browser") << browser;
        } else if (!options.cookiesFile.isEmpty() && QFile::exists(options.cookiesFile)) {
            args << QStringLiteral("--cookies") << options.cookiesFile;
        }
    }

    // Relative template. An absolute -o disables --paths, and the pieces
    // then appear in the download folder until yt-dlp deletes them.
    args << QStringLiteral("-o")
         << (outputBaseName(options.outputTemplate) + QStringLiteral(".%(ext)s"));

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
        args << QStringLiteral("-f")
             << buildVideoAudioFormat(options.videoQuality, options.videoCodec, options.previewMode,
                                      !options.previewMode && effectiveContainer == VideoContainer::MP4);
        args << QStringLiteral("--merge-output-format") << containerExt(effectiveContainer);
        break;
    case DownloadMode::VideoOnly:
        if (!options.previewMode && effectiveContainer == VideoContainer::MP4) {
            args << QStringLiteral("-f") << buildMp4VideoOnlyFormat(options.videoQuality, options.videoCodec);
        } else {
            args << QStringLiteral("-f") << buildVideoOnlyFormat(options.videoQuality, options.videoCodec);
        }
        args << QStringLiteral("--remux-video") << containerExt(effectiveContainer);
        break;
    case DownloadMode::AudioOnly:
        if (options.audioCodec == AudioCodec::Any) {
            args << QStringLiteral("-f") << buildAudioFormat(options.audioQuality);
        } else {
            // Re-encoding sets the bitrate. Filtering the source by abr would throw that away.
            args << QStringLiteral("-f") << QStringLiteral("bestaudio/best");
        }
        args << QStringLiteral("-x");
        if (options.audioCodec != AudioCodec::Any) {
            args << QStringLiteral("--audio-format") << audioCodecExt(options.audioCodec);
            args << QStringLiteral("--audio-quality") << audioQualityValue(options.audioQuality);
        }
        break;
    }

    addSubtitleArgs(args, options);

    if (options.embedMetadata) {
        args << QStringLiteral("--embed-metadata");
    }
    if (options.embedThumbnail && options.mode != DownloadMode::VideoOnly) {
        args << QStringLiteral("--embed-thumbnail");
        args << QStringLiteral("--convert-thumbnails") << QStringLiteral("jpg");
    }
    if (options.sponsorBlockRemove) {
        args << QStringLiteral("--sponsorblock-remove") << QStringLiteral("all");
    }

    if (options.mode != DownloadMode::AudioOnly) {
        args << QStringLiteral("--ppa") << QStringLiteral("Merger+ffmpeg:-avoid_negative_ts make_zero");
    }

    const QString limit = speedLimitRate(options.speedLimit);
    if (!limit.isEmpty()) {
        args << QStringLiteral("--limit-rate") << limit;
    }

    const QString section = normalizeDownloadSection(options.downloadSections);
    if (!section.isEmpty()) {
        args << QStringLiteral("--download-sections") << section;
    }

    args << QStringLiteral("--concurrent-fragments")
         << (options.previewMode ? QStringLiteral("8") : QStringLiteral("4"));
    args << QStringLiteral("--retries") << QStringLiteral("10");
    args << QStringLiteral("--fragment-retries") << QStringLiteral("10");

    args << sanitizeCustomArgs(options.customArgs, options.allowUnsafeArgs);
    args << options.url;

    return args;
}

QString cookiesBrowserName(int id)
{
    switch (id) {
    case 1:
        return QStringLiteral("firefox");
    case 2:
        return QStringLiteral("chrome");
    case 3:
        return QStringLiteral("chromium");
    case 4:
        return QStringLiteral("brave");
    case 5:
        return QStringLiteral("edge");
    case 6:
        return QStringLiteral("vivaldi");
    default:
        return {};
    }
}

QString extractHttpUrl(const QString &text)
{
    static const QRegularExpression urlRe(QStringLiteral(R"(https?://[^\s<>"']+)"));
    const QRegularExpressionMatch match = urlRe.match(text);
    if (!match.hasMatch()) {
        return {};
    }
    QString url = match.captured(0);
    while (!url.isEmpty()) {
        const QChar tail = url.back();
        if (tail != QLatin1Char(')') && tail != QLatin1Char(',') && tail != QLatin1Char('.')
            && tail != QLatin1Char('>') && tail != QLatin1Char(']') && tail != QLatin1Char(';')) {
            break;
        }
        url.chop(1);
    }
    const int scheme = url.indexOf(QStringLiteral("://"));
    if (scheme < 0 || scheme + 3 >= url.size()) {
        return {};
    }
    return url;
}

bool pathIsInside(const QString &path, const QString &directory)
{
    if (path.isEmpty() || directory.isEmpty()) {
        return false;
    }
    const QString root = QDir::cleanPath(QDir(directory).absolutePath());
    const QString absolute = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    return absolute == root || absolute.startsWith(root + QLatin1Char('/'));
}

QString finishedDownloadPath(const QString &line)
{
    const QString prefix = QStringLiteral("[download] ");
    const QString marker = QStringLiteral(" has already been downloaded");
    if (!line.startsWith(prefix)) {
        return {};
    }
    const int markerAt = line.indexOf(marker);
    if (markerAt <= prefix.size()) {
        return {};
    }
    return line.mid(prefix.size(), markerAt - prefix.size()).trimmed();
}

ParsedProgress parseProgressLine(const QString &line)
{
    ParsedProgress parsed;
    if (!line.startsWith(QStringLiteral("@@"))) {
        return parsed;
    }
    const QStringList parts = line.split(QStringLiteral("@@"));
    if (parts.size() < 4) {
        return parsed;
    }
    QString percent = parts.at(1).trimmed();
    if (percent.endsWith(QLatin1Char('%'))) {
        percent.chop(1);
    }
    bool ok = false;
    const double value = percent.trimmed().toDouble(&ok);
    if (!ok) {
        return parsed;
    }

    const auto keepMetric = [](const QString &text) {
        const QString trimmed = text.trimmed();
        if (trimmed.isEmpty() || trimmed.compare(QStringLiteral("NA"), Qt::CaseInsensitive) == 0
            || trimmed.contains(QStringLiteral("Unknown"), Qt::CaseInsensitive)) {
            return QString();
        }
        return trimmed;
    };

    parsed.valid = true;
    parsed.percent = value;
    parsed.speed = keepMetric(parts.at(2));
    parsed.eta = keepMetric(parts.at(3));
    return parsed;
}

static QList<int> versionNumbers(QString text)
{
    text = text.trimmed();
    if (text.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        text = text.mid(1);
    }
    const int suffix = text.indexOf(QLatin1Char('-'));
    if (suffix >= 0) {
        text = text.left(suffix);
    }
    QList<int> numbers;
    const QStringList parts = text.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        bool ok = false;
        const int value = part.toInt(&ok);
        if (!ok) {
            break;
        }
        numbers.append(value);
    }
    return numbers;
}

int compareVersions(const QString &left, const QString &right)
{
    const QList<int> a = versionNumbers(left);
    const QList<int> b = versionNumbers(right);
    const int count = std::max(a.size(), b.size());
    for (int i = 0; i < count; ++i) {
        const int av = i < a.size() ? a.at(i) : 0;
        const int bv = i < b.size() ? b.at(i) : 0;
        if (av != bv) {
            return av > bv ? 1 : -1;
        }
    }
    return 0;
}

} // namespace YtdlpFormats
