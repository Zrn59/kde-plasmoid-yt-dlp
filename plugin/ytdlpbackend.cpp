// SPDX-License-Identifier: MIT

#include "ytdlpbackend.h"

#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <csignal>
#include <unistd.h>
#include <utility>

#include <KLocalizedString>
#include <KNotification>

using namespace YtdlpFormats;

namespace {

QString stripAnsi(const QString &text)
{
    static const QRegularExpression ansiRe(QStringLiteral(R"(\x1B\[[0-9;]*[A-Za-z])"));
    QString cleaned = text;
    cleaned.remove(ansiRe);
    return cleaned.trimmed();
}

QString sanitizeYtdlpMessage(const QString &line)
{
    QString msg = stripAnsi(line);
    if (msg.contains(QStringLiteral("ERROR:"), Qt::CaseInsensitive)) {
        msg = msg.section(QStringLiteral("ERROR:"), 1).trimmed();
    } else if (msg.contains(QStringLiteral("error:"))) {
        msg = msg.section(QStringLiteral("error:"), 1).trimmed();
    }
    static const QRegularExpression bracketPrefix(QStringLiteral(R"(^\[[^\]]+\]\s*)"));
    msg.remove(bracketPrefix);
    return msg.trimmed();
}

QString extractYtdlpError(const QString &text)
{
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QString lastError;
    for (const QString &rawLine : lines) {
        const QString line = stripAnsi(rawLine);
        if (line.contains(QStringLiteral("ERROR"), Qt::CaseInsensitive)) {
            lastError = sanitizeYtdlpMessage(line);
        }
    }
    return lastError;
}

bool isSidecarPath(const QString &path)
{
    QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QLatin1String("part")) {
        ext = QFileInfo(QFileInfo(path).completeBaseName()).suffix().toLower();
    }
    return ext == QLatin1String("vtt")
        || ext == QLatin1String("srt")
        || ext == QLatin1String("ass")
        || ext == QLatin1String("lrc")
        || ext == QLatin1String("json")
        || ext == QLatin1String("jpg")
        || ext == QLatin1String("jpeg")
        || ext == QLatin1String("png")
        || ext == QLatin1String("webp")
        || ext == QLatin1String("description");
}

bool hasStreamRole(const QString &path, const QString &role)
{
    QString name = QFileInfo(path).fileName();
    if (name.endsWith(QStringLiteral(".part"), Qt::CaseInsensitive)) {
        name.chop(5);
    }
    static const QRegularExpression roleRe(
        QStringLiteral(R"(\.(audio|video)(?:\.f\d+)?\.[^.]+$)"),
        QRegularExpression::CaseInsensitiveOption);
    const auto match = roleRe.match(name);
    return match.hasMatch() && match.captured(1).compare(role, Qt::CaseInsensitive) == 0;
}

QProcessEnvironment runtimeEnvironment()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("PATH"), YtdlpFormats::processPath());
    env.insert(QStringLiteral("NO_COLOR"), QStringLiteral("1"));
    env.insert(QStringLiteral("PYTHONUNBUFFERED"), QStringLiteral("1"));
    return env;
}

} // namespace

YtdlpBackend::YtdlpBackend(QObject *parent)
    : QObject(parent)
    , m_downloadModel(this)
    , m_ytdlpPath(findYtdlp())
    , m_ffmpegPath(findFfmpeg())
{
    refreshYtdlpVersion();
}

YtdlpBackend::~YtdlpBackend()
{
    const auto processes = findChildren<QProcess *>(Qt::FindDirectChildrenOnly);
    for (QProcess *process : processes) {
        process->blockSignals(true);
        if (process->state() != QProcess::NotRunning) {
            process->kill();
            process->waitForFinished(200);
        }
    }
    m_versionProcess = nullptr;

    for (auto *entry : std::as_const(m_active)) {
        destroyEntry(entry);
    }
    m_active.clear();
}

void YtdlpBackend::destroyEntry(DownloadEntry *entry)
{
    if (!entry) {
        return;
    }
    stopPreviewMux(entry);
    if (entry->process) {
        if (entry->process->state() != QProcess::NotRunning) {
            signalProcess(entry, SIGKILL);
            entry->process->kill();
        }
        entry->process->deleteLater();
        entry->process = nullptr;
    }
    delete entry;
}

