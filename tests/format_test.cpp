// SPDX-License-Identifier: MIT

#include "ytdlpformats.h"

#include <QCoreApplication>
#include <QLocale>

#include <cmath>
#include <cstdio>

using namespace YtdlpFormats;

namespace {

int g_failed = 0;

void expect(bool condition, const char *message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++g_failed;
    }
}

bool contains(const QStringList &args, const QString &value)
{
    return args.contains(value);
}

QString joined(const QStringList &args)
{
    return args.join(QLatin1Char(' '));
}

QString flagValue(const QStringList &args, const QString &flag)
{
    const int index = args.indexOf(flag);
    if (index < 0 || index + 1 >= args.size()) {
        return {};
    }
    return args.at(index + 1);
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    expect(normalizeDownloadSection(QStringLiteral("10:00-15:00")) == QStringLiteral("*10:00-15:00"),
           "time ranges get a leading *");
    expect(normalizeDownloadSection(QStringLiteral("*1-2")) == QStringLiteral("*1-2"),
           "existing section markers stay");
    expect(normalizeDownloadSection(QStringLiteral("  ")) .isEmpty(), "blank sections stay empty");

    const QStringList safe = sanitizeCustomArgs(
        QStringLiteral("--exec echo hi --sponsorblock-remove all -o /tmp/out --output=elsewhere"));
    expect(!safe.contains(QStringLiteral("--exec")), "exec flag is removed");
    expect(!safe.contains(QStringLiteral("echo")), "exec command is removed");
    expect(!safe.contains(QStringLiteral("-o")), "output flag is removed");
    expect(!safe.contains(QStringLiteral("/tmp/out")), "output path is removed");
    expect(!safe.contains(QStringLiteral("--output=elsewhere")), "output= form is removed");
    expect(safe.contains(QStringLiteral("--sponsorblock-remove")), "safe flags are kept");
    expect(safe.contains(QStringLiteral("all")), "safe flag values are kept");

    DownloadOptions video;
    video.url = QStringLiteral("https://example.com/watch");
    video.outputDir = QStringLiteral("/tmp/videos");
    video.mode = DownloadMode::VideoWithAudio;
    video.videoQuality = VideoQuality::FHD;
    const QStringList videoArgs = buildArgs(video, QStringLiteral("/usr/bin/ffmpeg"));
    expect(videoArgs.constLast() == video.url, "url is the last argument");
    expect(contains(videoArgs, QStringLiteral("--ignore-config")), "global yt-dlp config is ignored");
    expect(contains(videoArgs, QStringLiteral("--no-playlist")), "playlists are off by default");
    expect(!joined(videoArgs).contains(QStringLiteral("--exec")), "built command has no exec");
    expect(joined(videoArgs).contains(QStringLiteral("%(title).200B.%(ext)s")),
           "output name is the title only");
    expect(!joined(videoArgs).contains(QStringLiteral("%(id)s")), "video id is not part of the filename");
    expect(!joined(videoArgs).contains(QStringLiteral("audio:")), "audio: is not used as a path");
    expect(!joined(videoArgs).contains(QStringLiteral("video:")), "video: is not used as a path");
    expect(!joined(videoArgs).contains(QStringLiteral(".audio.")), "final name has no .audio. marker");
    expect(!joined(videoArgs).contains(QStringLiteral(".video.")), "final name has no .video. marker");
    expect(joined(videoArgs).contains(QStringLiteral("[height<=1080]")), "1080p cap is applied");

    DownloadOptions named = video;
    named.outputTemplate = QStringLiteral("%(upload_date)s %(title).80B");
    named.sponsorBlockRemove = true;
    const QString namedJoined = joined(buildArgs(named, QStringLiteral("/usr/bin/ffmpeg")));
    expect(namedJoined.contains(QStringLiteral("%(upload_date)s %(title).80B.%(ext)s")),
           "custom file name template is used");
    expect(contains(buildArgs(named, QStringLiteral("/usr/bin/ffmpeg")), QStringLiteral("--sponsorblock-remove")),
           "sponsorblock removal is optional");
    expect(namedJoined.contains(QStringLiteral("--sponsorblock-remove all")),
           "sponsorblock removes all categories");

    DownloadOptions escaped = video;
    escaped.outputTemplate = QStringLiteral("../secret/%(title)s.%(ext)s");
    const QString escapedJoined = joined(buildArgs(escaped, QStringLiteral("/usr/bin/ffmpeg")));
    expect(escapedJoined.contains(QStringLiteral("%(title).200B.%(ext)s")),
           "path-like templates fall back to the title");
    expect(!escapedJoined.contains(QStringLiteral("../")), "templates cannot leave the folder");
    expect(!escapedJoined.contains(QStringLiteral("%(ext)s.%(ext)s")), "extension is added once");

    DownloadOptions preview = video;
    preview.previewMode = true;
    preview.previewWorkDir = QStringLiteral("/tmp/com.zrn59.ytdlp-preview");
    const QStringList previewArgs = buildArgs(preview, QStringLiteral("/usr/bin/ffmpeg"));
    expect(contains(previewArgs, QStringLiteral("-P")), "preview intermediates use a temp path");
    expect(contains(previewArgs, QStringLiteral("temp:/tmp/com.zrn59.ytdlp-preview")),
           "preview intermediates stay out of the download folder");
    expect(contains(previewArgs, QStringLiteral("bestaudio+bestvideo[height<=1080][ext=webm]/bestaudio+bestvideo[height<=1080]/best")),
           "preview downloads audio first and prefers webm video");
    const int previewOutput = previewArgs.indexOf(QStringLiteral("-o"));
    expect(previewOutput >= 0 && previewOutput + 1 < previewArgs.size(), "preview still has an output template");
    if (previewOutput >= 0 && previewOutput + 1 < previewArgs.size()) {
        expect(previewArgs.at(previewOutput + 1) == QStringLiteral("%(title).200B.%(ext)s"),
               "output template is relative so temporary files can use a separate folder");
    }
    expect(joined(videoArgs).contains(QStringLiteral("vcodec^=av01")),
           "mp4 downloads avoid vp9, which breaks the start of the file in VLC");
    expect(contains(videoArgs, QStringLiteral("Merger+ffmpeg:-avoid_negative_ts make_zero")),
           "merged files start at zero so VLC does not break the opening seconds");
    expect(contains(videoArgs, QStringLiteral("--convert-thumbnails")),
           "thumbnails are converted before embedding");
    const int formatIndex = videoArgs.indexOf(QStringLiteral("-f"));
    expect(formatIndex >= 0 && formatIndex + 1 < videoArgs.size(), "format flag is present");
    if (formatIndex >= 0 && formatIndex + 1 < videoArgs.size()) {
        expect(!videoArgs.at(formatIndex + 1).contains(QLatin1Char('%')),
               "quality placeholder is substituted");
    }

    DownloadOptions audio = video;
    audio.mode = DownloadMode::AudioOnly;
    audio.outputDir = QStringLiteral("/tmp/music");
    const QStringList audioArgs = buildArgs(audio, QStringLiteral("/usr/bin/ffmpeg"));
    expect(contains(audioArgs, QStringLiteral("-x")), "audio mode extracts audio");
    expect(!joined(audioArgs).contains(QStringLiteral(".audio.")), "audio-only files keep a clean name");
    expect(!joined(audioArgs).contains(QStringLiteral(".video.")), "audio-only files are not video parts");

    DownloadOptions resume = video;
    resume.resumePartial = true;
    resume.allowPlaylist = true;
    resume.customArgs = QStringLiteral("--exec touch /tmp/pwned");
    const QStringList resumeArgs = buildArgs(resume, QStringLiteral("/usr/bin/ffmpeg"));
    expect(contains(resumeArgs, QStringLiteral("-c")), "retry continues partial files");
    expect(contains(resumeArgs, QStringLiteral("--yes-playlist")), "playlist flag is honored");
    expect(!resumeArgs.contains(QStringLiteral("--exec")), "custom exec is still stripped");
    expect(!resumeArgs.contains(QStringLiteral("touch")), "custom exec command is stripped");

    DownloadOptions unlocked = resume;
    unlocked.allowUnsafeArgs = true;
    const QStringList unlockedArgs = buildArgs(unlocked, QStringLiteral("/usr/bin/ffmpeg"));
    expect(unlockedArgs.contains(QStringLiteral("--exec")), "risky flags pass when the user allows them");
    expect(unlockedArgs.contains(QStringLiteral("touch")), "the command after a risky flag is kept");

    const QString local = localYtdlpPath();
    expect(local.endsWith(QStringLiteral("/com.zrn59.ytdlp/bin/yt-dlp")), "yt-dlp path is private to the widget");
    expect(!local.contains(QStringLiteral("/.local/bin/yt-dlp")), "widget yt-dlp is not ~/.local/bin/yt-dlp");

    DownloadOptions vp9 = video;
    vp9.videoContainer = VideoContainer::MKV;
    vp9.videoQuality = VideoQuality::Best;
    vp9.videoCodec = VideoCodec::VP9;
    const QString vp9Format = flagValue(buildArgs(vp9, QStringLiteral("/usr/bin/ffmpeg")), QStringLiteral("-f"));
    expect(vp9Format.startsWith(QStringLiteral("bestvideo[vcodec~='^(vp09|vp9)']")),
           "best + VP9 keeps the selected codec");

    DownloadOptions h265 = video;
    h265.videoCodec = VideoCodec::H265;
    const QString h265Format = flagValue(buildArgs(h265, QStringLiteral("/usr/bin/ffmpeg")), QStringLiteral("-f"));
    expect(h265Format.contains(QStringLiteral("hev1")), "explicit H.265 is requested");
    expect(!h265Format.contains(QStringLiteral("vcodec^=av01")), "explicit H.265 does not prefer AV1");

    DownloadOptions h264 = video;
    h264.videoQuality = VideoQuality::Best;
    h264.videoCodec = VideoCodec::H264;
    const QString h264Format = flagValue(buildArgs(h264, QStringLiteral("/usr/bin/ffmpeg")), QStringLiteral("-f"));
    expect(h264Format.contains(QStringLiteral("avc1")), "explicit H.264 is requested");
    expect(!h264Format.contains(QStringLiteral("av01")), "explicit H.264 does not prefer AV1");

    DownloadOptions mp3 = audio;
    mp3.audioCodec = AudioCodec::MP3;
    mp3.audioQuality = AudioQuality::Medium128;
    const QStringList mp3Args = buildArgs(mp3, QStringLiteral("/usr/bin/ffmpeg"));
    expect(flagValue(mp3Args, QStringLiteral("--audio-quality")) == QStringLiteral("128K"),
           "MP3 bitrate follows the quality setting");
    expect(flagValue(mp3Args, QStringLiteral("-f")) == QStringLiteral("bestaudio/best"),
           "converted audio starts from the best source");
    expect(!joined(mp3Args).contains(QStringLiteral("Merger+ffmpeg")),
           "audio extraction does not pass merger arguments");

    DownloadOptions sourceAudio = audio;
    sourceAudio.audioQuality = AudioQuality::Medium128;
    const QStringList sourceArgs = buildArgs(sourceAudio, QStringLiteral("/usr/bin/ffmpeg"));
    expect(flagValue(sourceArgs, QStringLiteral("-f")).contains(QStringLiteral("abr<=128")),
           "audio without conversion filters by bitrate");
    expect(!sourceArgs.contains(QStringLiteral("--audio-quality")),
           "source audio is not re-encoded");

    const ParsedProgress slow = parseProgressLine(QStringLiteral("@@  2.8%@@   6.59MiB/s@@00:00"));
    expect(slow.valid && std::abs(slow.percent - 2.8) < 0.001 && slow.speed == QStringLiteral("6.59MiB/s")
               && slow.eta == QStringLiteral("00:00"),
           "progress template lines are parsed");
    const ParsedProgress unknown = parseProgressLine(QStringLiteral("@@  0.4%@@ Unknown B/s@@Unknown"));
    expect(unknown.valid && std::abs(unknown.percent - 0.4) < 0.001 && unknown.speed.isEmpty() && unknown.eta.isEmpty(),
           "unknown progress metrics are hidden");
    const ParsedProgress done = parseProgressLine(QStringLiteral("@@100.0%@@1.11MiB/s@@NA"));
    expect(done.valid && std::abs(done.percent - 100.0) < 0.001 && done.speed == QStringLiteral("1.11MiB/s") && done.eta.isEmpty(),
           "NA eta is hidden");
    expect(!parseProgressLine(QStringLiteral("[download]  12.5%")).valid, "plain progress is not a template line");

    expect(finishedDownloadPath(QStringLiteral("[download] /tmp/My Video.webm has already been downloaded"))
               == QStringLiteral("/tmp/My Video.webm"),
           "already downloaded path is kept");
    expect(finishedDownloadPath(QStringLiteral("[download] Destination: /tmp/a.webm")).isEmpty(),
           "destination lines are not treated as already downloaded");

    expect(extractHttpUrl(QStringLiteral("see https://example.com/a/b?x=1).")) == QStringLiteral("https://example.com/a/b?x=1"),
           "a url can be pulled out of surrounding text");
    expect(extractHttpUrl(QStringLiteral("https://")).isEmpty(), "a bare scheme is not a url");

    expect(pathIsInside(QStringLiteral("/tmp/cache/1"), QStringLiteral("/tmp/cache")), "child is inside");
    expect(!pathIsInside(QStringLiteral("/tmp/cache-evil"), QStringLiteral("/tmp/cache")), "prefix is not containment");
    expect(!pathIsInside(QStringLiteral("/tmp/cache/../etc"), QStringLiteral("/tmp/cache")), "parent segments cannot escape");

    const QStringList dangerous = sanitizeCustomArgs(
        QStringLiteral("--plugin-dirs /tmp/pwn --netrc-cmd id --alias pwn --exec id --downloader /tmp/evil --ffmpeg-location /tmp/ff --limit-rate 1M"));
    expect(!dangerous.contains(QStringLiteral("--plugin-dirs")), "plugin dirs are removed");
    expect(!dangerous.contains(QStringLiteral("/tmp/pwn")), "plugin path is removed");
    expect(!dangerous.contains(QStringLiteral("--netrc-cmd")), "netrc command is removed");
    expect(!dangerous.contains(QStringLiteral("--alias")), "aliases are removed");
    expect(!dangerous.contains(QStringLiteral("--exec")), "exec stays removed");
    expect(!dangerous.contains(QStringLiteral("--downloader")), "custom downloader is removed");
    expect(!dangerous.contains(QStringLiteral("/tmp/evil")), "downloader path is removed");
    expect(!dangerous.contains(QStringLiteral("--ffmpeg-location")), "ffmpeg location override is removed");
    expect(!dangerous.contains(QStringLiteral("/tmp/ff")), "ffmpeg override path is removed");
    expect(!dangerous.contains(QStringLiteral("id")), "command text is removed");
    expect(!dangerous.contains(QStringLiteral("pwn")), "alias name is removed");
    expect(dangerous.contains(QStringLiteral("--limit-rate")), "rate limit is kept");
    expect(dangerous.contains(QStringLiteral("1M")), "rate limit value is kept");

    expect(compareVersions(QStringLiteral("v2.0.1"), QStringLiteral("2.0.1")) == 0, "a leading v does not change the version");
    expect(compareVersions(QStringLiteral("2.0.2"), QStringLiteral("2.0.1")) > 0, "a newer patch compares higher");
    expect(compareVersions(QStringLiteral("2.0.1"), QStringLiteral("2.1.0")) < 0, "a higher minor version compares higher");
    expect(compareVersions(QStringLiteral("2.0"), QStringLiteral("2.0.0")) == 0, "a missing part counts as zero");
    expect(compareVersions(QStringLiteral("2.0.1-1"), QStringLiteral("2.0.1")) == 0, "a suffix after a dash is ignored");

    DownloadOptions subs = video;
    subs.subtitleOption = SubtitleOption::Manual;
    subs.subtitleLanguage = SubtitleLanguage::Auto;
    const QString subLang = flagValue(buildArgs(subs, QStringLiteral("/usr/bin/ffmpeg")), QStringLiteral("--sub-langs"));
    const QString expectedLang = QLocale::system().name().section(QLatin1Char('_'), 0, 0);
    if (!expectedLang.isEmpty() && expectedLang != QLatin1String("C") && expectedLang != QLatin1String("POSIX")) {
        expect(subLang == expectedLang, "auto subtitles follow the system language");
    }

    DownloadOptions browserCookies = video;
    browserCookies.useCookies = true;
    browserCookies.cookiesBrowser = 1;
    const QStringList browserArgs = buildArgs(browserCookies, QStringLiteral("/usr/bin/ffmpeg"));
    expect(flagValue(browserArgs, QStringLiteral("--cookies-from-browser")) == QStringLiteral("firefox"),
           "firefox cookies are read from the browser");
    expect(!browserArgs.contains(QStringLiteral("--cookies")), "a browser profile replaces the cookies file");
    expect(cookiesBrowserName(0).isEmpty(), "zero keeps the cookies file");
    expect(cookiesBrowserName(99).isEmpty(), "unknown browser ids are ignored");

    expect(contains(videoArgs, QStringLiteral("--progress-template")), "progress uses a stable template");
    DownloadOptions limited = video;
    limited.speedLimit = SpeedLimit::Medium5M;
    expect(flagValue(buildArgs(limited, QStringLiteral("/usr/bin/ffmpeg")), QStringLiteral("--limit-rate")) == QStringLiteral("5M"),
           "speed limit uses yt-dlp size units");

    if (g_failed != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("ok\n");
    return 0;
}
