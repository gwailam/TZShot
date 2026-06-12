#include "ellipse_shape.h"

#include <cmath>

EllipseShape::EllipseShape(const QPoint &startPoint, const QPoint &endPoint,
                           const QColor &color, int size)
    : TwoPointShape(startPoint, endPoint, color, size) {}

void EllipseShape::draw(QPainter *painter)
{
    if (!painter) return;
    painter->save();
    QPen pen(m_color, m_size);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->drawEllipse(QRect(m_startPoint, m_endPoint).normalized());
    painter->restore();
}

bool EllipseShape::contains(const QPoint &point, int tolerance) const
{
    const QRect rect = QRect(m_startPoint, m_endPoint).normalized();
    const int hitWidth = qMax(tolerance, m_size / 2 + 4);
    const QRect hitRect = rect.adjusted(-hitWidth, -hitWidth, hitWidth, hitWidth);
    if (!hitRect.contains(point)) {
        return false;
    }

    const qreal rx = rect.width() / 2.0;
    const qreal ry = rect.height() / 2.0;
    if (rx <= 0.0 || ry <= 0.0) {
        return rect.adjusted(-hitWidth, -hitWidth, hitWidth, hitWidth).contains(point);
    }

    const QPointF center = rect.center();
    const qreal normalized =
        std::pow((point.x() - center.x()) / rx, 2.0)
        + std::pow((point.y() - center.y()) / ry, 2.0);
    const qreal outerRx = rx + hitWidth;
    const qreal outerRy = ry + hitWidth;
    const qreal innerRx = qMax<qreal>(1.0, rx - hitWidth);
    const qreal innerRy = qMax<qreal>(1.0, ry - hitWidth);
    const qreal outer =
        std::pow((point.x() - center.x()) / outerRx, 2.0)
        + std::pow((point.y() - center.y()) / outerRy, 2.0);
    const qreal inner =
        std::pow((point.x() - center.x()) / innerRx, 2.0)
        + std::pow((point.y() - center.y()) / innerRy, 2.0);

    return normalized <= 1.0 || (outer <= 1.0 && inner >= 1.0);
}