void YtdlpBackend::configureChildProcess(QProcess *process)
{
    if (!process) {
        return;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    process->setChildProcessModifier([] {
        setpgid(0, 0);
    });
#endif
}

bool YtdlpBackend::signalProcess(DownloadEntry *entry, int sig) const
{
    if (!entry || !entry->process) {
        return false;
    }
    const qint64 pid64 = entry->process->processId();
    if (pid64 <= 0) {
        return false;
    }
    const pid_t pid = static_cast<pid_t>(pid64);
    if (entry->ownProcessGroup) {
        if (::kill(-pid, sig) == 0) {
            return true;
        }
    }
    return ::kill(pid, sig) == 0;
}

QVariantList YtdlpBackend::failedDownloads() const
{
    return m_failedDownloads;
}

QVariantList YtdlpBackend::activeDownloads() const
{
    return m_activeDownloads;
}

YtdlpActiveModel *YtdlpBackend::downloadModel()
{
    return &m_downloadModel;
}

int YtdlpBackend::activeCount() const
{
    int count = 0;
    for (auto *entry : m_active) {
        if (entry && !entry->finished) {
            ++count;
        }
    }
    return count;
}

QString YtdlpBackend::lastError() const
{
    return m_lastError;
}

QString YtdlpBackend::ytdlpVersion() const
{
    return m_ytdlpVersion;
}

QString YtdlpBackend::ytdlpPath() const
{
    return m_ytdlpPath;
}

bool YtdlpBackend::updatingYtdlp() const
{
    return m_updatingYtdlp;
}

QString YtdlpBackend::defaultVideoFolder() const
{
    return QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
}

QString YtdlpBackend::defaultAudioFolder() const
{
    return QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
}

bool YtdlpBackend::isValidUrl(const QString &url) const
{
    const QString trimmed = url.trimmed();
    return trimmed.startsWith(QStringLiteral("http://")) || trimmed.startsWith(QStringLiteral("https://"));
}

QString YtdlpBackend::pasteFromClipboard() const
{
    return QGuiApplication::clipboard()->text(QClipboard::Clipboard).trimmed();
}

QString YtdlpBackend::selectFolder(const QString &current) const
{
    const QString start = current.isEmpty() ? QDir::homePath() : current;
    QWidget transientParent;
    return QFileDialog::getExistingDirectory(&transientParent,
                                             i18nc("@title:window", "Select download folder"),
                                             start,
                                             QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
}

QString YtdlpBackend::defaultCookiesFile() const
{
    return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)
        + QStringLiteral("/www.youtube.com_cookies.txt");
}

QString YtdlpBackend::selectCookiesFile(const QString &current) const
{
    const QString start = current.isEmpty() ? defaultCookiesFile() : current;
    const QString dir = QFileInfo(start).absolutePath();
    QWidget transientParent;
    return QFileDialog::getOpenFileName(&transientParent,
                                        i18nc("@title:window", "Select cookies file"),
                                        start.isEmpty() ? dir : start,
                                        i18nc("@item:inmenu", "Cookies (*.txt);;All files (*)"));
}

void YtdlpBackend::openFolder(const QString &path) const
{
    if (path.isEmpty()) {
        return;
    }
    QFileInfo info(path);
    const QString folder = info.isDir() ? path : info.absolutePath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

void YtdlpBackend::openFile(const QString &path) const
{
    if (path.isEmpty()) {
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void YtdlpBackend::clearError()
{
    if (m_lastError.isEmpty()) {
        return;
    }
    m_lastError.clear();
    Q_EMIT lastErrorChanged();
}

void YtdlpBackend::setError(const QString &message)
{
    if (m_lastError == message) {
        return;
    }
    m_lastError = message;
    Q_EMIT lastErrorChanged();
}

void YtdlpBackend::refreshYtdlpVersion()
{
    if (m_ytdlpPath.isEmpty()) {
        if (m_ytdlpVersion != i18n("not found")) {
            m_ytdlpVersion = i18n("not found");
            Q_EMIT ytdlpVersionChanged();
        }
        return;
    }

    if (m_versionProcess) {
        m_versionProcess->kill();
        m_versionProcess->deleteLater();
        m_versionProcess = nullptr;
    }

    auto *process = new QProcess(this);
    m_versionProcess = process;
    process->setProgram(m_ytdlpPath);
    process->setArguments({QStringLiteral("--version")});
    process->setProcessEnvironment(runtimeEnvironment());

    connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus) {
        if (m_versionProcess != process) {
            process->deleteLater();
            return;
        }
        m_versionProcess = nullptr;
        const QString out = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
        const QString version = (exitCode == 0 && !out.isEmpty()) ? out : i18n("not found");
        if (m_ytdlpVersion != version) {
            m_ytdlpVersion = version;
            Q_EMIT ytdlpVersionChanged();
        }
        process->deleteLater();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || m_versionProcess != process) {
            return;
        }
        m_versionProcess = nullptr;
        if (m_ytdlpVersion != i18n("not found")) {
            m_ytdlpVersion = i18n("not found");
            Q_EMIT ytdlpVersionChanged();
        }
        process->deleteLater();
    });
    process->start();
}

void YtdlpBackend::updateYtdlp()
{
    if (m_updatingYtdlp) {
        return;
    }

    const QString updateScript = YtdlpFormats::updateScriptPath();
    if (updateScript.isEmpty() || !QFileInfo(updateScript).isExecutable()) {
        setError(i18n("Updater is missing. Run install.sh again."));
        return;
    }

    m_updatingYtdlp = true;
    Q_EMIT updatingYtdlpChanged();

    auto *process = new QProcess(this);
    process->setProgram(updateScript);
    process->setProcessEnvironment(runtimeEnvironment());

    connect(process, &QProcess::finished, this,
            [this, process](int exitCode, QProcess::ExitStatus) {
                const QString output = QString::fromUtf8(process->readAllStandardOutput()
                                                         + process->readAllStandardError())
                                           .trimmed();
                m_ytdlpPath = findYtdlp();
                Q_EMIT ytdlpVersionChanged();
                refreshYtdlpVersion();

                m_updatingYtdlp = false;
                Q_EMIT updatingYtdlpChanged();

                QString line = output;
                const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                if (!lines.isEmpty()) {
                    line = lines.constLast().trimmed();
                }

                if (exitCode == 0) {
                    notify(i18n("yt-dlp"),
                           line.isEmpty() ? m_ytdlpPath : line,
                           QStringLiteral("system-software-update"),
                           5000);
                } else {
                    const QString msg = line.isEmpty() ? i18n("Update failed (code %1)", exitCode) : line;
                    setError(msg);
                    notify(i18n("yt-dlp update failed"), msg, QStringLiteral("dialog-error"), 8000);
                }
                process->deleteLater();
            });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) {
            return;
        }
        m_updatingYtdlp = false;
        Q_EMIT updatingYtdlpChanged();
        setError(i18n("Failed to start the yt-dlp updater."));
        process->deleteLater();
    });

    process->start();
}

