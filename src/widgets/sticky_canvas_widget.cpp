#include "widgets/sticky_canvas_widget.h"

#include "paint_board/shape/arrow_shape.h"
#include "paint_board/shape/blur_shape.h"
#include "paint_board/shape/ellipse_shape.h"
#include "paint_board/shape/mosaic_shape.h"
#include "paint_board/shape/rect_shape.h"
#include "paint_board/shape/text_shape.h"
#include "paint_board/shape/two_point_shape.h"

#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

namespace {

QRect normalizedRectFromPoints(const QPoint &a, const QPoint &b)
{
    return QRect(a, b).normalized();
}

bool pointHitsHandle(const QPoint &point, const QPoint &handlePoint, int radius)
{
    const QPoint delta = point - handlePoint;
    return delta.x() * delta.x() + delta.y() * delta.y() <= radius * radius;
}

}

StickyCanvasWidget::StickyCanvasWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

StickyCanvasWidget::~StickyCanvasWidget()
{
    clearShapes();
}

void StickyCanvasWidget::setBackgroundImage(const QImage &image)
{
    m_backgroundImage = image;
    for (Shape *shape : m_shapes) {
        if (auto *mosaic = dynamic_cast<MosaicShape*>(shape)) {
            mosaic->setCanvasSnapshot(m_backgroundImage);
        } else if (auto *blur = dynamic_cast<BlurShape*>(shape)) {
            blur->setCanvasSnapshot(m_backgroundImage);
        }
    }
    update();
}

void StickyCanvasWidget::setBackgroundVisible(bool visible)
{
    if (m_backgroundVisible == visible) {
        return;
    }
    m_backgroundVisible = visible;
    update();
}

void StickyCanvasWidget::setViewScale(qreal scale)
{
    const qreal next = qMax<qreal>(0.01, scale);
    if (qFuzzyCompare(m_viewScale, next)) {
        return;
    }
    m_viewScale = next;
    update();
}

void StickyCanvasWidget::setAnnotationBounds(const QRect &displayBounds)
{
    const QRect next = displayBounds.normalized().intersected(rect());
    if (m_annotationBounds == next) {
        return;
    }
    m_annotationBounds = next;
    update();
}

void StickyCanvasWidget::setContentOpacity(qreal opacity)
{
    const qreal next = qBound<qreal>(0.05, opacity, 1.0);
    if (qFuzzyCompare(m_contentOpacity, next)) {
        return;
    }
    m_contentOpacity = next;
    update();
}

void StickyCanvasWidget::setActiveShapeType(Shapeype type)
{
    if (m_shapeType != type) {
        clearSelectedShape();
        update();
    }
    m_shapeType = type;
}

void StickyCanvasWidget::setPenColor(const QColor &color)
{
    m_penColor = color;
    if (Shape *shape = selectedShape()) {
        shape->setColor(color);
        update();
    }
}

void StickyCanvasWidget::setPenSize(int size)
{
    m_penSize = qMax(1, size);
    if (Shape *shape = selectedShape()) {
        shape->setSize(m_penSize);
        update();
    }
}

void StickyCanvasWidget::setDrawingEnabled(bool enabled)
{
    m_drawingEnabled = enabled;
    setAttribute(Qt::WA_TransparentForMouseEvents, !m_drawingEnabled);
    if (!m_drawingEnabled) {
        finishCurrentShape();
        clearSelectedShape();
        update();
    }
}

void StickyCanvasWidget::setNumberValue(int value)
{
    m_numberValue = qMax(1, value);
}

void StickyCanvasWidget::setNumberAutoIncrement(bool enabled)
{
    m_numberAutoIncrement = enabled;
}

void StickyCanvasWidget::addTextAnnotation(const QPoint &point,
                                           const QString &text,
                                           const QColor &color,
                                           int size,
                                           bool withBackground)
{
    const QString resolved = text.trimmed();
    if (resolved.isEmpty()) {
        return;
    }

    const QPoint imagePoint(qRound(point.x() / m_viewScale),
                            qRound(point.y() / m_viewScale));

    Shape *textShape = m_factory.createTextShape(imagePoint,
                                                 resolved,
                                                 color,
                                                 size,
                                                 withBackground);
    if (!textShape) {
        return;
    }

    m_shapes.append(textShape);
    clearSelectedShape();
    update();
}

QImage StickyCanvasWidget::compositedImage() const
{
    return compositedImage(rect());
}

