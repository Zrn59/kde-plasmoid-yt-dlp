// SPDX-License-Identifier: MIT

#include "ytdlpbackend.h"

#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QMimeData>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
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

void trimStreamBuf(QByteArray &buf)
{
    constexpr int kMaxStreamBuf = 256 * 1024;
    if (buf.size() <= kMaxStreamBuf) {
        return;
    }
    const int cut = buf.size() - kMaxStreamBuf;
    const int newline = buf.indexOf('\n', cut);
    buf.remove(0, newline >= 0 ? newline + 1 : cut);
}

void removeFreshThumbSidecars(const QString &videoPath, qint64 startedAtMs)
{
    const QFileInfo info(videoPath);
    if (!info.exists() || info.isSymLink()) {
        return;
    }
    const QString suffix = info.suffix().toLower();
    if (suffix == QLatin1String("webp") || suffix == QLatin1String("png")
        || suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg")) {
        return;
    }
    const QDir dir = info.dir();
    const QString base = info.completeBaseName();
    const QDateTime started = QDateTime::fromMSecsSinceEpoch(startedAtMs);
    const QStringList extras = {QStringLiteral("webp"), QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg")};
    for (const QString &extra : extras) {
        const QFileInfo sidecar(dir.filePath(base + QLatin1Char('.') + extra));
        if (!sidecar.exists() || sidecar.isSymLink()) {
            continue;
        }
        if (startedAtMs > 0 && sidecar.lastModified() < started.addSecs(-2)) {
            continue;
        }
        QFile::remove(sidecar.absoluteFilePath());
    }
}

bool moveFileTo(const QString &from, const QString &to)
{
    if (from == to) {
        return true;
    }
    QDir().mkpath(QFileInfo(to).absolutePath());
    if (QFile::exists(to)) {
        QFile::remove(to);
    }
    if (QFile::rename(from, to)) {
        return true;
    }
    if (!QFile::copy(from, to)) {
        return false;
    }
    QFile::remove(from);
    return true;
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
    loadState();
    refreshYtdlpVersion();
    m_checkpointTimer = new QTimer(this);
    m_checkpointTimer->setInterval(20000);
    connect(m_checkpointTimer, &QTimer::timeout, this, [this]() {
        if (!m_active.isEmpty()) {
            saveState();
        }
    });
    m_checkpointTimer->start();
    QTimer::singleShot(0, this, &YtdlpBackend::ensureRuntime);
}

YtdlpBackend::~YtdlpBackend()
{
    m_closing = true;
    saveState();

    for (auto *entry : std::as_const(m_active)) {
        signalProcess(entry, SIGKILL);
    }

    const auto processes = findChildren<QProcess *>(Qt::FindDirectChildrenOnly);
    for (QProcess *process : processes) {
        process->blockSignals(true);
        if (process->state() != QProcess::NotRunning) {
            process->kill();
            process->waitForFinished(200);
        }
    }
    m_versionProcess = nullptr;
    m_inspectProcess = nullptr;
    if (m_plasmoidReply) {
        m_plasmoidReply->abort();
        m_plasmoidReply = nullptr;
    }

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
        entry->process->disconnect();
        if (entry->process->state() != QProcess::NotRunning) {
            signalProcess(entry, SIGKILL);
            entry->process->kill();
        }
        if (!m_closing) {
            entry->process->deleteLater();
        }
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
    // Only signal the group once the child has made itself the leader.
    // Before setpgid(0, 0), the child still shares plasmashell's group.
    if (getpgid(pid) == pid && ::kill(-pid, sig) == 0) {
        return true;
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

bool YtdlpBackend::checkingPlasmoidUpdate() const
{
    return m_checkingPlasmoidUpdate;
}

bool YtdlpBackend::plasmoidUpdateAvailable() const
{
    return m_plasmoidUpdateAvailable;
}

QString YtdlpBackend::plasmoidUpdateNote() const
{
    return m_plasmoidUpdateNote;
}

QString YtdlpBackend::plasmoidLatestVersion() const
{
    return m_plasmoidLatestVersion;
}

QString YtdlpBackend::plasmoidReleaseUrl() const
{
    return m_plasmoidReleaseUrl;
}

QVariantList YtdlpBackend::history() const
{
    return m_history;
}

QString YtdlpBackend::linkInfo() const
{
    return m_linkInfo;
}

bool YtdlpBackend::linkBusy() const
{
    return m_linkBusy;
}

bool YtdlpBackend::linkInfoError() const
{
    return m_linkInfoError;
}

void YtdlpBackend::copyText(const QString &text) const
{
    if (text.isEmpty()) {
        return;
    }
    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(text);
    }
}

QString YtdlpBackend::stateFilePath() const
{
    return runtimeRoot() + QStringLiteral("/state.json");
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

QString YtdlpBackend::httpUrlFromMime(QObject *mimeObject) const
{
    const auto *mime = qobject_cast<const QMimeData *>(mimeObject);
    if (!mime) {
        return {};
    }

    QStringList parts;
    if (mime->hasUrls()) {
        for (const QUrl &url : mime->urls()) {
            parts << url.toString();
        }
    }
    if (mime->hasText()) {
        parts << mime->text();
    }
    if (mime->hasHtml()) {
        parts << mime->html();
    }
    if (mime->hasFormat(QStringLiteral("text/uri-list"))) {
        parts << QString::fromUtf8(mime->data(QStringLiteral("text/uri-list")));
    }
    if (mime->hasFormat(QStringLiteral("text/x-moz-url"))) {
        const QByteArray raw = mime->data(QStringLiteral("text/x-moz-url"));
        parts << QString::fromUtf16(reinterpret_cast<const char16_t *>(raw.constData()), raw.size() / 2);
    }

    QString blob = parts.join(QLatin1Char('\n'));
    blob.remove(QChar(0));
    const QString url = extractHttpUrl(blob);
    return isValidUrl(url) ? url : QString();
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

void YtdlpBackend::checkPlasmoidUpdate(const QString &currentVersion)
{
    m_plasmoidVersion = currentVersion.trimmed();
    ++m_plasmoidUpdateGeneration;
    if (m_plasmoidReply) {
        QNetworkReply *old = m_plasmoidReply;
        m_plasmoidReply = nullptr;
        old->abort();
    }
    if (!m_network) {
        m_network = new QNetworkAccessManager(this);
    }
    m_checkingPlasmoidUpdate = true;
    m_plasmoidUpdateAvailable = false;
    m_plasmoidUpdateNote.clear();
    m_plasmoidLatestVersion.clear();
    m_plasmoidReleaseUrl.clear();
    Q_EMIT plasmoidUpdateChanged();
    requestPlasmoidFeed(QUrl(QStringLiteral("https://api.github.com/repos/Zrn59/kde-plasmoid-yt-dlp/releases/latest")), false);
}

void YtdlpBackend::requestPlasmoidFeed(const QUrl &url, bool tags)
{
    const int generation = m_plasmoidUpdateGeneration;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("kde-plasmoid-yt-dlp"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setTransferTimeout(15000);
    QNetworkReply *reply = m_network->get(request);
    m_plasmoidReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, tags]() {
        if (m_plasmoidReply == reply) {
            m_plasmoidReply = nullptr;
        }
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        const auto error = reply->error();
        reply->deleteLater();
        if (m_closing || generation != m_plasmoidUpdateGeneration) {
            return;
        }
        if (!tags && status == 404) {
            requestPlasmoidFeed(QUrl(QStringLiteral("https://api.github.com/repos/Zrn59/kde-plasmoid-yt-dlp/tags?per_page=100")), true);
            return;
        }
        if (error != QNetworkReply::NoError || status >= 400) {
            applyPlasmoidUpdateResult(QString(), QString());
            return;
        }
        finishPlasmoidFeed(body, status, tags);
    });
}

void YtdlpBackend::finishPlasmoidFeed(const QByteArray &body, int status, bool tags)
{
    Q_UNUSED(status)
    const QJsonDocument document = QJsonDocument::fromJson(body);
    QString latest;
    QString url;
    if (!tags && document.isObject()) {
        const QJsonObject object = document.object();
        latest = object.value(QStringLiteral("tag_name")).toString().trimmed();
        url = object.value(QStringLiteral("html_url")).toString().trimmed();
    } else if (tags && document.isArray()) {
        const QJsonArray array = document.array();
        for (const QJsonValue &value : array) {
            const QString name = value.toObject().value(QStringLiteral("name")).toString().trimmed();
            if (name.isEmpty()) {
                continue;
            }
            if (latest.isEmpty() || compareVersions(name, latest) > 0) {
                latest = name;
            }
        }
        if (!latest.isEmpty()) {
            url = QStringLiteral("https://github.com/Zrn59/kde-plasmoid-yt-dlp/releases/tag/") + latest;
        }
    }
    applyPlasmoidUpdateResult(latest, url);
}

void YtdlpBackend::applyPlasmoidUpdateResult(const QString &latest, const QString &url)
{
    m_checkingPlasmoidUpdate = false;
    QString shown = latest.trimmed();
    if (shown.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        shown = shown.mid(1);
    }
    m_plasmoidLatestVersion = shown;
    m_plasmoidReleaseUrl = url;
    if (shown.isEmpty()) {
        m_plasmoidUpdateAvailable = false;
        m_plasmoidUpdateNote = i18n("could not check");
    } else if (compareVersions(shown, m_plasmoidVersion) > 0) {
        m_plasmoidUpdateAvailable = true;
        m_plasmoidUpdateNote = i18n("%1 is available", shown);
    } else {
        m_plasmoidUpdateAvailable = false;
        m_plasmoidUpdateNote = i18n("up to date");
    }
    Q_EMIT plasmoidUpdateChanged();
}

void YtdlpBackend::ensureRuntime()
{
    const QString script = YtdlpFormats::updateScriptPath();
    if (script.isEmpty()) {
        return;
    }

    const QString unitDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/systemd/user");
    QDir().mkpath(unitDir);
    const QString servicePath = unitDir + QStringLiteral("/com.zrn59.ytdlp-update.service");
    const QString timerPath = unitDir + QStringLiteral("/com.zrn59.ytdlp-update.timer");
    const QString service = QStringLiteral(
                                "[Unit]\n"
                                "Description=Update the yt-dlp binary used by the com.zrn59.ytdlp Plasma widget\n"
                                "\n"
                                "[Service]\n"
                                "Type=oneshot\n"
                                "ExecStart=%1\n"
                                "Nice=10\n")
                                .arg(script);
    const QByteArray timer = QByteArrayLiteral(
        "[Unit]\n"
        "Description=Daily update of the yt-dlp binary used by com.zrn59.ytdlp\n"
        "\n"
        "[Timer]\n"
        "OnBootSec=5min\n"
        "OnUnitActiveSec=1d\n"
        "RandomizedDelaySec=30min\n"
        "Persistent=true\n"
        "\n"
        "[Install]\n"
        "WantedBy=timers.target\n");

    bool changed = false;
    QFile currentService(servicePath);
    QString existingService;
    if (currentService.open(QIODevice::ReadOnly)) {
        existingService = QString::fromUtf8(currentService.readAll());
    }
    if (existingService != service) {
        QFile out(servicePath);
        if (out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            out.write(service.toUtf8());
            changed = true;
        }
    }
    if (!QFileInfo::exists(timerPath)) {
        QFile out(timerPath);
        if (out.open(QIODevice::WriteOnly)) {
            out.write(timer);
            changed = true;
        }
    }
    if (changed) {
        auto *reload = new QProcess(this);
        reload->setProgram(QStringLiteral("systemctl"));
        reload->setArguments({QStringLiteral("--user"), QStringLiteral("daemon-reload")});
        connect(reload, &QProcess::finished, this, [reload](int, QProcess::ExitStatus) {
            QProcess::startDetached(QStringLiteral("systemctl"),
                                    {QStringLiteral("--user"),
                                     QStringLiteral("enable"),
                                     QStringLiteral("--now"),
                                     QStringLiteral("com.zrn59.ytdlp-update.timer")});
            reload->deleteLater();
        });
        reload->start();
    }

    if (m_ytdlpPath.isEmpty()) {
        updateYtdlp();
    }
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

QString YtdlpBackend::previewCacheRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
        + QStringLiteral("/com.zrn59.ytdlp/preview");
}

void YtdlpBackend::removePreviewWork(const DownloadEntry &entry) const
{
    const QString dir = QDir::cleanPath(entry.previewWorkDir);
    if (!isManagedScratch(dir) || QFileInfo(dir).isSymLink()) {
        return;
    }
    QDir(dir).removeRecursively();
}

QString YtdlpBackend::locatePreviewSource(const QString &destination, const QString &workDir) const
{
    const QString base = QFileInfo(destination).fileName();
    const QStringList candidates = {
        workDir.isEmpty() ? QString() : QDir(workDir).filePath(base),
        workDir.isEmpty() ? QString() : QDir(workDir).filePath(base + QStringLiteral(".part")),
        destination,
        destination + QStringLiteral(".part"),
    };
    QString best;
    qint64 bestSize = 0;
    for (const QString &candidate : candidates) {
        if (candidate.isEmpty() || !QFile::exists(candidate)) {
            continue;
        }
        const qint64 size = QFileInfo(candidate).size();
        if (size > bestSize) {
            best = candidate;
            bestSize = size;
        }
    }
    return best;
}

QString YtdlpBackend::publishPreviewDownload(DownloadEntry *entry) const
{
    if (!entry || entry->previewWorkDir.isEmpty()) {
        return entry ? entry->filePath : QString();
    }

    QString source = entry->filePath;
    if (!source.isEmpty() && !QFile::exists(source)) {
        const QString inHome = QDir(entry->outputDir).filePath(QFileInfo(source).fileName());
        if (QFile::exists(inHome)) {
            return inHome;
        }
    }
    const QDir work(entry->previewWorkDir);
    const QRegularExpression fragmentRe(QStringLiteral(R"(\.f\d+\.)"));
    const auto skipName = [&fragmentRe](const QString &name) {
        return name == QStringLiteral("watch.mkv")
            || name == QStringLiteral("preview-mux.mkv")
            || name.endsWith(QStringLiteral(".tmp"))
            || name.endsWith(QStringLiteral(".part"))
            || name.endsWith(QStringLiteral(".ytdl"))
            || fragmentRe.match(name).hasMatch();
    };

    if (source.isEmpty() || !QFile::exists(source) || skipName(QFileInfo(source).fileName())) {
        source.clear();
        qint64 bestSize = 0;
        const QFileInfoList files = work.entryInfoList(QDir::Files);
        for (const QFileInfo &info : files) {
            if (skipName(info.fileName()) || info.size() <= bestSize) {
                continue;
            }
            source = info.absoluteFilePath();
            bestSize = info.size();
        }
    }

    if (source.isEmpty() || !pathIsInside(source, entry->previewWorkDir)) {
        return entry->filePath;
    }

    const QString destination = !entry->previewMuxPath.isEmpty()
        ? entry->previewMuxPath
        : QDir(entry->outputDir).filePath(QFileInfo(source).fileName());
    if (!moveFileTo(source, destination)) {
        return source;
    }
    return destination;
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
    entry->muxTimer->setInterval(20000);
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
        entry->muxTimer->disconnect();
        if (!m_closing) {
            entry->muxTimer->deleteLater();
        }
        entry->muxTimer = nullptr;
    }
    if (entry->muxProcess) {
        entry->muxProcess->disconnect();
        if (entry->muxProcess->state() != QProcess::NotRunning) {
            entry->muxProcess->kill();
        }
        if (!m_closing) {
            entry->muxProcess->deleteLater();
        }
        entry->muxProcess = nullptr;
    }
    if (!entry->previewMuxPath.isEmpty()) {
        QFile::remove(entry->previewMuxPath + QStringLiteral(".part"));
    }
}

void YtdlpBackend::updatePreviewMux(DownloadEntry *entry)
{
    if (!entry || !entry->previewEnabled || entry->paused || entry->merging || isAudioOnlyMode(*entry)) {
        return;
    }
    if (entry->videoPartPath.isEmpty() || entry->audioPartPath.isEmpty()) {
        return;
    }
    const QString videoSource = locatePreviewSource(entry->videoPartPath, entry->previewWorkDir);
    const QString audioSource = locatePreviewSource(entry->audioPartPath, entry->previewWorkDir);
    if (videoSource.isEmpty() || audioSource.isEmpty()) {
        return;
    }
    const qint64 videoBytes = QFileInfo(videoSource).size();
    const qint64 audioBytes = QFileInfo(audioSource).size();
    if (videoBytes < 64 * 1024 || audioBytes < 32 * 1024) {
        return;
    }
    if (entry->muxVideoBytes >= 0 && !entry->previewMuxPath.isEmpty() && QFileInfo(entry->previewMuxPath).size() > 0) {
        const bool videoGrew = videoBytes - entry->muxVideoBytes >= 16 * 1024 * 1024;
        const bool audioGrew = audioBytes - entry->muxAudioBytes >= 1024 * 1024;
        if (!videoGrew && !audioGrew) {
            return;
        }
    }
    if (entry->muxProcess && entry->muxProcess->state() != QProcess::NotRunning) {
        return;
    }
    if (m_ffmpegPath.isEmpty()) {
        return;
    }

    if (entry->previewMuxPath.isEmpty()) {
        QString base = streamBaseName(entry->videoPartPath);
        base.replace(QLatin1Char('/'), QLatin1Char('_'));
        base.replace(QLatin1Char('\\'), QLatin1Char('_'));
        if (base.isEmpty()) {
            base = QStringLiteral("preview");
        }
        QDir().mkpath(entry->outputDir);
        // Same name the finished download will have, so the player keeps one file.
        entry->previewMuxPath = QDir(entry->outputDir).filePath(base + QStringLiteral(".mkv"));
    }

    const QString tempPath = entry->previewMuxPath + QStringLiteral(".part");

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
                           videoSource,
                           QStringLiteral("-i"),
                           audioSource,
                           QStringLiteral("-c"),
                           QStringLiteral("copy"),
                           QStringLiteral("-map"),
                           QStringLiteral("0:v:0"),
                           QStringLiteral("-map"),
                           QStringLiteral("1:a:0"),
                           QStringLiteral("-f"),
                           QStringLiteral("matroska"),
                           tempPath});

    const int id = entry->id;
    const QString finalPath = entry->previewMuxPath;
    connect(process, &QProcess::finished, this,
            [this, id, process, tempPath, finalPath, videoBytes, audioBytes](int exitCode, QProcess::ExitStatus status) {
                auto *active = findActiveEntry(id);
                if (active && active->merging) {
                    QFile::remove(tempPath);
                    if (active->muxProcess == process) {
                        active->muxProcess = nullptr;
                    }
                    process->deleteLater();
                    return;
                }
                const bool muxed = status == QProcess::NormalExit && exitCode == 0 && QFile::exists(tempPath)
                    && QFileInfo(tempPath).size() > 0 && moveFileTo(tempPath, finalPath);
                if (!muxed) {
                    const QString err = QString::fromUtf8(process->readAllStandardError()).trimmed();
                    if (!err.isEmpty()) {
                        qWarning("kde-plasmoid-yt-dlp preview: %s", qUtf8Printable(err));
                    }
                    QFile::remove(tempPath);
                }
                if (active) {
                    if (active->muxProcess == process) {
                        active->muxProcess = nullptr;
                    }
                    if (muxed) {
                        active->muxVideoBytes = videoBytes;
                        active->muxAudioBytes = audioBytes;
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
    scheduleSave();
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
    auto *notification = new KNotification(QStringLiteral("download"), KNotification::CloseOnTimeout);
    notification->setComponentName(QStringLiteral("com.zrn59.ytdlp"));
    notification->setTitle(summary);
    notification->setText(body);
    notification->setIconName(iconName);
    if (timeoutMs >= 8000) {
        notification->setUrgency(KNotification::HighUrgency);
    } else if (timeoutMs <= 3000) {
        notification->setUrgency(KNotification::LowUrgency);
    }
    notification->sendEvent();
}

void YtdlpBackend::scheduleSave()
{
    if (!m_saveTimer) {
        m_saveTimer = new QTimer(this);
        m_saveTimer->setSingleShot(true);
        m_saveTimer->setInterval(400);
        connect(m_saveTimer, &QTimer::timeout, this, [this]() { saveState(); });
    }
    m_saveTimer->start();
}

QJsonObject YtdlpBackend::entryToJson(const DownloadEntry &entry, bool running) const
{
    QString log = entry.errorLog;
    if (log.size() > 8000) {
        log = log.right(8000);
    }
    QJsonObject object;
    object.insert(QStringLiteral("id"), entry.id);
    object.insert(QStringLiteral("url"), entry.fullUrl);
    object.insert(QStringLiteral("title"), entry.title);
    object.insert(QStringLiteral("status"), entry.status);
    object.insert(QStringLiteral("progress"), entry.progress);
    object.insert(QStringLiteral("filePath"), entry.filePath);
    object.insert(QStringLiteral("outputDir"), entry.outputDir);
    object.insert(QStringLiteral("formatLabel"), entry.formatLabel);
    object.insert(QStringLiteral("errorLog"), log);
    object.insert(QStringLiteral("running"), running);
    object.insert(QStringLiteral("previewWorkDir"), entry.previewWorkDir);
    object.insert(QStringLiteral("options"), QJsonObject::fromVariantMap(entry.options));
    return object;
}

bool YtdlpBackend::isManagedScratch(const QString &path) const
{
    const QString root = QDir::cleanPath(previewCacheRoot());
    const QString absolute = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    return pathIsInside(absolute, root) && absolute != root;
}

void YtdlpBackend::sweepOrphanScratch() const
{
    const QString rootPath = previewCacheRoot();
    QDir root(rootPath);
    if (!root.exists()) {
        return;
    }

    QSet<QString> keep;
    for (const DownloadEntry &entry : m_failed) {
        if (isManagedScratch(entry.previewWorkDir)) {
            keep.insert(QDir::cleanPath(entry.previewWorkDir));
        }
        const QString byId = QDir::cleanPath(root.filePath(QString::number(entry.id)));
        if (QFileInfo(byId).isDir()) {
            keep.insert(byId);
        }
    }

    const QFileInfoList dirs = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &info : dirs) {
        bool numeric = !info.fileName().isEmpty();
        for (const QChar c : info.fileName()) {
            if (!c.isDigit()) {
                numeric = false;
                break;
            }
        }
        if (!numeric) {
            continue;
        }
        const QString absolute = QDir::cleanPath(info.absoluteFilePath());
        if (keep.contains(absolute) || !isManagedScratch(absolute)) {
            continue;
        }
        if (info.isSymLink()) {
            QFile::remove(info.absoluteFilePath());
        } else {
            QDir(absolute).removeRecursively();
        }
    }
}

YtdlpBackend::DownloadEntry YtdlpBackend::entryFromJson(const QJsonObject &object) const
{
    DownloadEntry entry;
    entry.id = object.value(QStringLiteral("id")).toInt();
    entry.fullUrl = object.value(QStringLiteral("url")).toString();
    entry.displayUrl = entry.fullUrl.length() > 60 ? entry.fullUrl.left(57) + QStringLiteral("...") : entry.fullUrl;
    entry.title = object.value(QStringLiteral("title")).toString();
    entry.status = object.value(QStringLiteral("status")).toString();
    entry.progress = object.value(QStringLiteral("progress")).toDouble();
    entry.filePath = object.value(QStringLiteral("filePath")).toString();
    entry.outputDir = object.value(QStringLiteral("outputDir")).toString();
    entry.formatLabel = object.value(QStringLiteral("formatLabel")).toString();
    entry.errorLog = object.value(QStringLiteral("errorLog")).toString();
    entry.failed = true;
    entry.finished = true;
    entry.options = object.value(QStringLiteral("options")).toObject().toVariantMap();
    if (!entry.options.contains(QStringLiteral("url"))) {
        entry.options.insert(QStringLiteral("url"), entry.fullUrl);
    }
    entry.previewWorkDir = object.value(QStringLiteral("previewWorkDir")).toString();
    if (entry.previewWorkDir.isEmpty()) {
        entry.previewWorkDir = entry.options.value(QStringLiteral("scratchDir")).toString();
    }
    if (!isManagedScratch(entry.previewWorkDir)) {
        const QString guess = previewCacheRoot() + QLatin1Char('/') + QString::number(entry.id);
        entry.previewWorkDir = QFileInfo(guess).isDir() ? QDir::cleanPath(QFileInfo(guess).absoluteFilePath()) : QString();
    } else {
        entry.previewWorkDir = QDir::cleanPath(QFileInfo(entry.previewWorkDir).absoluteFilePath());
    }
    return entry;
}

void YtdlpBackend::saveState() const
{
    QJsonArray stopped;
    for (const DownloadEntry &entry : m_failed) {
        if (!entry.fullUrl.isEmpty()) {
            stopped.append(entryToJson(entry, false));
        }
    }
    for (const DownloadEntry *entry : std::as_const(m_active)) {
        if (entry && !entry->finished && !entry->fullUrl.isEmpty()) {
            stopped.append(entryToJson(*entry, true));
        }
    }
    while (stopped.size() > 40) {
        stopped.removeAt(0);
    }

    QJsonArray history;
    const int historyCount = qMin(30, m_history.size());
    for (int i = 0; i < historyCount; ++i) {
        history.append(QJsonObject::fromVariantMap(m_history.at(i).toMap()));
    }

    QJsonObject root;
    root.insert(QStringLiteral("nextId"), m_nextId);
    root.insert(QStringLiteral("stopped"), stopped);
    root.insert(QStringLiteral("history"), history);

    QDir().mkpath(runtimeRoot());
    QSaveFile file(stateFilePath());
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.commit();
}

void YtdlpBackend::loadState()
{
    QFile file(stateFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return;
    }
    const QJsonObject root = doc.object();
    m_nextId = qMax(1, root.value(QStringLiteral("nextId")).toInt(1));
    int maxId = 0;

    const QJsonArray stopped = root.value(QStringLiteral("stopped")).toArray();
    for (const QJsonValue &value : stopped) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        DownloadEntry entry = entryFromJson(object);
        if (entry.fullUrl.isEmpty()) {
            continue;
        }
        if (object.value(QStringLiteral("running")).toBool()) {
            entry.status = i18n("Interrupted. Retry continues the partial file.");
        }
        maxId = qMax(maxId, entry.id);
        m_failed.append(entry);
    }
    while (m_failed.size() > 40) {
        m_failed.removeFirst();
    }
    sweepOrphanScratch();

    const QJsonArray history = root.value(QStringLiteral("history")).toArray();
    for (const QJsonValue &value : history) {
        if (!value.isObject()) {
            continue;
        }
        const QVariantMap item = value.toObject().toVariantMap();
        if (item.value(QStringLiteral("url")).toString().isEmpty() && item.value(QStringLiteral("filePath")).toString().isEmpty()) {
            continue;
        }
        m_history.append(item);
        maxId = qMax(maxId, item.value(QStringLiteral("id")).toInt());
    }
    while (m_history.size() > 30) {
        m_history.removeLast();
    }

    m_nextId = qMax(m_nextId, maxId + 1);
    updateFailedList();
    Q_EMIT historyChanged();
}

void YtdlpBackend::rememberDownload(const DownloadEntry &entry)
{
    const QString path = entry.filePath;
    if (!path.isEmpty()) {
        for (int i = m_history.size() - 1; i >= 0; --i) {
            if (m_history.at(i).toMap().value(QStringLiteral("filePath")).toString() == path) {
                m_history.removeAt(i);
            }
        }
    }
    QVariantMap item;
    item.insert(QStringLiteral("id"), entry.id);
    item.insert(QStringLiteral("title"), entry.title.isEmpty() ? entry.fullUrl : entry.title);
    item.insert(QStringLiteral("filePath"), entry.filePath);
    item.insert(QStringLiteral("outputDir"), entry.outputDir);
    item.insert(QStringLiteral("url"), entry.fullUrl);
    m_history.prepend(item);
    while (m_history.size() > 30) {
        m_history.removeLast();
    }
    Q_EMIT historyChanged();
    scheduleSave();
}

void YtdlpBackend::dismissHistory(int id)
{
    for (int i = 0; i < m_history.size(); ++i) {
        if (m_history.at(i).toMap().value(QStringLiteral("id")).toInt() == id) {
            m_history.removeAt(i);
            Q_EMIT historyChanged();
            scheduleSave();
            return;
        }
    }
}

void YtdlpBackend::clearHistory()
{
    if (m_history.isEmpty()) {
        return;
    }
    m_history.clear();
    Q_EMIT historyChanged();
    scheduleSave();
}

void YtdlpBackend::clearLinkInfo()
{
    ++m_inspectGeneration;
    if (m_inspectProcess) {
        m_inspectProcess->kill();
        m_inspectProcess->deleteLater();
        m_inspectProcess = nullptr;
    }
    const bool wasBusy = m_linkBusy;
    const bool hadInfo = !m_linkInfo.isEmpty() || m_linkInfoError;
    m_linkBusy = false;
    m_linkInfo.clear();
    m_linkInfoError = false;
    if (wasBusy) {
        Q_EMIT linkBusyChanged();
    }
    if (hadInfo) {
        Q_EMIT linkInfoChanged();
    }
}

void YtdlpBackend::inspectUrl(const QVariantMap &options)
{
    const QString url = extractHttpUrl(options.value(QStringLiteral("url")).toString());
    if (!isValidUrl(url)) {
        m_linkInfo = i18n("Enter a URL");
        m_linkInfoError = true;
        m_linkBusy = false;
        Q_EMIT linkBusyChanged();
        Q_EMIT linkInfoChanged();
        return;
    }
    if (m_ytdlpPath.isEmpty()) {
        m_linkInfo = i18n("yt-dlp is not installed yet. Click refresh, or run install.sh.");
        m_linkInfoError = true;
        m_linkBusy = false;
        Q_EMIT linkBusyChanged();
        Q_EMIT linkInfoChanged();
        return;
    }

    const int generation = ++m_inspectGeneration;
    if (m_inspectProcess) {
        m_inspectProcess->kill();
        m_inspectProcess->deleteLater();
        m_inspectProcess = nullptr;
    }

    m_linkBusy = true;
    m_linkInfoError = false;
    m_linkInfo.clear();
    Q_EMIT linkBusyChanged();
    Q_EMIT linkInfoChanged();

    auto *process = new QProcess(this);
    m_inspectProcess = process;
    QStringList args{
        QStringLiteral("--ignore-config"),
        QStringLiteral("--no-warnings"),
        QStringLiteral("--flat-playlist"),
        QStringLiteral("--skip-download"),
        QStringLiteral("--print"),
        QStringLiteral("%(title)s\t%(duration_string)s\t%(playlist_count)s"),
    };
    if (options.value(QStringLiteral("useCookies")).toBool()) {
        const QString browser = cookiesBrowserName(options.value(QStringLiteral("cookiesBrowser")).toInt());
        if (!browser.isEmpty()) {
            args << QStringLiteral("--cookies-from-browser") << browser;
        } else {
            const QString cookies = options.value(QStringLiteral("cookiesFile")).toString();
            if (QFile::exists(cookies)) {
                args << QStringLiteral("--cookies") << cookies;
            }
        }
    }
    const QString jsRuntime = jsRuntimeSpec();
    if (!jsRuntime.isEmpty()) {
        args << QStringLiteral("--js-runtimes") << jsRuntime;
    }
    args << url;
    process->setProgram(m_ytdlpPath);
    process->setArguments(args);
    process->setProcessEnvironment(runtimeEnvironment());
    connect(process, &QProcess::finished, this, [this, process, generation](int code, QProcess::ExitStatus) {
        if (generation != m_inspectGeneration) {
            if (m_inspectProcess == process) {
                m_inspectProcess = nullptr;
            }
            process->deleteLater();
            return;
        }
        const QString out = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
        const QString err = extractYtdlpError(QString::fromUtf8(process->readAllStandardError()));
        m_inspectProcess = nullptr;
        process->deleteLater();
        m_linkBusy = false;
        if (code != 0 || out.isEmpty()) {
            m_linkInfoError = true;
            m_linkInfo = err.isEmpty() ? i18n("Could not read this link") : err;
        } else {
            const QString line = out.section(QLatin1Char('\n'), 0, 0);
            const QString title = line.section(QLatin1Char('\t'), 0, 0).trimmed();
            const QString duration = line.section(QLatin1Char('\t'), 1, 1).trimmed();
            const QString count = line.section(QLatin1Char('\t'), 2, 2).trimmed();
            QStringList parts;
            if (!title.isEmpty() && title.compare(QStringLiteral("NA"), Qt::CaseInsensitive) != 0) {
                parts << title;
            }
            bool countOk = false;
            const int playlistCount = count.toInt(&countOk);
            if (countOk && playlistCount > 1) {
                parts << i18n("playlist, %1 items", playlistCount);
            } else if (!duration.isEmpty() && duration.compare(QStringLiteral("NA"), Qt::CaseInsensitive) != 0) {
                parts << duration;
            }
            m_linkInfoError = false;
            m_linkInfo = parts.join(QStringLiteral(" · "));
            if (m_linkInfo.isEmpty()) {
                m_linkInfo = i18n("Link is readable");
            }
        }
        Q_EMIT linkBusyChanged();
        Q_EMIT linkInfoChanged();
    });
    process->start();
}

YtdlpFormats::DownloadOptions YtdlpBackend::optionsFromMap(const QVariantMap &map, bool resumePartial) const
{
    DownloadOptions opts;
    opts.url = extractHttpUrl(map.value(QStringLiteral("url")).toString());
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
    opts.cookiesBrowser = map.value(QStringLiteral("cookiesBrowser"), 0).toInt();
    opts.customArgs = map.value(QStringLiteral("customArgs")).toString();
    opts.allowUnsafeArgs = map.value(QStringLiteral("allowUnsafeArgs"), false).toBool();
    opts.downloadSections = map.value(QStringLiteral("downloadSections")).toString();
    opts.outputTemplate = map.value(QStringLiteral("outputTemplate")).toString();
    opts.sponsorBlockRemove = map.value(QStringLiteral("sponsorBlock")).toBool();
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
        if (cookiesBrowserName(opts.cookiesBrowser).isEmpty()) {
            if (opts.cookiesFile.isEmpty()) {
                setError(i18n("Cookies enabled but no cookies file selected"));
                return false;
            }
            if (!QFile::exists(opts.cookiesFile)) {
                setError(i18n("Cookies file not found: %1", opts.cookiesFile));
                return false;
            }
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
    entry->previewEnabled = opts.previewMode && opts.mode != DownloadMode::AudioOnly;
    entry->audioFirst = entry->previewEnabled && opts.mode == DownloadMode::VideoWithAudio;
    entry->outputDir = opts.outputDir;
    entry->startedAtMs = QDateTime::currentMSecsSinceEpoch();
    const QString requestedScratch = options.value(QStringLiteral("scratchDir")).toString();
    if (isManagedScratch(requestedScratch)) {
        entry->previewWorkDir = QDir::cleanPath(QFileInfo(requestedScratch).absoluteFilePath());
    } else {
        entry->previewWorkDir = previewCacheRoot() + QLatin1Char('/') + QString::number(entry->id);
    }
    QDir().mkpath(entry->previewWorkDir);
    opts.tempDir = entry->previewWorkDir;
    if (entry->previewEnabled) {
        opts.previewWorkDir = entry->previewWorkDir;
        opts.previewMode = true;
    } else {
        opts.previewMode = false;
    }
    entry->options = options;
    entry->options.insert(QStringLiteral("url"), opts.url);
    entry->options.insert(QStringLiteral("outputDir"), entry->outputDir);
    entry->options.insert(QStringLiteral("customArgs"), opts.customArgs);
    entry->options.insert(QStringLiteral("downloadSections"), opts.downloadSections);
    entry->options.insert(QStringLiteral("scratchDir"), entry->previewWorkDir);

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
    scheduleSave();

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
    if (isManagedScratch(failed->previewWorkDir)) {
        options.insert(QStringLiteral("scratchDir"), failed->previewWorkDir);
    }

    if (!startDownload(options)) {
        return;
    }

    m_failed.erase(std::remove_if(m_failed.begin(), m_failed.end(), [id](const DownloadEntry &e) { return e.id == id; }),
                   m_failed.end());
    updateFailedList();
}

void YtdlpBackend::dismissFailed(int id)
{
    for (const DownloadEntry &entry : m_failed) {
        if (entry.id == id) {
            removePreviewWork(entry);
            break;
        }
    }
    m_failed.erase(std::remove_if(m_failed.begin(), m_failed.end(), [id](const DownloadEntry &e) { return e.id == id; }),
                  m_failed.end());
    updateFailedList();
}

void YtdlpBackend::dismissAllFailed()
{
    for (const DownloadEntry &entry : m_failed) {
        removePreviewWork(entry);
    }
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
    trimStreamBuf(entry->stdoutBuf);

    int idx = 0;
    while ((idx = entry->stdoutBuf.indexOf('\n')) >= 0) {
        const QByteArray rawLine = entry->stdoutBuf.left(idx);
        entry->stdoutBuf.remove(0, idx + 1);
        processStdoutLine(entry, QString::fromUtf8(rawLine));
    }
}

void YtdlpBackend::processStdoutLine(DownloadEntry *entry, const QString &rawLine)
{
    if (!entry) {
        return;
    }
    const QString line = stripAnsi(rawLine).trimmed();
    if (line.isEmpty()) {
        return;
    }

    const ParsedProgress machineProgress = parseProgressLine(line);
    if (machineProgress.valid) {
        entry->progress = machineProgress.percent;
        if (!machineProgress.speed.isEmpty()) {
            entry->speed = machineProgress.speed;
        }
        if (!machineProgress.eta.isEmpty()) {
            entry->eta = machineProgress.eta;
        }
        if (!entry->paused && !entry->merging) {
            entry->status = i18n("Downloading…");
        }
        const int pct = static_cast<int>(std::lround(entry->progress));
        if (pct != entry->lastEmittedProgress) {
            entry->lastEmittedProgress = pct;
            syncActiveRow(*entry);
        } else {
            m_downloadModel.updateProgress(entry->id, entry->progress, entry->speed, entry->eta, entry->status);
        }
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

    static const QRegularExpression formatPairRe(QStringLiteral(R"(format\(s\): (\d+)\+(\d+))"));
    const QRegularExpressionMatch formatPair = formatPairRe.match(line);
    if (formatPair.hasMatch()) {
        entry->firstFormatId = formatPair.captured(1);
        entry->secondFormatId = formatPair.captured(2);
    }

    const QString alreadyPath = finishedDownloadPath(line);
    if (!alreadyPath.isEmpty() && !isSidecarPath(alreadyPath)) {
        entry->filePath = alreadyPath;
        entry->previewPath = alreadyPath;
        if (entry->outputDir.isEmpty()) {
            entry->outputDir = QFileInfo(alreadyPath).absolutePath();
        }
        entry->formatLabel = formatFromPath(alreadyPath);
        entry->title = QFileInfo(alreadyPath).completeBaseName();
        entry->progress = 100;
        pathsChanged = true;
        titleChanged = true;
    } else if (line.contains(QStringLiteral("[download] Destination:"))) {
        const QString path = line.section(QLatin1Char(':'), 1).trimmed();
        if (!path.isEmpty() && !isSidecarPath(path)) {
            const bool scratch = pathIsInside(path, entry->previewWorkDir);
            if (!scratch) {
                entry->filePath = path;
                if (entry->outputDir.isEmpty()) {
                    entry->outputDir = QFileInfo(path).absolutePath();
                }
            }
            entry->formatLabel = formatFromPath(path);
            pathsChanged = true;
            static const QRegularExpression formatIdRe(QStringLiteral(R"(\.f(\d+)\.)"));
            const QRegularExpressionMatch formatId = formatIdRe.match(QFileInfo(path).fileName());
            const QString id = formatId.hasMatch() ? formatId.captured(1) : QString();
            const bool knownAudio = entry->audioFirst
                ? (!entry->firstFormatId.isEmpty() && id == entry->firstFormatId)
                : (!entry->secondFormatId.isEmpty() && id == entry->secondFormatId);
            const bool knownVideo = entry->audioFirst
                ? (!entry->secondFormatId.isEmpty() && id == entry->secondFormatId)
                : (!entry->firstFormatId.isEmpty() && id == entry->firstFormatId);
            const bool audioPart = knownAudio
                || (!knownVideo && (hasStreamRole(path, QStringLiteral("audio")) || isLikelyAudioPart(path)));
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
        entry->merging = true;
        stopPreviewMux(entry);
        const QString path = line.section(QLatin1Char('"'), 1, 1);
        entry->filePath = path;
        entry->previewPath = path;
        if (entry->previewEnabled && !entry->outputDir.isEmpty() && !path.isEmpty()) {
            const QString finalInFolder = QDir(entry->outputDir).filePath(QFileInfo(path).fileName());
            if (entry->previewMuxPath.isEmpty()) {
                entry->previewMuxPath = finalInFolder;
            } else if (entry->previewMuxPath != finalInFolder && QFile::exists(entry->previewMuxPath)) {
                QFile::rename(entry->previewMuxPath, finalInFolder);
                entry->previewMuxPath = finalInFolder;
            }
        }
        entry->formatLabel = formatFromPath(path);
        entry->title = QFileInfo(path).completeBaseName();
        pathsChanged = true;
        titleChanged = true;
        if (!entry->paused) {
            entry->status = i18n("Processing");
        }
    } else if (line.contains(QStringLiteral("[VideoRemuxer]"))) {
        entry->merging = true;
        stopPreviewMux(entry);
        if (!entry->paused) {
            entry->status = i18n("Processing");
        }
    } else if (line.contains(QStringLiteral("[ExtractAudio] Destination:"))) {
        entry->merging = true;
        stopPreviewMux(entry);
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
    trimStreamBuf(entry->stderrBuf);
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
    if (success) {
        const QString watchFile = entry->previewMuxPath;
        entry->filePath = publishPreviewDownload(entry);
        if (!watchFile.isEmpty() && watchFile != entry->filePath) {
            QFile::remove(watchFile);
        }
        removeFreshThumbSidecars(entry->filePath, entry->startedAtMs);
        removePreviewWork(*entry);
    } else if (cancelled) {
        removePreviewWork(*entry);
    }

    const QString filePath = entry->filePath;
    QVariantMap savedOptions = entry->options;
    savedOptions.insert(QStringLiteral("url"), entry->fullUrl);

    if (entry->process) {
        entry->process->deleteLater();
        entry->process = nullptr;
    }

    if (success) {
        rememberDownload(*entry);
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
        scheduleSave();
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
        failedEntry.previewWorkDir = entry->previewWorkDir;
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