YtdlpBackend::DownloadEntry *YtdlpBackend::findActiveEntry(int id)
{
    return m_active.value(id, nullptr);
}

YtdlpBackend::DownloadEntry *YtdlpBackend::findFailedEntry(int id)
{
    for (auto &entry : m_failed) {
        if (entry.id == id) {
            return &entry;
        }
    }
    return nullptr;
}

QVariantMap YtdlpBackend::entryToVariant(const DownloadEntry &entry, bool includeOptions) const
{
    QVariantMap item;
    item.insert(QStringLiteral("id"), entry.id);
    item.insert(QStringLiteral("url"), entry.displayUrl);
    item.insert(QStringLiteral("title"), entry.title);
    item.insert(QStringLiteral("status"), sanitizeYtdlpMessage(entry.status));
    item.insert(QStringLiteral("progress"), entry.progress);
    item.insert(QStringLiteral("speed"), entry.speed);
    item.insert(QStringLiteral("eta"), entry.eta);
    item.insert(QStringLiteral("failed"), entry.failed);
    item.insert(QStringLiteral("paused"), entry.paused);
    item.insert(QStringLiteral("previewEnabled"), entry.previewEnabled);
    item.insert(QStringLiteral("hasPreview"), !resolvePreviewPath(entry).isEmpty());
    item.insert(QStringLiteral("filePath"), entry.filePath);
    item.insert(QStringLiteral("outputDir"), entry.outputDir);
    item.insert(QStringLiteral("formatLabel"), entry.formatLabel);
    item.insert(QStringLiteral("errorLog"), entry.errorLog);
    item.insert(QStringLiteral("previewFile"), resolvePreviewPath(entry));
    if (includeOptions) {
        item.insert(QStringLiteral("options"), entry.options);
    }
    return item;
}

QString YtdlpBackend::resolvePreviewPath(const DownloadEntry &entry) const
{
    const auto tryPath = [](const QString &path) -> QString {
        if (path.isEmpty()) {
            return {};
        }
        if (QFile::exists(path) && QFileInfo(path).size() > 0) {
            return path;
        }
        const QString partPath = path + QStringLiteral(".part");
        if (QFile::exists(partPath) && QFileInfo(partPath).size() > 0) {
            return partPath;
        }
        return {};
    };

    if (entry.previewEnabled) {
        if (isAudioOnlyMode(entry)) {
            if (!entry.audioPartPath.isEmpty()) {
                const QString audioPath = tryPath(entry.audioPartPath);
                if (!audioPath.isEmpty()) {
                    return audioPath;
                }
            }
        } else if (!entry.previewMuxPath.isEmpty()) {
            const QString muxPath = tryPath(entry.previewMuxPath);
            if (!muxPath.isEmpty()) {
                return muxPath;
            }
        } else if (!entry.videoPartPath.isEmpty()) {
            const QString videoPath = tryPath(entry.videoPartPath);
            if (!videoPath.isEmpty()) {
                return videoPath;
            }
        }
    }

    QString path = tryPath(entry.previewPath);
    if (!path.isEmpty()) {
        return path;
    }
    path = tryPath(entry.filePath);
    if (!path.isEmpty()) {
        return path;
    }

    if (!entry.filePath.isEmpty()) {
        const QFileInfo info(entry.filePath);
        QDir dir(info.absolutePath());
        const QString base = info.completeBaseName();
        const QStringList parts = dir.entryList({base + QStringLiteral("*.part")}, QDir::Files, QDir::Time);
        if (!parts.isEmpty()) {
            return dir.filePath(parts.first());
        }
    }

    return {};
}

bool YtdlpBackend::isAudioOnlyMode(const DownloadEntry &entry)
{
    return entry.options.value(QStringLiteral("downloadMode")).toInt()
        == static_cast<int>(DownloadMode::AudioOnly);
}

QString YtdlpBackend::streamBaseName(const QString &path)
{
    QString name = QFileInfo(path).completeBaseName();
    static const QRegularExpression streamSuffixRe(
        QStringLiteral(R"(\.(?:f\d+|video|audio)$)"),
        QRegularExpression::CaseInsensitiveOption);
    name.remove(streamSuffixRe);
    name.remove(streamSuffixRe);
    return name;
}

bool YtdlpBackend::isLikelyAudioPart(const QString &path)
{
    QString probe = path;
    if (probe.endsWith(QStringLiteral(".part"), Qt::CaseInsensitive)) {
        probe.chop(5);
    }
    const QString lower = probe.toLower();
    static const QStringList suffixes = {
        QStringLiteral(".m4a"),
        QStringLiteral(".mp3"),
        QStringLiteral(".opus"),
        QStringLiteral(".ogg"),
        QStringLiteral(".oga"),
        QStringLiteral(".aac"),
        QStringLiteral(".flac"),
        QStringLiteral(".wav"),
    };
    for (const QString &suffix : suffixes) {
        if (lower.endsWith(suffix)) {
            return true;
        }
    }
    static const QRegularExpression audioFormatRe(
        QStringLiteral(R"(\.f(139|140|249|250|251|256|258|141)\.)"),
        QRegularExpression::CaseInsensitiveOption);
    return audioFormatRe.match(path).hasMatch();
}

