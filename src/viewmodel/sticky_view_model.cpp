#include "sticky_view_model.h"

#include "widgets/sticky_pin_widget.h"

StickyViewModel::StickyViewModel(StickyImageStore &store, QObject *parent)
    : QObject(parent)
    , m_store(store)
{
}

void StickyViewModel::requestSticky(const QString &imageUrl, const QRect &imgRect)
{
    if (imageUrl.isEmpty()) {
        return;
    }

    QRect targetRect = imgRect.normalized();
#ifdef Q_OS_WIN
    const QImage image = m_store.getImageByUrl(imageUrl);
    if (!image.isNull()) {
        auto *widget = new StickyPinWidget(imageUrl,
                                           targetRect,
                                           image,
                                           &m_store);
        Q_UNUSED(widget);
        return;
    }
#endif
    emit stickyReady(imageUrl, targetRect);
}

void StickyViewModel::requestStickyImage(const QImage &image, const QRect &imgRect)
{
    if (image.isNull()) {
        return;
    }

    const QString imageUrl = m_store.storeImage(image);
    if (imageUrl.isEmpty()) {
        return;
    }
    requestSticky(imageUrl, imgRect);
}

void StickyViewModel::releaseImage(const QString &imageUrl)
{
    m_store.releaseImage(imageUrl);
}

bool StickyViewModel::saveImage(const QString &imageUrl, const QUrl &targetUrl)
{
    return m_store.saveImage(imageUrl, targetUrl);
}

bool StickyViewModel::copyImageToClipboard(const QString &imageUrl)
{
    return m_store.copyImageToClipboard(imageUrl);
}

QImage StickyViewModel::getImageByUrl(const QString &imageUrl) const
{
    return m_store.getImageByUrl(imageUrl);
}

