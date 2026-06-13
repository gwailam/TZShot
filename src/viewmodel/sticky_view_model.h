#ifndef STICKY_VIEW_MODEL_H
#define STICKY_VIEW_MODEL_H

#include <QObject>
#include <QRect>
#include <QImage>
#include "sticky_image_store.h"

// StickyViewModel
// ---------------
// 负责贴图会话的业务逻辑：
//   - 从 StickyImageStore 读写贴图数据（save/copy/release）
//   - 在 Windows 上直接驱动 QWidget 贴图窗口
//   - 在其他平台保留信号式窗口创建接口作为回退路径

class StickyViewModel : public QObject
{
    Q_OBJECT

public:
    explicit StickyViewModel(StickyImageStore &store, QObject *parent = nullptr);

    // 在 Windows 上直接创建 QWidget 贴图；其他平台通过 stickyReady 回退。
    Q_INVOKABLE void requestSticky(const QString &imageUrl, const QRect &imgRect);
    Q_INVOKABLE void requestStickyImage(const QImage &image, const QRect &imgRect);

    // 贴图窗口内部操作
    Q_INVOKABLE void    releaseImage(const QString &imageUrl);
    Q_INVOKABLE bool    saveImage(const QString &imageUrl, const QUrl &targetUrl);
    Q_INVOKABLE bool    copyImageToClipboard(const QString &imageUrl);
    Q_INVOKABLE QImage  getImageByUrl(const QString &imageUrl) const;

signals:
    // 非 Windows 平台可监听此信号自行创建贴图窗口
    void stickyReady(const QString &imageUrl, const QRect &imgRect);

private:
    StickyImageStore &m_store;
};

#endif // STICKY_VIEW_MODEL_H