QString YtdlpBackend::formatFromPath(const QString &path)
{
    if (path.isEmpty()) {
        return {};
    }
    QString ext = QFileInfo(path).suffix().toUpper();
    // strip .part
    if (ext == QStringLiteral("PART")) {
        const QString base = QFileInfo(path).completeBaseName();
        ext = QFileInfo(base).suffix().toUpper();
    }
    // ignore yt-dlp stream ids like f137
    if (ext.startsWith(QLatin1Char('F')) && ext.mid(1).toInt() > 0) {
        return {};
    }
    return ext;
}

void YtdlpBackend::appendLog(DownloadEntry *entry, const QString &chunk)
{
    if (!entry || chunk.isEmpty()) {
        return;
    }
    entry->errorLog.append(chunk);
    if (entry->errorLog.size() > kMaxLogChars) {
        entry->errorLog = entry->errorLog.right(kMaxLogChars);
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (entry->lastLogEmitMs == 0 || now - entry->lastLogEmitMs >= kLogEmitIntervalMs) {
        entry->lastLogEmitMs = now;
        m_downloadModel.updateErrorLog(entry->id, entry->errorLog);
    }
}

void YtdlpBackend::ensurePreviewMuxTimer(DownloadEntry *entry)
{
    if (!entry || !entry->previewEnabled || isAudioOnlyMode(*entry) || entry->muxTimer) {
        return;
    }

    entry->muxTimer = new QTimer(this);
    entry->muxTimer->setInterval(3000);
    const int id = entry->id;
    connect(entry->muxTimer, &QTimer::timeout, this, [this, id]() {
        if (auto *active = findActiveEntry(id)) {
            updatePreviewMux(active);
        }
    });
    entry->muxTimer->start();
}

void YtdlpBackend::stopPreviewMux(DownloadEntry *entry)
{
    if (!entry) {
        return;
    }
    if (entry->muxTimer) {
        entry->muxTimer->stop();
        entry->muxTimer->deleteLater();
        entry->muxTimer = nullptr;
    }
    if (entry->muxProcess) {
        if (entry->muxProcess->state() != QProcess::NotRunning) {
            entry->muxProcess->kill();
        }
        entry->muxProcess->deleteLater();
        entry->muxProcess = nullptr;
    }
    if (!entry->previewMuxPath.isEmpty()) {
        QFile::remove(entry->previewMuxPath + QStringLiteral(".tmp"));
    }
}

void YtdlpBackend::updatePreviewMux(DownloadEntry *entry)
{
    if (!entry || !entry->previewEnabled || entry->paused || isAudioOnlyMode(*entry)) {
        return;
    }
    if (entry->videoPartPath.isEmpty() || entry->audioPartPath.isEmpty()) {
        return;
    }
    if (QFileInfo(entry->videoPartPath).size() < 128 * 1024
        || QFileInfo(entry->audioPartPath).size() < 64 * 1024) {
        return;
    }
    if (entry->muxProcess && entry->muxProcess->state() != QProcess::NotRunning) {
        return;
    }
    if (m_ffmpegPath.isEmpty()) {
        return;
    }

    if (entry->previewMuxPath.isEmpty()) {
        const QFileInfo videoInfo(entry->videoPartPath);
        entry->previewMuxPath = videoInfo.dir().filePath(streamBaseName(entry->videoPartPath)
                                                          + QStringLiteral(".watch.mkv"));
    }

    const QString tempPath = entry->previewMuxPath + QStringLiteral(".tmp");

    auto *process = new QProcess(this);
    entry->muxProcess = process;
    process->setProgram(m_ffmpegPath);
    process->setArguments({QStringLiteral("-nostdin"),
                           QStringLiteral("-loglevel"),
                           QStringLiteral("error"),
                           QStringLiteral("-y"),
                           QStringLiteral("-fflags"),
                           QStringLiteral("+genpts+discardcorrupt+igndts"),
                           QStringLiteral("-i"),
                           entry->videoPartPath,
                           QStringLiteral("-i"),
                           entry->audioPartPath,
                           QStringLiteral("-c"),
                           QStringLiteral("copy"),
                           QStringLiteral("-map"),
                           QStringLiteral("0:v:0?"),
                           QStringLiteral("-map"),
                           QStringLiteral("1:a:0?"),
                           QStringLiteral("-f"),
                           QStringLiteral("matroska"),
                           tempPath});

    const int id = entry->id;
    const QString finalPath = entry->previewMuxPath;
    connect(process, &QProcess::finished, this,
            [this, id, process, tempPath, finalPath](int exitCode, QProcess::ExitStatus) {
                if (exitCode == 0 && QFile::exists(tempPath) && QFileInfo(tempPath).size() > 0) {
                    QFile::remove(finalPath);
                    QFile::rename(tempPath, finalPath);
                } else {
                    QFile::remove(tempPath);
                }

                if (auto *active = findActiveEntry(id)) {
                    if (active->muxProcess == process) {
                        active->muxProcess = nullptr;
                    }
                    syncActiveRow(*active);
                }
                process->deleteLater();
            });

    process->start();
}

void YtdlpBackend::syncActiveRow(const DownloadEntry &entry)
{
    const QString previewPath = resolvePreviewPath(entry);
    const bool hasPreview = !previewPath.isEmpty();
    m_downloadModel.updateProgress(entry.id, entry.progress, entry.speed, entry.eta, entry.status);
    m_downloadModel.updatePreview(entry.id, hasPreview, previewPath);
    m_downloadModel.updatePaths(entry.id, entry.filePath, entry.outputDir, entry.formatLabel, previewPath);
}

void YtdlpBackend::removeActiveRow(int id)
{
    if (m_downloadModel.indexForId(id) >= 0) {
        m_downloadModel.removeRow(id);
        Q_EMIT activeCountChanged();
    }
    updateActiveList();
}

void YtdlpBackend::updateFailedList()
{
    m_failedDownloads.clear();
    for (const auto &entry : m_failed) {
        m_failedDownloads.append(entryToVariant(entry, true));
    }
    Q_EMIT failedDownloadsChanged();
}

void YtdlpBackend::updateActiveList()
{
    m_activeDownloads.clear();
    for (int i = 0; i < m_downloadModel.rowCount(); ++i) {
        const int id = m_downloadModel.data(m_downloadModel.index(i), YtdlpActiveModel::IdRole).toInt();
        if (const DownloadEntry *entry = m_active.value(id, nullptr)) {
            if (!entry->finished) {
                m_activeDownloads.append(entryToVariant(*entry, false));
            }
        }
    }
    Q_EMIT activeDownloadsChanged();
}

void YtdlpBackend::notify(const QString &summary, const QString &body, const QString &iconName, int timeoutMs)
{
    Q_UNUSED(timeoutMs)
    auto *notification = new KNotification(QStringLiteral("download"), KNotification::CloseOnTimeout);
    notification->setComponentName(QStringLiteral("com.zrn59.ytdlp"));
    notification->setTitle(summary);
    notification->setText(body);
    notification->setIconName(iconName);
    notification->sendEvent();
}

YtdlpFormats::DownloadOptions YtdlpBackend::optionsFromMap(const QVariantMap &map, bool resumePartial) const
{
    DownloadOptions opts;
    opts.url = map.value(QStringLiteral("url")).toString().trimmed();
    opts.outputDir = map.value(QStringLiteral("outputDir")).toString();
    opts.mode = static_cast<DownloadMode>(map.value(QStringLiteral("downloadMode"), 0).toInt());
    opts.videoQuality = static_cast<VideoQuality>(map.value(QStringLiteral("videoQuality"), 7).toInt());
    opts.videoCodec = static_cast<VideoCodec>(map.value(QStringLiteral("videoCodec"), 0).toInt());
    opts.videoContainer = static_cast<VideoContainer>(map.value(QStringLiteral("videoContainer"), 0).toInt());
    opts.audioQuality = static_cast<AudioQuality>(map.value(QStringLiteral("audioQuality"), 0).toInt());
    opts.audioCodec = static_cast<AudioCodec>(map.value(QStringLiteral("audioCodec"), 0).toInt());
    opts.subtitleOption = static_cast<SubtitleOption>(map.value(QStringLiteral("subtitleOption"), 0).toInt());
    opts.subtitleLanguage = static_cast<SubtitleLanguage>(map.value(QStringLiteral("subtitleLanguage"), 0).toInt());
    opts.subtitleFormat = static_cast<SubtitleFormat>(map.value(QStringLiteral("subtitleFormat"), 0).toInt());
    opts.speedLimit = static_cast<SpeedLimit>(map.value(QStringLiteral("speedLimit"), 0).toInt());
    opts.embedMetadata = map.value(QStringLiteral("embedMetadata"), true).toBool();
    opts.embedThumbnail = map.value(QStringLiteral("embedThumbnail"), true).toBool();
    opts.allowPlaylist = map.value(QStringLiteral("allowPlaylist"), false).toBool();
    opts.previewMode = map.value(QStringLiteral("previewMode"), false).toBool();
    opts.useCookies = map.value(QStringLiteral("useCookies"), false).toBool();
    opts.cookiesFile = map.value(QStringLiteral("cookiesFile")).toString().trimmed();
    opts.customArgs = map.value(QStringLiteral("customArgs")).toString();
    opts.downloadSections = map.value(QStringLiteral("downloadSections")).toString();
    opts.resumePartial = resumePartial;
    return opts;
}

void YtdlpBackend::launchDownload(DownloadEntry &entry, const YtdlpFormats::DownloadOptions &opts)
{
    auto *process = new QProcess(this);
    entry.process = process;

    const QStringList args = buildArgs(opts, m_ffmpegPath);
    process->setProgram(m_ytdlpPath);
    process->setArguments(args);
    process->setProcessChannelMode(QProcess::SeparateChannels);
    process->setWorkingDirectory(opts.outputDir);
    configureChildProcess(process);
    process->setProcessEnvironment(runtimeEnvironment());

    QStringList pretty;
    pretty << m_ytdlpPath;
    for (const QString &arg : args) {
        if (arg.contains(QLatin1Char(' ')) || arg.contains(QLatin1Char('"'))) {
            pretty << QStringLiteral("\"%1\"").arg(arg);
        } else {
            pretty << arg;
        }
    }
    appendLog(&entry, pretty.join(QLatin1Char(' ')) + QLatin1Char('\n'));

    const int id = entry.id;

    connect(process, &QProcess::readyReadStandardOutput, this, [this, id, process]() {
        handleStdout(id, process->readAllStandardOutput());
    });
    connect(process, &QProcess::readyReadStandardError, this, [this, id, process]() {
        handleStderr(id, process->readAllStandardError());
    });
    connect(process, &QProcess::errorOccurred, this, [this, id](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            finishDownload(id, false, i18n("Failed to start yt-dlp. Is it installed?"), false);
        }
    });
    connect(process, &QProcess::finished, this,
            [this, id, process](int exitCode, QProcess::ExitStatus status) {
                Q_UNUSED(status)
                auto *active = findActiveEntry(id);
                if (!active || active->finished) {
                    return;
                }
                if (active->cancelled) {
                    return;
                }
                if (exitCode == 0) {
                    const QString title = active->title.isEmpty() ? i18n("Download complete") : active->title;
                    finishDownload(id, true, title, false);
                } else if (active->paused) {
                    // SIGSTOP can look like a signal exit — ignore while paused
                    return;
                } else {
                    appendLog(active, QString::fromUtf8(process->readAllStandardError()));
                    m_downloadModel.updateErrorLog(active->id, active->errorLog);
                    QString err = extractYtdlpError(active->errorLog);
                    if (err.isEmpty()) {
                        err = extractYtdlpError(active->status);
                    }
                    if (err.isEmpty()) {
                        err = i18n("Download failed (exit code %1)", exitCode);
                    }
                    finishDownload(id, false, err, false);
                }
            });

    connect(process, &QProcess::started, this, [this, id]() {
        auto *active = findActiveEntry(id);
        if (!active || !active->process) {
            return;
        }
        const pid_t pid = static_cast<pid_t>(active->process->processId());
        if (pid <= 0) {
            return;
        }
        if (setpgid(pid, pid) == 0 || getpgid(pid) == pid) {
            active->ownProcessGroup = true;
        }
    });

    process->start();
}

