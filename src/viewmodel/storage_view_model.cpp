#include "storage_view_model.h"

StorageViewModel::StorageViewModel(AppSettings &settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
}

QUrl StorageViewModel::savePath() const
{
    return QUrl::fromLocalFile(m_settings.savePath());
}

void StorageViewModel::setSavePath(const QUrl &path)
{
    QString localPath = path.toLocalFile();
    if (localPath.isEmpty()) localPath = path.toString();
    if (localPath.startsWith("file://"))
        localPath = QUrl(localPath).toLocalFile();

    if (localPath.isEmpty() || m_settings.savePath() == localPath) return;

    m_settings.setSavePath(localPath);
    emit savePathChanged(savePath());
}
