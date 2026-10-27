// SPDX-License-Identifier: MIT

#include "ytdlpformats.h"

#include <QCoreApplication>

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

    const QString local = localYtdlpPath();
    expect(local.endsWith(QStringLiteral("/com.zrn59.ytdlp/bin/yt-dlp")), "yt-dlp path is private to the widget");
    expect(!local.contains(QStringLiteral("/.local/bin/yt-dlp")), "widget yt-dlp is not ~/.local/bin/yt-dlp");

    if (g_failed != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failed);
        return 1;
    }
    std::printf("ok\n");
    return 0;
}