bool YtdlpBackend::startDownload(const QVariantMap &options)
{
    if (m_active.size() >= kMaxConcurrent) {
        setError(i18n("Maximum of %1 concurrent downloads", kMaxConcurrent));
        return false;
    }

    if (m_ytdlpPath.isEmpty()) {
        setError(i18n("yt-dlp is not installed yet. Click refresh, or run install.sh."));
        return false;
    }

    const bool resume = options.value(QStringLiteral("resumePartial"), false).toBool();
    DownloadOptions opts = optionsFromMap(options, resume);

    if (!isValidUrl(opts.url)) {
        setError(i18n("Invalid URL (must start with http:// or https://)"));
        return false;
    }

    if (opts.outputDir.isEmpty()) {
        opts.outputDir = opts.mode == DownloadMode::AudioOnly ? defaultAudioFolder() : defaultVideoFolder();
    }

    if (m_ffmpegPath.isEmpty()) {
        setError(i18n("ffmpeg was not found. Install it to merge and convert media."));
        return false;
    }

    if (opts.useCookies) {
        if (opts.cookiesFile.isEmpty()) {
            setError(i18n("Cookies enabled but no cookies file selected"));
            return false;
        }
        if (!QFile::exists(opts.cookiesFile)) {
            setError(i18n("Cookies file not found: %1", opts.cookiesFile));
            return false;
        }
        if (YtdlpFormats::jsRuntimeSpec().isEmpty()) {
            setError(i18n("YouTube cookies need Deno or Node. Run install.sh, or install deno."));
            return false;
        }
    }

    QDir().mkpath(opts.outputDir);

    auto *entry = new DownloadEntry;
    entry->id = m_nextId++;
    entry->fullUrl = opts.url;
    entry->displayUrl = opts.url.length() > 60 ? opts.url.left(57) + QStringLiteral("...") : opts.url;
    entry->title = resume ? i18n("Resuming…") : i18n("Starting…");
    entry->status = resume ? i18n("Resuming") : i18n("Starting");
    entry->previewEnabled = opts.previewMode;
    entry->outputDir = opts.outputDir;
    entry->options = options;
    entry->options.insert(QStringLiteral("url"), opts.url);
    entry->options.insert(QStringLiteral("outputDir"), opts.outputDir);
    entry->options.insert(QStringLiteral("customArgs"), opts.customArgs);
    entry->options.insert(QStringLiteral("downloadSections"), opts.downloadSections);

    const int id = entry->id;
    m_active.insert(id, entry);

    ActiveDownloadRow row;
    row.id = id;
    row.title = entry->title;
    row.status = entry->status;
    row.outputDir = entry->outputDir;
    row.previewEnabled = entry->previewEnabled;
    m_downloadModel.addRow(row);
    Q_EMIT activeCountChanged();

    launchDownload(*entry, opts);
    if (!findActiveEntry(id)) {
        return false;
    }

    if (entry->previewEnabled) {
        ensurePreviewMuxTimer(entry);
    }
    m_downloadModel.updateErrorLog(id, entry->errorLog);
    updateActiveList();

    clearError();

    notify(resume ? i18n("Resuming download") : i18n("Download started"),
           opts.url,
           QStringLiteral("folder-download-symbolic"),
           3000);
    return true;
}

