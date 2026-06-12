#include "pen_shape.h"

#include <QLineF>

namespace {

qreal pointToSegmentDistance(const QPointF &point, const QPointF &a, const QPointF &b)
{
    const QPointF ab = b - a;
    const QPointF ap = point - a;
    const qreal lengthSquared = ab.x() * ab.x() + ab.y() * ab.y();
    if (lengthSquared <= 0.0) {
        return QLineF(point, a).length();
    }

    const qreal t = qBound<qreal>(0.0,
                                  (ap.x() * ab.x() + ap.y() * ab.y()) / lengthSquared,
                                  1.0);
    const QPointF projection(a.x() + ab.x() * t, a.y() + ab.y() * t);
    return QLineF(point, projection).length();
}

}

PenShape::PenShape(const QPoint &startPoint, const QColor &color, int size)
    : Shape(color, size)
{
    m_points.append(startPoint);
}

void PenShape::addPoint(const QPoint &point)
{
    // 避免重复添加相同的点
    if (!m_points.isEmpty() && m_points.last() == point)
        return;
    m_points.append(point);
}

void PenShape::setEndPoint(const QPoint &point)
{
    addPoint(point);
}

void PenShape::draw(QPainter *painter)
{
    if (!painter || m_points.size() < 2)
        return;

    painter->save();
    QPen pen(m_color, m_size);
    pen.setCapStyle(Qt::RoundCap);    // 端点圆滑
    pen.setJoinStyle(Qt::RoundJoin);  // 转折点圆滑
    painter->setPen(pen);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->drawPolyline(QPolygon(m_points));

    painter->restore();
}

QRect PenShape::boundingRect() const
{
    if (m_points.isEmpty()) {
        return {};
    }

    int left = m_points.first().x();
    int right = left;
    int top = m_points.first().y();
    int bottom = top;
    for (const QPoint &point : m_points) {
        left = qMin(left, point.x());
        right = qMax(right, point.x());
        top = qMin(top, point.y());
        bottom = qMax(bottom, point.y());
    }

    const int margin = qMax(2, m_size / 2 + 1);
    return QRect(QPoint(left, top), QPoint(right, bottom)).normalized()
        .adjusted(-margin, -margin, margin, margin);
}

bool PenShape::contains(const QPoint &point, int tolerance) const
{
    if (m_points.isEmpty()) {
        return false;
    }

    const qreal hitWidth = qMax(tolerance, m_size / 2 + 4);
    if (m_points.size() == 1) {
        return QLineF(QPointF(point), QPointF(m_points.first())).length() <= hitWidth;
    }

    const QPointF hitPoint(point);
    for (int i = 1; i < m_points.size(); ++i) {
        if (pointToSegmentDistance(hitPoint, QPointF(m_points.at(i - 1)), QPointF(m_points.at(i))) <= hitWidth) {
            return true;
        }
    }
    return false;
}

void PenShape::translate(const QPoint &offset)
{
    for (QPoint &point : m_points) {
        point += offset;
    }
}
