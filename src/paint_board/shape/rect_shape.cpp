#include "rect_shape.h"

RectShape::RectShape(const QPoint &startPoint, const QPoint &endPoint,
                     const QColor &color, int size)
    : TwoPointShape(startPoint, endPoint, color, size) {}

void RectShape::draw(QPainter *painter)
{
    if (!painter) return;
    painter->save();
    QPen pen(m_color, m_size);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->drawRect(QRect(m_startPoint, m_endPoint).normalized());
    painter->restore();
}

bool RectShape::contains(const QPoint &point, int tolerance) const
{
    const QRect rect = QRect(m_startPoint, m_endPoint).normalized();
    if (rect.isNull()) {
        return QRect(m_startPoint, m_endPoint).normalized()
            .adjusted(-tolerance, -tolerance, tolerance, tolerance)
            .contains(point);
    }

    const int hitWidth = qMax(tolerance, m_size / 2 + 4);
    const QRect outer = rect.adjusted(-hitWidth, -hitWidth, hitWidth, hitWidth);
    if (!outer.contains(point)) {
        return false;
    }

    const QRect inner = rect.adjusted(hitWidth, hitWidth, -hitWidth, -hitWidth);
    return inner.isEmpty() || !inner.contains(point);
}