void YtdlpBackend::retryDownload(int id)
{
    auto *failed = findFailedEntry(id);
    if (!failed) {
        return;
    }

    QVariantMap options = failed->options;
    options.insert(QStringLiteral("resumePartial"), true);
    options.insert(QStringLiteral("url"), failed->fullUrl);

    if (!startDownload(options)) {
        return;
    }

    m_failed.erase(std::remove_if(m_failed.begin(), m_failed.end(), [id](const DownloadEntry &e) { return e.id == id; }),
                   m_failed.end());
    updateFailedList();
}

void YtdlpBackend::dismissFailed(int id)
{
    m_failed.erase(std::remove_if(m_failed.begin(), m_failed.end(), [id](const DownloadEntry &e) { return e.id == id; }),
                  m_failed.end());
    updateFailedList();
}

void YtdlpBackend::dismissAllFailed()
{
    m_failed.clear();
    updateFailedList();
}

void YtdlpBackend::cancelDownload(int id)
{
    auto *entry = findActiveEntry(id);
    if (!entry || entry->finished || !entry->process) {
        return;
    }
    entry->cancelled = true;
    signalProcess(entry, SIGKILL);
    if (entry->process) {
        entry->process->kill();
    }
    finishDownload(id, false, i18n("Cancelled"), true);
}

