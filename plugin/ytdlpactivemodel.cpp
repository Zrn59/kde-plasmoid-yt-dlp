// SPDX-License-Identifier: MIT

#include "ytdlpactivemodel.h"

#include <cmath>

YtdlpActiveModel::YtdlpActiveModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int YtdlpActiveModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_rows.size();
}

QVariant YtdlpActiveModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }

    const ActiveDownloadRow &row = m_rows.at(index.row());
    switch (role) {
    case IdRole:
        return row.id;
    case TitleRole:
        return row.title;
    case StatusRole:
        return row.status;
    case FilePathRole:
        return row.filePath;
    case OutputDirRole:
        return row.outputDir;
    case FormatLabelRole:
        return row.formatLabel;
    case PreviewFileRole:
        return row.previewFile;
    case ErrorLogRole:
        return row.errorLog;
    case ProgressRole:
        return row.progress;
    case SpeedRole:
        return row.speed;
    case EtaRole:
        return row.eta;
    case FailedRole:
        return row.failed;
    case PausedRole:
        return row.paused;
    case PreviewEnabledRole:
        return row.previewEnabled;
    case HasPreviewRole:
        return row.hasPreview;
    default:
        return {};
    }
}

QHash<int, QByteArray> YtdlpActiveModel::roleNames() const
{
    return {
        {IdRole, "downloadId"},
        {TitleRole, "title"},
        {StatusRole, "status"},
        {FilePathRole, "filePath"},
        {OutputDirRole, "outputDir"},
        {FormatLabelRole, "formatLabel"},
        {PreviewFileRole, "previewFile"},
        {ErrorLogRole, "errorLog"},
        {ProgressRole, "progress"},
        {SpeedRole, "speed"},
        {EtaRole, "eta"},
        {FailedRole, "failed"},
        {PausedRole, "paused"},
        {PreviewEnabledRole, "previewEnabled"},
        {HasPreviewRole, "hasPreview"},
    };
}

void YtdlpActiveModel::addRow(const ActiveDownloadRow &row)
{
    beginInsertRows(QModelIndex(), m_rows.size(), m_rows.size());
    m_rows.append(row);
    endInsertRows();
}

void YtdlpActiveModel::removeRow(int id)
{
    const int idx = indexForId(id);
    if (idx < 0) {
        return;
    }
    beginRemoveRows(QModelIndex(), idx, idx);
    m_rows.removeAt(idx);
    endRemoveRows();
}

bool YtdlpActiveModel::updateProgress(int id, qreal progress, const QString &speed, const QString &eta, const QString &status)
{
    const int idx = indexForId(id);
    if (idx < 0) {
        return false;
    }

    ActiveDownloadRow &row = m_rows[idx];
    const int newPct = static_cast<int>(std::lround(progress));
    const int oldPct = static_cast<int>(std::lround(row.progress));

    QList<int> roles;
    if (newPct != oldPct) {
        row.progress = newPct;
        roles << ProgressRole;
    }
    if (speed != row.speed) {
        row.speed = speed;
        roles << SpeedRole;
    }
    if (eta != row.eta) {
        row.eta = eta;
        roles << EtaRole;
    }
    if (!status.isEmpty() && status != row.status) {
        row.status = status;
        roles << StatusRole;
    }

    if (roles.isEmpty()) {
        return false;
    }

    emit dataChanged(index(idx), index(idx), roles);
    return true;
}

void YtdlpActiveModel::updateState(int id, bool paused, bool failed, const QString &status)
{
    const int idx = indexForId(id);
    if (idx < 0) {
        return;
    }

    ActiveDownloadRow &row = m_rows[idx];
    QList<int> roles;
    if (row.paused != paused) {
        row.paused = paused;
        roles << PausedRole;
    }
    if (row.failed != failed) {
        row.failed = failed;
        roles << FailedRole;
    }
    if (!status.isEmpty() && status != row.status) {
        row.status = status;
        roles << StatusRole;
    }

    if (!roles.isEmpty()) {
        emit dataChanged(index(idx), index(idx), roles);
    }
}

void YtdlpActiveModel::updatePreview(int id, bool hasPreview, const QString &previewFile)
{
    const int idx = indexForId(id);
    if (idx < 0) {
        return;
    }

    ActiveDownloadRow &row = m_rows[idx];
    QList<int> roles;
    if (row.hasPreview != hasPreview) {
        row.hasPreview = hasPreview;
        roles << HasPreviewRole;
    }
    if (!previewFile.isEmpty() && previewFile != row.previewFile) {
        row.previewFile = previewFile;
        roles << PreviewFileRole;
    }

    if (!roles.isEmpty()) {
        emit dataChanged(index(idx), index(idx), roles);
    }
}

void YtdlpActiveModel::updatePaths(int id, const QString &filePath, const QString &outputDir, const QString &formatLabel, const QString &previewFile)
{
    const int idx = indexForId(id);
    if (idx < 0) {
        return;
    }

    ActiveDownloadRow &row = m_rows[idx];
    QList<int> roles;
    if (!filePath.isEmpty() && filePath != row.filePath) {
        row.filePath = filePath;
        roles << FilePathRole;
    }
    if (!outputDir.isEmpty() && outputDir != row.outputDir) {
        row.outputDir = outputDir;
        roles << OutputDirRole;
    }
    if (!formatLabel.isEmpty() && formatLabel != row.formatLabel) {
        row.formatLabel = formatLabel;
        roles << FormatLabelRole;
    }
    if (!previewFile.isEmpty() && previewFile != row.previewFile) {
        row.previewFile = previewFile;
        roles << PreviewFileRole;
    }

    if (!roles.isEmpty()) {
        emit dataChanged(index(idx), index(idx), roles);
    }
}

void YtdlpActiveModel::updateTitle(int id, const QString &title)
{
    const int idx = indexForId(id);
    if (idx < 0) {
        return;
    }

    if (m_rows[idx].title == title) {
        return;
    }
    m_rows[idx].title = title;
    emit dataChanged(index(idx), index(idx), {TitleRole});
}

void YtdlpActiveModel::updateErrorLog(int id, const QString &errorLog)
{
    const int idx = indexForId(id);
    if (idx < 0) {
        return;
    }

    if (m_rows[idx].errorLog == errorLog) {
        return;
    }
    m_rows[idx].errorLog = errorLog;
    emit dataChanged(index(idx), index(idx), {ErrorLogRole});
}

int YtdlpActiveModel::indexForId(int id) const
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).id == id) {
            return i;
        }
    }
    return -1;
}
