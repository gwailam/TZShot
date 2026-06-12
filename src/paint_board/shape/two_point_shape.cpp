#include "two_point_shape.h"

TwoPointShape::TwoPointShape(const QPoint &startPoint, const QPoint &endPoint,
                             const QColor &color, int size)
    : Shape(color, size), m_startPoint(startPoint), m_endPoint(endPoint) {}

void TwoPointShape::setStartPoint(const QPoint &point)
{
    m_startPoint = point;
}

QPoint TwoPointShape::startPoint() const
{
    return m_startPoint;
}

void TwoPointShape::setEndPoint(const QPoint &point)
{
    m_endPoint = point;
}

QPoint TwoPointShape::endPoint() const
{
    return m_endPoint;
}

QRect TwoPointShape::boundingRect() const
{
    const int margin = qMax(2, m_size / 2 + 1);
    return QRect(m_startPoint, m_endPoint).normalized().adjusted(-margin, -margin, margin, margin);
}

void TwoPointShape::translate(const QPoint &offset)
{
    m_startPoint += offset;
    m_endPoint += offset;
}