void YtdlpBackend::cancelAll()
{
    QList<int> ids = m_active.keys();
    for (int id : ids) {
        cancelDownload(id);
    }
}

void YtdlpBackend::pauseDownload(int id)
{
    auto *entry = findActiveEntry(id);
    if (!entry || entry->finished || entry->paused || !entry->process) {
        return;
    }
    if (entry->process->state() != QProcess::Running) {
        return;
    }

    if (signalProcess(entry, SIGSTOP)) {
        entry->paused = true;
        entry->status = i18n("Paused");
        stopPreviewMux(entry);
        m_downloadModel.updateState(id, true, false, entry->status);
        updateActiveList();
    }
}

void YtdlpBackend::resumeDownload(int id)
{
    auto *entry = findActiveEntry(id);
    if (!entry || entry->finished || !entry->paused || !entry->process) {
        return;
    }

    if (signalProcess(entry, SIGCONT)) {
        entry->paused = false;
        entry->status = i18n("Downloading");
        if (entry->previewEnabled) {
            ensurePreviewMuxTimer(entry);
        }
        m_downloadModel.updateState(id, false, false, entry->status);
        updateActiveList();
    }
}

void YtdlpBackend::openPreview(int id)
{
    auto *entry = findActiveEntry(id);
    if (!entry) {
        return;
    }

    if (entry->previewEnabled && !isAudioOnlyMode(*entry)) {
        updatePreviewMux(entry);
    }

    const QString path = resolvePreviewPath(*entry);
    if (path.isEmpty()) {
        setError(i18n("No preview file yet — wait until streams start downloading"));
        return;
    }

    if (QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
        return;
    }
    const QString player = YtdlpFormats::findMediaPlayer();
    if (!player.isEmpty() && QProcess::startDetached(player, {path})) {
        return;
    }
    setError(i18n("Failed to open a media player"));
}

void YtdlpBackend::handleStdout(int id, const QByteArray &data)
{
    auto *entry = findActiveEntry(id);
    if (!entry) {
        return;
    }

    entry->stdoutBuf += data;
    entry->stdoutBuf.replace('\r', '\n');

    int idx = 0;
    while ((idx = entry->stdoutBuf.indexOf('\n')) >= 0) {
        const QByteArray rawLine = entry->stdoutBuf.left(idx);
        entry->stdoutBuf.remove(0, idx + 1);
        processStdoutLine(entry, QString::fromUtf8(rawLine));
    }
}

void YtdlpBackend::processStdoutLine(DownloadEntry *entry, const QString &line)
{
    if (!entry || line.isEmpty()) {
        return;
    }

    static const QRegularExpression progressRe(QStringLiteral(R"((\d+(?:\.\d+)?)%)"));
    static const QRegularExpression speedRe(QStringLiteral(R"(\s+at\s+(\S+/s))"));
    static const QRegularExpression etaRe(QStringLiteral(R"(ETA\s+(\S+))"));

    bool pathsChanged = false;
    bool titleChanged = false;

    const bool isProgressLine = line.contains(QStringLiteral("[download]")) && line.contains(QLatin1Char('%'));
    if (!isProgressLine) {
        appendLog(entry, line + QLatin1Char('\n'));
    }

    if (line.contains(QStringLiteral("[download] Destination:"))) {
        const QString path = line.section(QLatin1Char(':'), 1).trimmed();
        if (!path.isEmpty() && !isSidecarPath(path)) {
            entry->filePath = path;
            entry->formatLabel = formatFromPath(path);
            if (entry->outputDir.isEmpty()) {
                entry->outputDir = QFileInfo(path).absolutePath();
            }
            pathsChanged = true;
            const bool audioPart = hasStreamRole(path, QStringLiteral("audio"))
                || (!hasStreamRole(path, QStringLiteral("video")) && isLikelyAudioPart(path));
            if (audioPart) {
                entry->audioPartPath = path;
                if (!entry->paused && entry->videoPartPath.isEmpty()) {
                    entry->status = i18n("Downloading audio…");
                }
            } else if (hasStreamRole(path, QStringLiteral("video")) || entry->videoPartPath.isEmpty()) {
                entry->videoPartPath = path;
                if (!entry->paused && entry->audioPartPath.isEmpty()) {
                    entry->status = i18n("Downloading video…");
                } else if (!entry->paused) {
                    entry->status = i18n("Downloading…");
                }
            }
            entry->title = streamBaseName(path);
            titleChanged = true;

            if (entry->previewEnabled && !entry->videoPartPath.isEmpty() && !entry->audioPartPath.isEmpty()) {
                updatePreviewMux(entry);
            }
        }
    } else if (line.contains(QStringLiteral("[Merger] Merging formats into"))) {
        const QString path = line.section(QLatin1Char('"'), 1, 1);
        entry->filePath = path;
        entry->previewPath = path;
        entry->formatLabel = formatFromPath(path);
        entry->title = QFileInfo(path).completeBaseName();
        pathsChanged = true;
        titleChanged = true;
        if (!entry->paused) {
            entry->status = i18n("Processing");
        }
    } else if (line.contains(QStringLiteral("[ExtractAudio] Destination:"))) {
        const QString path = line.section(QLatin1Char(':'), 1).trimmed();
        entry->filePath = path;
        entry->previewPath = path;
        entry->formatLabel = formatFromPath(path);
        entry->title = QFileInfo(path).completeBaseName();
        pathsChanged = true;
        titleChanged = true;
        if (!entry->paused) {
            entry->status = i18n("Processing");
        }
    } else if (isProgressLine) {
        const auto match = progressRe.match(line);
        if (match.hasMatch()) {
            entry->progress = match.captured(1).toDouble();
        }
        const auto speedMatch = speedRe.match(line);
        if (speedMatch.hasMatch()) {
            entry->speed = speedMatch.captured(1);
        }
        const auto etaMatch = etaRe.match(line);
        if (etaMatch.hasMatch()) {
            entry->eta = etaMatch.captured(1);
        }
        if (!entry->paused) {
            entry->status = i18n("Downloading…");
        }
    }

    if (titleChanged) {
        m_downloadModel.updateTitle(entry->id, entry->title);
    }

    const int pct = static_cast<int>(std::lround(entry->progress));
    const bool progressTick = pct != entry->lastEmittedProgress;
    if (progressTick) {
        entry->lastEmittedProgress = pct;
    }

    if (progressTick || pathsChanged || titleChanged) {
        syncActiveRow(*entry);
    } else if (isProgressLine) {
        m_downloadModel.updateProgress(entry->id, entry->progress, entry->speed, entry->eta, entry->status);
    }
}