QImage StickyCanvasWidget::compositedImage(const QRect &displayRect) const
{
    const QRect targetRect = displayRect.normalized().intersected(rect());
    if (targetRect.isEmpty()) {
        return {};
    }

    QImage image(targetRect.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(-targetRect.topLeft());

    if (!m_backgroundImage.isNull()) {
        painter.drawImage(rect(), m_backgroundImage);
    } else {
        painter.fillRect(targetRect, Qt::white);
    }

    painter.save();
    painter.scale(m_viewScale, m_viewScale);
    for (Shape *shape : m_shapes) {
        if (shape) {
            shape->draw(&painter);
        }
    }
    if (m_currentShape) {
        m_currentShape->draw(&painter);
    }
    painter.restore();

    return image;
}

void StickyCanvasWidget::renderAnnotations(QPainter *painter) const
{
    if (!painter) {
        return;
    }
    painter->save();
    painter->scale(m_viewScale, m_viewScale);
    for (Shape *shape : m_shapes) {
        if (shape) {
            shape->draw(painter);
        }
    }
    if (m_currentShape) {
        m_currentShape->draw(painter);
    }
    painter->restore();
}

void StickyCanvasWidget::undo()
{
    if (!m_shapes.isEmpty()) {
        const int removedIndex = m_shapes.size() - 1;
        delete m_shapes.takeLast();
        if (m_selectedShapeIndex == removedIndex) {
            clearSelectedShape();
        } else if (m_selectedShapeIndex > removedIndex) {
            --m_selectedShapeIndex;
        }
        update();
    }
}

bool StickyCanvasWidget::deleteSelectedShape()
{
    if (m_selectedShapeIndex < 0 || m_selectedShapeIndex >= m_shapes.size()) {
        return false;
    }

    delete m_shapes.takeAt(m_selectedShapeIndex);
    clearSelectedShape();
    update();
    return true;
}

void StickyCanvasWidget::reset()
{
    finishCurrentShape();
    clearShapes();
    m_backgroundImage = QImage();
    update();
}

void StickyCanvasWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.save();
    painter.setOpacity(m_contentOpacity);
    if (m_backgroundVisible) {
        painter.fillRect(rect(), Qt::white);
    }

    if (m_backgroundVisible && !m_backgroundImage.isNull()) {
        painter.drawImage(rect(), m_backgroundImage);
    }

    const QRect displayBounds = m_annotationBounds.normalized().intersected(rect());
    if (!displayBounds.isEmpty()) {
        painter.setClipRect(displayBounds);
    }

    painter.scale(m_viewScale, m_viewScale);
    for (Shape *shape : m_shapes) {
        if (shape) {
            shape->draw(&painter);
        }
    }

    drawSelectedShapeOutline(&painter);

    if (m_currentShape) {
        m_currentShape->draw(&painter);
    }
    painter.restore();
}

