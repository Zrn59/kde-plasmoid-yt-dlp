// SPDX-License-Identifier: MIT

#pragma once

#include <QHash>
#include <QObject>
#include <QProcess>
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
    Q_INVOKABLE void refreshYtdlpVersion();
    Q_INVOKABLE void updateYtdlp();

signals:
    void activeCountChanged();
    void activeDownloadsChanged();
    void failedDownloadsChanged();
    void lastErrorChanged();
    void ytdlpVersionChanged();
    void updatingYtdlpChanged();
    void downloadFinished(int id, bool success, const QString &title, const QString &filePath);

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
        QString errorLog;
        QByteArray stdoutBuf;
        QByteArray stderrBuf;
        bool paused = false;
        bool previewEnabled = false;
        bool failed = false;
        bool finished = false;
        bool cancelled = false;
        bool ownProcessGroup = false;
        qint64 lastLogEmitMs = 0;
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
    static bool isLikelyAudioPart(const QString &path);
    static bool isAudioOnlyMode(const DownloadEntry &entry);
    static QString formatFromPath(const QString &path);
    QVariantMap entryToVariant(const DownloadEntry &entry, bool includeOptions) const;
    void updateFailedList();
    void updateActiveList();
    void destroyEntry(DownloadEntry *entry);
    void ensureRuntime();

    static constexpr int kMaxLogChars = 48 * 1024;
    static constexpr int kLogEmitIntervalMs = 250;
    QHash<int, DownloadEntry *> m_active;
    QList<DownloadEntry> m_failed;
    YtdlpActiveModel m_downloadModel;
    QVariantList m_failedDownloads;
    QVariantList m_activeDownloads;
    QString m_lastError;
    QString m_ytdlpVersion;
    bool m_updatingYtdlp = false;
    int m_nextId = 1;
    QString m_ytdlpPath;
    QString m_ffmpegPath;
    QProcess *m_versionProcess = nullptr;
};