void YtdlpBackend::handleStderr(int id, const QByteArray &data)
{
    auto *entry = findActiveEntry(id);
    if (!entry) {
        return;
    }

    entry->stderrBuf += data;
    int idx = 0;
    QString lastError;
    while ((idx = entry->stderrBuf.indexOf('\n')) >= 0) {
        const QByteArray rawLine = entry->stderrBuf.left(idx);
        entry->stderrBuf.remove(0, idx + 1);
        const QString line = QString::fromUtf8(rawLine);
        if (line.isEmpty()) {
            continue;
        }
        appendLog(entry, line + QLatin1Char('\n'));
        const QString error = extractYtdlpError(line);
        if (!error.isEmpty()) {
            lastError = error;
        }
    }

    if (!lastError.isEmpty()) {
        entry->status = lastError;
        m_downloadModel.updateState(id, entry->paused, false, entry->status);
    }
}

void YtdlpBackend::finishDownload(int id, bool success, const QString &message, bool cancelled)
{
    auto *entry = findActiveEntry(id);
    if (!entry) {
        return;
    }

    entry->finished = true;
    entry->failed = !success;
    entry->progress = success ? 100.0 : entry->progress;
    entry->status = success ? message : sanitizeYtdlpMessage(message);

    if (!entry->stdoutBuf.isEmpty()) {
        processStdoutLine(entry, QString::fromUtf8(entry->stdoutBuf));
        entry->stdoutBuf.clear();
    }
    if (!entry->stderrBuf.isEmpty()) {
        appendLog(entry, QString::fromUtf8(entry->stderrBuf) + QLatin1Char('\n'));
        entry->stderrBuf.clear();
    }
    m_downloadModel.updateErrorLog(entry->id, entry->errorLog);

    stopPreviewMux(entry);

    const QString filePath = entry->filePath;
    if (success && !entry->previewMuxPath.isEmpty()) {
        QFile::remove(entry->previewMuxPath);
    }
    QVariantMap savedOptions = entry->options;
    savedOptions.insert(QStringLiteral("url"), entry->fullUrl);

    if (entry->process) {
        entry->process->deleteLater();
        entry->process = nullptr;
    }

    if (success) {
        Q_EMIT downloadFinished(id, true, message, filePath);
        notify(i18n("Download complete"), message, QStringLiteral("emblem-ok-symbolic"), 5000);
        m_active.remove(id);
        destroyEntry(entry);
        removeActiveRow(id);
        updateActiveList();
    } else if (cancelled) {
        m_active.remove(id);
        destroyEntry(entry);
        removeActiveRow(id);
        updateActiveList();
    } else {
        DownloadEntry failedEntry;
        failedEntry.id = entry->id;
        failedEntry.fullUrl = entry->fullUrl;
        failedEntry.displayUrl = entry->displayUrl;
        failedEntry.title = entry->title;
        failedEntry.status = sanitizeYtdlpMessage(message);
        failedEntry.progress = entry->progress;
        failedEntry.filePath = entry->filePath;
        failedEntry.outputDir = entry->outputDir;
        failedEntry.formatLabel = entry->formatLabel;
        failedEntry.errorLog = entry->errorLog;
        failedEntry.failed = true;
        failedEntry.finished = true;
        failedEntry.options = savedOptions;
        m_failed.append(failedEntry);

        setError(message);
        notify(i18n("Download error"), message, QStringLiteral("dialog-error-symbolic"), 10000);
        Q_EMIT downloadFinished(id, false, message, filePath);

        m_active.remove(id);
        destroyEntry(entry);
        removeActiveRow(id);
        updateFailedList();
        updateActiveList();
    }
}
