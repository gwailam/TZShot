#ifndef STORAGE_VIEW_MODEL_H
#define STORAGE_VIEW_MODEL_H

#include <QObject>
#include <QUrl>
#include "model/app_settings.h"

// StorageViewModel
// ----------------
// 负责截图保存目录（savePath）的读写与暴露，数据由 AppSettings 持久化。

class StorageViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl savePath READ savePath NOTIFY savePathChanged)

public:
    explicit StorageViewModel(AppSettings &settings, QObject *parent = nullptr);

    QUrl savePath() const;
    Q_INVOKABLE void setSavePath(const QUrl &path);

signals:
    void savePathChanged(const QUrl &savePath);

private:
    AppSettings &m_settings;
};

#endif // STORAGE_VIEW_MODEL_H
