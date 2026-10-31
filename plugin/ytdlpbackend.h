// SPDX-License-Identifier: MIT

#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QProcess>

class QNetworkAccessManager;
class QNetworkReply;
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include "ytdlpformats.h"
#include "ytdlpactivemodel.h"

class YtdlpBackend : public QObject
{
    Q_OBJECT

    Q_PROPERTY(YtdlpActiveModel *downloadModel READ downloadModel CONSTANT)
    Q_PROPERTY(QVariantList failedDownloads READ failedDownloads NOTIFY failedDownloadsChanged)
    Q_PROPERTY(QVariantList activeDownloads READ activeDownloads NOTIFY activeDownloadsChanged)
    Q_PROPERTY(int activeCount READ activeCount NOTIFY activeCountChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString ytdlpVersion READ ytdlpVersion NOTIFY ytdlpVersionChanged)
    Q_PROPERTY(QString ytdlpPath READ ytdlpPath NOTIFY ytdlpVersionChanged)
    Q_PROPERTY(bool updatingYtdlp READ updatingYtdlp NOTIFY updatingYtdlpChanged)
    Q_PROPERTY(bool checkingPlasmoidUpdate READ checkingPlasmoidUpdate NOTIFY plasmoidUpdateChanged)
    Q_PROPERTY(bool plasmoidUpdateAvailable READ plasmoidUpdateAvailable NOTIFY plasmoidUpdateChanged)
    Q_PROPERTY(QString plasmoidUpdateNote READ plasmoidUpdateNote NOTIFY plasmoidUpdateChanged)
    Q_PROPERTY(QString plasmoidLatestVersion READ plasmoidLatestVersion NOTIFY plasmoidUpdateChanged)
    Q_PROPERTY(QString plasmoidReleaseUrl READ plasmoidReleaseUrl NOTIFY plasmoidUpdateChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(QString linkInfo READ linkInfo NOTIFY linkInfoChanged)
    Q_PROPERTY(bool linkBusy READ linkBusy NOTIFY linkBusyChanged)
    Q_PROPERTY(bool linkInfoError READ linkInfoError NOTIFY linkInfoChanged)

public:
    explicit YtdlpBackend(QObject *parent = nullptr);
    ~YtdlpBackend() override;

    QVariantList failedDownloads() const;
    QVariantList activeDownloads() const;
    YtdlpActiveModel *downloadModel();
    int activeCount() const;
    QString lastError() const;
    QString ytdlpVersion() const;
    QString ytdlpPath() const;
    bool updatingYtdlp() const;
    bool checkingPlasmoidUpdate() const;
    bool plasmoidUpdateAvailable() const;
    QString plasmoidUpdateNote() const;
    QString plasmoidLatestVersion() const;
    QString plasmoidReleaseUrl() const;
    QVariantList history() const;
    QString linkInfo() const;
    bool linkBusy() const;
    bool linkInfoError() const;

    Q_INVOKABLE bool startDownload(const QVariantMap &options);
    Q_INVOKABLE void retryDownload(int id);
    Q_INVOKABLE void dismissFailed(int id);
    Q_INVOKABLE void dismissAllFailed();
    Q_INVOKABLE void cancelDownload(int id);
    Q_INVOKABLE void cancelAll();
    Q_INVOKABLE void pauseDownload(int id);
    Q_INVOKABLE void resumeDownload(int id);
    Q_INVOKABLE void openPreview(int id);
    Q_INVOKABLE QString pasteFromClipboard() const;
    Q_INVOKABLE QString selectFolder(const QString &current) const;
    Q_INVOKABLE QString selectCookiesFile(const QString &current) const;
    Q_INVOKABLE QString defaultCookiesFile() const;
    Q_INVOKABLE void openFolder(const QString &path) const;
    Q_INVOKABLE void openFile(const QString &path) const;
    Q_INVOKABLE void clearError();
    Q_INVOKABLE void setError(const QString &message);
    Q_INVOKABLE QString defaultVideoFolder() const;
    Q_INVOKABLE QString defaultAudioFolder() const;
    Q_INVOKABLE bool isValidUrl(const QString &url) const;
    Q_INVOKABLE QString httpUrlFromMime(QObject *mimeData) const;
    Q_INVOKABLE void refreshYtdlpVersion();
    Q_INVOKABLE void updateYtdlp();
    Q_INVOKABLE void checkPlasmoidUpdate(const QString &currentVersion);
    Q_INVOKABLE void inspectUrl(const QVariantMap &options);
    Q_INVOKABLE void clearLinkInfo();
    Q_INVOKABLE void copyText(const QString &text) const;
    Q_INVOKABLE void dismissHistory(int id);
    Q_INVOKABLE void clearHistory();

signals:
    void activeCountChanged();
    void activeDownloadsChanged();
    void failedDownloadsChanged();
    void lastErrorChanged();
    void ytdlpVersionChanged();
    void updatingYtdlpChanged();
    void plasmoidUpdateChanged();
    void downloadFinished(int id, bool success, const QString &title, const QString &filePath);
    void historyChanged();
    void linkInfoChanged();
    void linkBusyChanged();

private:
    struct DownloadEntry {
        int id = 0;
        QString fullUrl;
        QString displayUrl;
        QString title;
        QString status;
        qreal progress = 0.0;
        int lastEmittedProgress = -1;
        QString speed;
        QString eta;
        QString filePath;
        QString outputDir;
        QString formatLabel;
        QString previewPath;
        QString videoPartPath;
        QString audioPartPath;
        QString previewMuxPath;
        QString previewWorkDir;
        QString errorLog;
        QByteArray stdoutBuf;
        QByteArray stderrBuf;
        bool paused = false;
        bool previewEnabled = false;
        bool audioFirst = false;
        QString firstFormatId;
        QString secondFormatId;
        bool failed = false;
        bool finished = false;
        bool cancelled = false;
        bool merging = false;
        qint64 lastLogEmitMs = 0;
        qint64 startedAtMs = 0;
        qint64 muxVideoBytes = -1;
        qint64 muxAudioBytes = -1;
        QVariantMap options;
        QProcess *process = nullptr;
        QProcess *muxProcess = nullptr;
        QTimer *muxTimer = nullptr;
    };

    static constexpr int kMaxConcurrent = 5;

    YtdlpFormats::DownloadOptions optionsFromMap(const QVariantMap &map, bool resumePartial) const;
    void launchDownload(DownloadEntry &entry, const YtdlpFormats::DownloadOptions &opts);
    void syncActiveRow(const DownloadEntry &entry);
    void removeActiveRow(int id);
    void handleStdout(int id, const QByteArray &data);
    void handleStderr(int id, const QByteArray &data);
    void processStdoutLine(DownloadEntry *entry, const QString &line);
    void appendLog(DownloadEntry *entry, const QString &chunk);
    void finishDownload(int id, bool success, const QString &message, bool cancelled);
    void notify(const QString &summary, const QString &body, const QString &iconName, int timeoutMs);
    void configureChildProcess(QProcess *process);
    bool signalProcess(DownloadEntry *entry, int sig) const;
    DownloadEntry *findActiveEntry(int id);
    DownloadEntry *findFailedEntry(int id);
    QString resolvePreviewPath(const DownloadEntry &entry) const;
    void ensurePreviewMuxTimer(DownloadEntry *entry);
    void stopPreviewMux(DownloadEntry *entry);
    void updatePreviewMux(DownloadEntry *entry);
    static QString streamBaseName(const QString &path);
    static QString previewCacheRoot();
    void removePreviewWork(const DownloadEntry &entry) const;
    bool isManagedScratch(const QString &path) const;
    void sweepOrphanScratch() const;
    QString locatePreviewSource(const QString &destination, const QString &workDir) const;
    QString publishPreviewDownload(DownloadEntry *entry) const;
    static bool isLikelyAudioPart(const QString &path);
    static bool isAudioOnlyMode(const DownloadEntry &entry);
    static QString formatFromPath(const QString &path);
    QVariantMap entryToVariant(const DownloadEntry &entry, bool includeOptions) const;
    void updateFailedList();
    void updateActiveList();
    void destroyEntry(DownloadEntry *entry);
    void ensureRuntime();
    void scheduleSave();
    void saveState() const;
    void loadState();
    QString stateFilePath() const;
    void requestPlasmoidFeed(const QUrl &url, bool tags);
    void finishPlasmoidFeed(const QByteArray &body, int status, bool tags);
    void applyPlasmoidUpdateResult(const QString &latest, const QString &url);
    void rememberDownload(const DownloadEntry &entry);
    QJsonObject entryToJson(const DownloadEntry &entry, bool running) const;
    DownloadEntry entryFromJson(const QJsonObject &object) const;

    static constexpr int kMaxLogChars = 48 * 1024;
    static constexpr int kLogEmitIntervalMs = 250;
    QHash<int, DownloadEntry *> m_active;
    QList<DownloadEntry> m_failed;
    YtdlpActiveModel m_downloadModel;
    QVariantList m_failedDownloads;
    QVariantList m_activeDownloads;
    QVariantList m_history;
    QString m_lastError;
    QString m_ytdlpVersion;
    QString m_linkInfo;
    bool m_linkBusy = false;
    bool m_linkInfoError = false;
    int m_inspectGeneration = 0;
    bool m_updatingYtdlp = false;
    bool m_checkingPlasmoidUpdate = false;
    bool m_plasmoidUpdateAvailable = false;
    int m_plasmoidUpdateGeneration = 0;
    QString m_plasmoidVersion;
    QString m_plasmoidUpdateNote;
    QString m_plasmoidLatestVersion;
    QString m_plasmoidReleaseUrl;
    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_plasmoidReply = nullptr;
    bool m_closing = false;
    int m_nextId = 1;
    QString m_ytdlpPath;
    QString m_ffmpegPath;
    QProcess *m_versionProcess = nullptr;
    QProcess *m_inspectProcess = nullptr;
    QTimer *m_saveTimer = nullptr;
    QTimer *m_checkpointTimer = nullptr;
};
