#include "shape.h"

Shape::Shape(const QColor &color, int size)
    : m_color(color), m_size(size) {}

QRect Shape::boundingRect() const
{
    return {};
}

bool Shape::contains(const QPoint &point, int tolerance) const
{
    Q_UNUSED(point);
    Q_UNUSED(tolerance);
    return false;
}

void Shape::translate(const QPoint &offset)
{
    Q_UNUSED(offset);
}
