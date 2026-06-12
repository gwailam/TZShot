#include "arrow_shape.h"

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

ArrowShape::ArrowShape(const QPoint &startPoint, const QPoint &endPoint,
                       const QColor &color, int size)
    : TwoPointShape(startPoint, endPoint, color, size) {}

void ArrowShape::draw(QPainter *painter)
{
    if (!painter) return;

    // 起点和终点相同时不绘制
    if (m_startPoint == m_endPoint) return;

    painter->save();
    const QPointF startPoint(m_startPoint);
    const QPointF endPoint(m_endPoint);
    QPen pen(m_color, m_size);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(pen);
    painter->setRenderHint(QPainter::Antialiasing);

    painter->drawLine(startPoint, endPoint);

    // 计算箭头方向角
    const double angle = std::atan2(
        endPoint.y() - startPoint.y(),
        endPoint.x() - startPoint.x()
    );

    const qreal headLength = ARROW_HEAD_LENGTH;
    QPointF arrowP1(
        endPoint.x() - std::cos(angle + ARROW_HEAD_ANGLE) * headLength,
        endPoint.y() - std::sin(angle + ARROW_HEAD_ANGLE) * headLength
    );
    QPointF arrowP2(
        endPoint.x() - std::cos(angle - ARROW_HEAD_ANGLE) * headLength,
        endPoint.y() - std::sin(angle - ARROW_HEAD_ANGLE) * headLength
    );

    QPolygonF arrowHead;
    arrowHead << endPoint << arrowP1 << arrowP2;
    painter->setBrush(QBrush(m_color));
    painter->drawPolygon(arrowHead);

    painter->restore();
}

QRect ArrowShape::boundingRect() const
{
    const int margin = qMax(qRound(ARROW_HEAD_LENGTH), m_size / 2 + 2);
    return QRect(m_startPoint, m_endPoint).normalized().adjusted(-margin, -margin, margin, margin);
}

bool ArrowShape::contains(const QPoint &point, int tolerance) const
{
    if (m_startPoint == m_endPoint) {
        return QRect(m_startPoint, m_endPoint).normalized()
            .adjusted(-tolerance, -tolerance, tolerance, tolerance)
            .contains(point);
    }

    const QPointF startPoint(m_startPoint);
    const QPointF endPoint(m_endPoint);
    const qreal hitWidth = qMax(tolerance, m_size / 2 + 4);
    const QPointF hitPoint(point);

    if (pointToSegmentDistance(hitPoint, startPoint, endPoint) <= hitWidth) {
        return true;
    }

    const double angle = std::atan2(
        endPoint.y() - startPoint.y(),
        endPoint.x() - startPoint.x()
    );
    const qreal headLength = ARROW_HEAD_LENGTH;
    const QPointF arrowP1(
        endPoint.x() - std::cos(angle + ARROW_HEAD_ANGLE) * headLength,
        endPoint.y() - std::sin(angle + ARROW_HEAD_ANGLE) * headLength
    );
    const QPointF arrowP2(
        endPoint.x() - std::cos(angle - ARROW_HEAD_ANGLE) * headLength,
        endPoint.y() - std::sin(angle - ARROW_HEAD_ANGLE) * headLength
    );
    return pointToSegmentDistance(hitPoint, endPoint, arrowP1) <= hitWidth
        || pointToSegmentDistance(hitPoint, endPoint, arrowP2) <= hitWidth;
}