void StickyCanvasWidget::mousePressEvent(QMouseEvent *event)
{
    if (!m_drawingEnabled || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QPoint displayPoint = event->position().toPoint();
    const QPoint imagePoint(qRound(displayPoint.x() / m_viewScale),
                            qRound(displayPoint.y() / m_viewScale));
    const QRect bounds = annotationBounds();
    if (!bounds.isEmpty() && !bounds.contains(imagePoint)) {
        QWidget::mousePressEvent(event);
        return;
    }

    m_startPoint = imagePoint;
    setFocus(Qt::MouseFocusReason);

    const EditHandle editHandle = editHandleAt(m_startPoint);
    if (editHandle != EditHandle::None) {
        m_activeEditHandle = editHandle;
        m_pressingEditHandle = true;
        m_draggingEditHandle = false;
        m_selectedShapePressDisplayPoint = displayPoint;
        if (TwoPointShape *shape = selectedTwoPointShape()) {
            m_editStartRect = normalizedRectFromPoints(shape->startPoint(), shape->endPoint());
        } else {
            m_editStartRect = {};
        }
        update();
        return;
    }

    const int shapeIndex = findTopmostShapeIndex(m_startPoint);
    if (shapeIndex >= 0) {
        m_selectedShapeIndex = shapeIndex;
        m_pressingSelectedShape = true;
        m_draggingSelectedShape = false;
        m_activeEditHandle = EditHandle::None;
        m_pressingEditHandle = false;
        m_draggingEditHandle = false;
        m_selectedShapePressDisplayPoint = displayPoint;
        m_selectedShapeLastPoint = m_startPoint;
        update();
        return;
    }

    if (m_shapeType == TEXT) {
        clearSelectedShape();
        update();
        emit textPlacementRequested(displayPoint);
        return;
    }

    clearSelectedShape();

    if (m_shapeType == NUMBER) {
        Shape *numberShape = m_factory.createNumberShape(m_startPoint,
                                                         m_numberValue,
                                                         m_penColor,
                                                         m_penSize);
        if (numberShape) {
            m_shapes.append(numberShape);
            if (m_numberAutoIncrement) {
                ++m_numberValue;
                emit numberValueChanged(m_numberValue);
            }
            update();
        }
        return;
    }

    delete m_currentShape;
    m_currentShape = nullptr;

    if (m_shapeType == MOSAIC) {
        auto *mosaic = new MosaicShape(m_startPoint,
                                       qMax(10, m_penSize * 5),
                                       qMax(2, m_penSize));
        mosaic->setCanvasSnapshot(m_backgroundImage);
        m_currentShape = mosaic;
        m_draggingShape = true;
        update();
        return;
    }

    if (m_shapeType == BLUR) {
        auto *blur = new BlurShape(m_startPoint,
                                   qMax(10, m_penSize * 5),
                                   qMax(2, m_penSize));
        blur->setCanvasSnapshot(m_backgroundImage);
        m_currentShape = blur;
        m_draggingShape = true;
        update();
        return;
    }

    m_currentShape = m_factory.createShape(m_shapeType,
                                           m_startPoint,
                                           m_startPoint,
                                           m_penColor,
                                           m_penSize);
    m_draggingShape = (m_currentShape != nullptr);
    update();
}

void StickyCanvasWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (!m_drawingEnabled || m_shapeType != TEXT || event->button() != Qt::LeftButton) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }

    const QPoint displayPoint = event->position().toPoint();
    const QPoint imagePoint(qRound(displayPoint.x() / m_viewScale),
                            qRound(displayPoint.y() / m_viewScale));
    const QRect bounds = annotationBounds();
    if (!bounds.isEmpty() && !bounds.contains(imagePoint)) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }

    const int textIndex = findTopmostTextShapeIndex(imagePoint);
    TextShape *textShape = textShapeAt(textIndex);
    if (!textShape) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }

    const QPoint anchorPoint = textShape->anchorPoint();
    const QPoint displayAnchor(qRound(anchorPoint.x() * m_viewScale),
                               qRound(anchorPoint.y() * m_viewScale));
    const QString text = textShape->text();
    const QColor color = textShape->color();
    const int size = textShape->size();
    const bool withBackground = textShape->withBackground();

    delete m_shapes.takeAt(textIndex);
    clearSelectedShape();
    update();
    emit textEditRequested(displayAnchor, text, color, size, withBackground);
}

void StickyCanvasWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_drawingEnabled) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const QPoint rawImagePoint(
        qRound(event->position().x() / m_viewScale),
        qRound(event->position().y() / m_viewScale));
    const QPoint imagePoint = clampPointToAnnotationBounds(rawImagePoint);

    if (m_pressingEditHandle && (event->buttons() & Qt::LeftButton)) {
        if (!selectedTwoPointShape()) {
            clearSelectedShape();
            QWidget::mouseMoveEvent(event);
            return;
        }

        if (!m_draggingEditHandle) {
            const int dragDistance =
                (event->position().toPoint() - m_selectedShapePressDisplayPoint).manhattanLength();
            if (dragDistance < QApplication::startDragDistance()) {
                return;
            }
            m_draggingEditHandle = true;
        }

        updateEditHandle(m_activeEditHandle, imagePoint);
        update();
        return;
    }

    if (m_pressingSelectedShape && (event->buttons() & Qt::LeftButton)) {
        Shape *shape = selectedShape();
        if (!shape) {
            clearSelectedShape();
            QWidget::mouseMoveEvent(event);
            return;
        }

        if (!m_draggingSelectedShape) {
            const int dragDistance =
                (event->position().toPoint() - m_selectedShapePressDisplayPoint).manhattanLength();
            if (dragDistance < QApplication::startDragDistance()) {
                return;
            }
            m_draggingSelectedShape = true;
        }

        const QPoint requestedOffset = imagePoint - m_selectedShapeLastPoint;
        const QPoint offset = constrainedTranslation(shape, requestedOffset);
        if (!offset.isNull()) {
            shape->translate(offset);
            m_selectedShapeLastPoint += offset;
            update();
        }
        return;
    }

    if (!m_draggingShape || !m_currentShape) {
        updateHoverCursor(rawImagePoint);
        QWidget::mouseMoveEvent(event);
        return;
    }

    m_currentShape->setEndPoint(imagePoint);
    update();
}

