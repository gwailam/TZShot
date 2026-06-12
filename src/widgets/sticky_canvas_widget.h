#ifndef STICKY_CANVAS_WIDGET_H
#define STICKY_CANVAS_WIDGET_H

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QWidget>

#include "paint_board/shape/shape.h"
#include "paint_board/shape/shape_factory.h"
#include "paint_board/shape/shape_type.h"

class TextShape;
class TwoPointShape;

class StickyCanvasWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StickyCanvasWidget(QWidget *parent = nullptr);
    ~StickyCanvasWidget() override;

    void setBackgroundImage(const QImage &image);
    void setBackgroundVisible(bool visible);
    void setViewScale(qreal scale);
    void setAnnotationBounds(const QRect &displayBounds);
    void setContentOpacity(qreal opacity);
    void setActiveShapeType(Shapeype type);
    void setPenColor(const QColor &color);
    void setPenSize(int size);
    void setDrawingEnabled(bool enabled);
    void setNumberValue(int value);
    void setNumberAutoIncrement(bool enabled);
    bool drawingEnabled() const { return m_drawingEnabled; }
    Shapeype activeShapeType() const { return m_shapeType; }
    bool hasAnnotations() const { return !m_shapes.isEmpty(); }
    QImage compositedImage() const;
    QImage compositedImage(const QRect &displayRect) const;
    // 将标注层（按当前 viewScale 缩放）绘制到外部 painter，
    // 供调用方以任意目标分辨率合成输出。
    void renderAnnotations(QPainter *painter) const;
    void addTextAnnotation(const QPoint &point,
                           const QString &text,
                           const QColor &color,
                           int size,
                           bool withBackground);
    void undo();
    bool deleteSelectedShape();
    void reset();

signals:
    void numberValueChanged(int value);
    void textPlacementRequested(const QPoint &point);
    void textEditRequested(const QPoint &point,
                           const QString &text,
                           const QColor &color,
                           int size,
                           bool withBackground);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    enum class EditHandle
    {
        None,
        TopLeft,
        Top,
        TopRight,
        Right,
        BottomRight,
        Bottom,
        BottomLeft,
        Left,
        ArrowStart,
        ArrowEnd
    };

    void finishCurrentShape();
    void clearShapes();
    void clearSelectedShape();
    int findTopmostShapeIndex(const QPoint &point) const;
    int findTopmostTextShapeIndex(const QPoint &point) const;
    Shape *selectedShape() const;
    TwoPointShape *selectedTwoPointShape() const;
    TextShape *textShapeAt(int index) const;
    int selectionHitTolerance() const;
    int editHandleRadius() const;
    QRect annotationBounds() const;
    QPoint clampPointToAnnotationBounds(const QPoint &point) const;
    QPoint constrainedTranslation(Shape *shape, const QPoint &offset) const;
    EditHandle editHandleAt(const QPoint &point) const;
    void updateEditHandle(EditHandle handle, const QPoint &point);
    void updateHoverCursor(const QPoint &point);
    Qt::CursorShape cursorForEditHandle(EditHandle handle) const;
    void drawSelectedShapeOutline(QPainter *painter) const;
    void drawEditHandles(QPainter *painter) const;

    ShapeFactory m_factory;
    QList<Shape*> m_shapes;
    Shape *m_currentShape = nullptr;
    QImage m_backgroundImage;
    bool m_backgroundVisible = true;
    QColor m_penColor = Qt::red;
    int m_penSize = 6;
    Shapeype m_shapeType = PEN;
    bool m_numberAutoIncrement = true;
    int m_numberValue = 1;
    bool m_drawingEnabled = false;
    bool m_draggingShape = false;
    int m_selectedShapeIndex = -1;
    bool m_pressingSelectedShape = false;
    bool m_draggingSelectedShape = false;
    EditHandle m_activeEditHandle = EditHandle::None;
    bool m_pressingEditHandle = false;
    bool m_draggingEditHandle = false;
    qreal m_viewScale = 1.0;
    qreal m_contentOpacity = 1.0;
    QRect m_annotationBounds;
    QPoint m_startPoint;
    QPoint m_selectedShapePressDisplayPoint;
    QPoint m_selectedShapeLastPoint;
    QRect m_editStartRect;
};

#endif // STICKY_CANVAS_WIDGET_H
