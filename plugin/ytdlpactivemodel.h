// SPDX-License-Identifier: MIT

#pragma once

#include <QAbstractListModel>
#include <QString>

struct ActiveDownloadRow {
    int id = 0;
    QString title;
    QString status;
    QString filePath;
    QString outputDir;
    QString formatLabel;
    QString previewFile;
    QString errorLog;
    qreal progress = 0.0;
    QString speed;
    QString eta;
    bool failed = false;
    bool paused = false;
    bool previewEnabled = false;
    bool hasPreview = false;
};

class YtdlpActiveModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        StatusRole,
        FilePathRole,
        OutputDirRole,
        FormatLabelRole,
        PreviewFileRole,
        ErrorLogRole,
        ProgressRole,
        SpeedRole,
        EtaRole,
        FailedRole,
        PausedRole,
        PreviewEnabledRole,
        HasPreviewRole,
    };
    Q_ENUM(Role)

    explicit YtdlpActiveModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void addRow(const ActiveDownloadRow &row);
    void removeRow(int id);
    /// Returns false if nothing visible to QML changed (integer progress throttle).
    bool updateProgress(int id, qreal progress, const QString &speed, const QString &eta, const QString &status);
    void updateState(int id, bool paused, bool failed, const QString &status);
    void updatePreview(int id, bool hasPreview, const QString &previewFile);
    void updatePaths(int id, const QString &filePath, const QString &outputDir, const QString &formatLabel, const QString &previewFile);
    void updateTitle(int id, const QString &title);
    void updateErrorLog(int id, const QString &errorLog);
    int indexForId(int id) const;

private:
    QList<ActiveDownloadRow> m_rows;
};