void StickyCanvasWidget::mouseReleaseEvent(QMouseEvent *event)
{
    const QPoint rawImagePoint(qRound(event->position().x() / m_viewScale),
                               qRound(event->position().y() / m_viewScale));

    if (m_pressingEditHandle && event->button() == Qt::LeftButton) {
        m_pressingEditHandle = false;
        m_draggingEditHandle = false;
        m_activeEditHandle = EditHandle::None;
        m_selectedShapePressDisplayPoint = QPoint();
        m_editStartRect = {};
        updateHoverCursor(rawImagePoint);
        update();
        return;
    }

    if (m_pressingSelectedShape && event->button() == Qt::LeftButton) {
        m_pressingSelectedShape = false;
        m_draggingSelectedShape = false;
        m_selectedShapePressDisplayPoint = QPoint();
        m_selectedShapeLastPoint = QPoint();
        updateHoverCursor(rawImagePoint);
        update();
        return;
    }

    if (!m_draggingShape || !m_currentShape || event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    const QPoint imagePoint = clampPointToAnnotationBounds(rawImagePoint);
    m_currentShape->setEndPoint(imagePoint);
    m_shapes.append(m_currentShape);
    m_currentShape = nullptr;
    m_draggingShape = false;
    update();
}

void StickyCanvasWidget::keyPressEvent(QKeyEvent *event)
{
    if (event && (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)) {
        if (deleteSelectedShape()) {
            event->accept();
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

void StickyCanvasWidget::finishCurrentShape()
{
    if (m_currentShape) {
        delete m_currentShape;
        m_currentShape = nullptr;
    }
    m_draggingShape = false;
}

void StickyCanvasWidget::clearShapes()
{
    qDeleteAll(m_shapes);
    m_shapes.clear();
    clearSelectedShape();
}

void StickyCanvasWidget::clearSelectedShape()
{
    m_selectedShapeIndex = -1;
    m_pressingSelectedShape = false;
    m_draggingSelectedShape = false;
    m_activeEditHandle = EditHandle::None;
    m_pressingEditHandle = false;
    m_draggingEditHandle = false;
    m_selectedShapePressDisplayPoint = QPoint();
    m_selectedShapeLastPoint = QPoint();
    m_editStartRect = {};
    unsetCursor();
}

int StickyCanvasWidget::findTopmostShapeIndex(const QPoint &point) const
{
    const QRect bounds = annotationBounds();
    if (!bounds.isEmpty() && !bounds.contains(point)) {
        return -1;
    }

    const int tolerance = selectionHitTolerance();
    for (int i = m_shapes.size() - 1; i >= 0; --i) {
        Shape *shape = m_shapes.at(i);
        if (shape && shape->contains(point, tolerance)) {
            return i;
        }
    }
    return -1;
}

int StickyCanvasWidget::findTopmostTextShapeIndex(const QPoint &point) const
{
    for (int i = m_shapes.size() - 1; i >= 0; --i) {
        TextShape *textShape = textShapeAt(i);
        if (textShape && textShape->contains(point)) {
            return i;
        }
    }
    return -1;
}

TextShape *StickyCanvasWidget::textShapeAt(int index) const
{
    if (index < 0 || index >= m_shapes.size()) {
        return nullptr;
    }
    return dynamic_cast<TextShape *>(m_shapes.at(index));
}

Shape *StickyCanvasWidget::selectedShape() const
{
    if (m_selectedShapeIndex < 0 || m_selectedShapeIndex >= m_shapes.size()) {
        return nullptr;
    }
    return m_shapes.at(m_selectedShapeIndex);
}

TwoPointShape *StickyCanvasWidget::selectedTwoPointShape() const
{
    return dynamic_cast<TwoPointShape *>(selectedShape());
}

int StickyCanvasWidget::selectionHitTolerance() const
{
    return qMax(4, qRound(6.0 / qMax<qreal>(0.01, m_viewScale)));
}

int StickyCanvasWidget::editHandleRadius() const
{
    return qMax(4, qRound(5.0 / qMax<qreal>(0.01, m_viewScale)));
}

QRect StickyCanvasWidget::annotationBounds() const
{
    const QRect displayBounds = m_annotationBounds.normalized().intersected(rect());
    if (displayBounds.isEmpty()) {
        return {};
    }
    const QPoint topLeft(qRound(displayBounds.left() / m_viewScale),
                         qRound(displayBounds.top() / m_viewScale));
    const QPoint bottomRight(qRound(displayBounds.right() / m_viewScale),
                             qRound(displayBounds.bottom() / m_viewScale));
    return QRect(topLeft, bottomRight).normalized();
}

QPoint StickyCanvasWidget::clampPointToAnnotationBounds(const QPoint &point) const
{
    const QRect bounds = annotationBounds();
    if (bounds.isEmpty()) {
        return point;
    }
    return QPoint(qBound(bounds.left(), point.x(), bounds.right()),
                  qBound(bounds.top(), point.y(), bounds.bottom()));
}

QPoint StickyCanvasWidget::constrainedTranslation(Shape *shape, const QPoint &offset) const
{
    if (!shape || offset.isNull()) {
        return {};
    }

    const QRect bounds = annotationBounds();
    if (bounds.isEmpty()) {
        return offset;
    }

    const QRect shapeBounds = shape->boundingRect();
    if (shapeBounds.isEmpty()) {
        return offset;
    }

    int dx = offset.x();
    int dy = offset.y();
    if (shapeBounds.left() + dx < bounds.left()) {
        dx = bounds.left() - shapeBounds.left();
    }
    if (shapeBounds.right() + dx > bounds.right()) {
        dx = bounds.right() - shapeBounds.right();
    }
    if (shapeBounds.top() + dy < bounds.top()) {
        dy = bounds.top() - shapeBounds.top();
    }
    if (shapeBounds.bottom() + dy > bounds.bottom()) {
        dy = bounds.bottom() - shapeBounds.bottom();
    }
    return QPoint(dx, dy);
}

StickyCanvasWidget::EditHandle StickyCanvasWidget::editHandleAt(const QPoint &point) const
{
    const QRect bounds = annotationBounds();
    if (!bounds.isEmpty() && !bounds.contains(point)) {
        return EditHandle::None;
    }

    Shape *shape = selectedShape();
    auto *twoPointShape = dynamic_cast<TwoPointShape *>(shape);
    if (!twoPointShape) {
        return EditHandle::None;
    }

    const int radius = editHandleRadius() + 2;
    if (dynamic_cast<ArrowShape *>(shape)) {
        if (pointHitsHandle(point, twoPointShape->startPoint(), radius)) {
            return EditHandle::ArrowStart;
        }
        if (pointHitsHandle(point, twoPointShape->endPoint(), radius)) {
            return EditHandle::ArrowEnd;
        }
        return EditHandle::None;
    }

    if (!dynamic_cast<RectShape *>(shape) && !dynamic_cast<EllipseShape *>(shape)) {
        return EditHandle::None;
    }

    const QRect rect = normalizedRectFromPoints(twoPointShape->startPoint(), twoPointShape->endPoint());
    const QVector<QPair<EditHandle, QPoint>> handles {
        { EditHandle::TopLeft, rect.topLeft() },
        { EditHandle::Top, QPoint(rect.center().x(), rect.top()) },
        { EditHandle::TopRight, rect.topRight() },
        { EditHandle::Right, QPoint(rect.right(), rect.center().y()) },
        { EditHandle::BottomRight, rect.bottomRight() },
        { EditHandle::Bottom, QPoint(rect.center().x(), rect.bottom()) },
        { EditHandle::BottomLeft, rect.bottomLeft() },
        { EditHandle::Left, QPoint(rect.left(), rect.center().y()) }
    };
    for (const auto &handle : handles) {
        if (pointHitsHandle(point, handle.second, radius)) {
            return handle.first;
        }
    }
    return EditHandle::None;
}

void StickyCanvasWidget::updateEditHandle(EditHandle handle, const QPoint &point)
{
    TwoPointShape *shape = selectedTwoPointShape();
    if (!shape || handle == EditHandle::None) {
        return;
    }

    if (handle == EditHandle::ArrowStart) {
        shape->setStartPoint(point);
        return;
    }
    if (handle == EditHandle::ArrowEnd) {
        shape->setEndPoint(point);
        return;
    }

    QRect rect = m_editStartRect.isEmpty()
        ? normalizedRectFromPoints(shape->startPoint(), shape->endPoint())
        : m_editStartRect;
    int left = rect.left();
    int right = rect.right();
    int top = rect.top();
    int bottom = rect.bottom();

    switch (handle) {
    case EditHandle::TopLeft:
        left = point.x();
        top = point.y();
        break;
    case EditHandle::Top:
        top = point.y();
        break;
    case EditHandle::TopRight:
        right = point.x();
        top = point.y();
        break;
    case EditHandle::Right:
        right = point.x();
        break;
    case EditHandle::BottomRight:
        right = point.x();
        bottom = point.y();
        break;
    case EditHandle::Bottom:
        bottom = point.y();
        break;
    case EditHandle::BottomLeft:
        left = point.x();
        bottom = point.y();
        break;
    case EditHandle::Left:
        left = point.x();
        break;
    default:
        return;
    }

    const QRect nextRect(QPoint(left, top), QPoint(right, bottom));
    const QRect normalized = nextRect.normalized();
    shape->setStartPoint(normalized.topLeft());
    shape->setEndPoint(normalized.bottomRight());
}

void StickyCanvasWidget::updateHoverCursor(const QPoint &point)
{
    if (!m_drawingEnabled || m_draggingShape) {
        unsetCursor();
        return;
    }

    const EditHandle handle = editHandleAt(point);
    if (handle != EditHandle::None) {
        setCursor(cursorForEditHandle(handle));
        return;
    }

    if (findTopmostShapeIndex(point) >= 0) {
        setCursor(Qt::SizeAllCursor);
        return;
    }

    unsetCursor();
}

Qt::CursorShape StickyCanvasWidget::cursorForEditHandle(EditHandle handle) const
{
    switch (handle) {
    case EditHandle::TopLeft:
    case EditHandle::BottomRight:
        return Qt::SizeFDiagCursor;
    case EditHandle::TopRight:
    case EditHandle::BottomLeft:
        return Qt::SizeBDiagCursor;
    case EditHandle::Top:
    case EditHandle::Bottom:
        return Qt::SizeVerCursor;
    case EditHandle::Left:
    case EditHandle::Right:
        return Qt::SizeHorCursor;
    case EditHandle::ArrowStart:
    case EditHandle::ArrowEnd:
        return Qt::CrossCursor;
    default:
        return Qt::ArrowCursor;
    }
}

void StickyCanvasWidget::drawSelectedShapeOutline(QPainter *painter) const
{
    if (!painter) {
        return;
    }

    Shape *shape = selectedShape();
    if (!shape) {
        return;
    }

    const QRect bounds = shape->boundingRect();
    if (bounds.isEmpty()) {
        return;
    }

    QPen pen(shape->color().isValid() ? shape->color() : QColor(QStringLiteral("#2563EB")));
    if (pen.color().alpha() == 0) {
        pen.setColor(QColor(QStringLiteral("#2563EB")));
    }
    pen.setWidth(1);
    pen.setCosmetic(true);
    pen.setStyle(Qt::CustomDashLine);
    pen.setDashPattern({ 3, 3 });
    painter->save();
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(bounds.adjusted(-3, -3, 3, 3));
    painter->restore();

    drawEditHandles(painter);
}

void StickyCanvasWidget::drawEditHandles(QPainter *painter) const
{
    if (!painter) {
        return;
    }

    Shape *shape = selectedShape();
    auto *twoPointShape = dynamic_cast<TwoPointShape *>(shape);
    if (!twoPointShape) {
        return;
    }

    QVector<QPoint> handles;
    if (dynamic_cast<ArrowShape *>(shape)) {
        handles.append(twoPointShape->startPoint());
        handles.append(twoPointShape->endPoint());
    } else if (dynamic_cast<RectShape *>(shape) || dynamic_cast<EllipseShape *>(shape)) {
        const QRect rect = normalizedRectFromPoints(twoPointShape->startPoint(), twoPointShape->endPoint());
        handles = {
            rect.topLeft(),
            QPoint(rect.center().x(), rect.top()),
            rect.topRight(),
            QPoint(rect.right(), rect.center().y()),
            rect.bottomRight(),
            QPoint(rect.center().x(), rect.bottom()),
            rect.bottomLeft(),
            QPoint(rect.left(), rect.center().y())
        };
    } else {
        return;
    }

    const int radius = editHandleRadius();
    painter->save();
    QPen pen(QColor(QStringLiteral("#2563EB")));
    pen.setWidth(1);
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->setBrush(Qt::white);
    for (const QPoint &handlePoint : handles) {
        painter->drawRect(QRect(handlePoint.x() - radius,
                                handlePoint.y() - radius,
                                radius * 2,
                                radius * 2));
    }
    painter->restore();
}
