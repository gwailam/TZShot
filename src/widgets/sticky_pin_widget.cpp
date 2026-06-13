#include "widgets/sticky_pin_widget.h"

#include "widgets/sticky_canvas_widget.h"
#include "sticky_image_store.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QFile>
#include <QContextMenuEvent>
#include <QEvent>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QInputDialog>
#include <QKeyEvent>
#include <QPushButton>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QKeySequence>
#include <QPlainTextEdit>
#include <QScreen>
#include <QSpinBox>
#include <QStyle>
#include <QTimer>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>
#include <QToolButton>
#include <QToolTip>
#include <QVariant>
#include <QVector>
#include <QVBoxLayout>
#include <QWindow>
#include <QGuiApplication>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellscalingapi.h>
#endif

namespace {

class AnnotationTextPanel : public QWidget
{
public:
    explicit AnnotationTextPanel(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TranslucentBackground, true);
    }

    void setBorderColor(const QColor &color)
    {
        if (m_borderColor == color) {
            return;
        }
        m_borderColor = color;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QWidget::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 1));
        painter.drawRect(rect());

        QPen pen(m_borderColor);
        pen.setWidth(1);
        pen.setStyle(Qt::CustomDashLine);
        pen.setDashPattern({ 3, 3 });
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }

private:
    QColor m_borderColor = QColor(QStringLiteral("#F43F5E"));
};

QString loadStickyPinStyleSheet()
{
    QFile file(QStringLiteral(":/resource/qss/sticky_pin_widget.qss"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

qreal monitorScaleForPoint(const QPoint &physicalPoint)
{
#ifdef Q_OS_WIN
    const POINT nativePoint { physicalPoint.x(), physicalPoint.y() };
    if (HMONITOR monitor = MonitorFromPoint(nativePoint, MONITOR_DEFAULTTONEAREST)) {
        using GetDpiForMonitorFn = HRESULT (WINAPI *)(HMONITOR, MONITOR_DPI_TYPE, UINT *, UINT *);
        static const auto getDpiForMonitor = reinterpret_cast<GetDpiForMonitorFn>(
            GetProcAddress(LoadLibraryW(L"Shcore.dll"), "GetDpiForMonitor"));
        if (getDpiForMonitor) {
            UINT dpiX = 0;
            UINT dpiY = 0;
            if (SUCCEEDED(getDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)) && dpiX > 0) {
                return qreal(dpiX) / 96.0;
            }
        }
    }
#endif
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        return screen->devicePixelRatio();
    }
    return 1.0;
}

bool isAltWheelActive(const QWheelEvent *event)
{
    if (event && event->modifiers().testFlag(Qt::AltModifier)) {
        return true;
    }
#ifdef Q_OS_WIN
    return (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
#else
    return QGuiApplication::keyboardModifiers().testFlag(Qt::AltModifier);
#endif
}

QPoint physicalOffsetForLogicalPoint(const QPoint &logicalPoint, const QPoint &referencePhysicalPoint)
{
#ifdef Q_OS_WIN
    const qreal dpr = qMax<qreal>(1.0, monitorScaleForPoint(referencePhysicalPoint));
    return QPoint(qRound(logicalPoint.x() * dpr),
                  qRound(logicalPoint.y() * dpr));
#else
    Q_UNUSED(referencePhysicalPoint);
    return logicalPoint;
#endif
}

QString normalizeAnnotationDraft(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text;
}

QString finalizeAnnotationText(const QString &text)
{
    return normalizeAnnotationDraft(text).trimmed();
}

void configureAnnotationEditor(QPlainTextEdit *editor)
{
    if (!editor) {
        return;
    }

    editor->setFrameStyle(QFrame::NoFrame);
    editor->setBackgroundVisible(false);
    editor->setTabChangesFocus(false);
    editor->setWordWrapMode(QTextOption::NoWrap);
    editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

int annotationTextPixelSize(int penSize)
{
    return qMax(12, penSize * 4);
}

QFont annotationTextFont(int penSize)
{
    QFont font(QStringLiteral("Microsoft YaHei"));
    font.setStyleStrategy(QFont::PreferAntialias);
    font.setHintingPreference(QFont::PreferNoHinting);
    font.setPixelSize(annotationTextPixelSize(penSize));
    return font;
}

void syncAnnotationEditorFont(QPlainTextEdit *editor, int penSize)
{
    if (!editor) {
        return;
    }

    editor->setFont(annotationTextFont(penSize));
}

void syncInlineAnnotationEditorAppearance(QWidget *panel,
                                          QPlainTextEdit *editor,
                                          const QColor &color,
                                          int penSize)
{
    if (!panel || !editor) {
        return;
    }

    syncAnnotationEditorFont(editor, penSize);
    editor->setStyleSheet(QStringLiteral("color:%1; margin:0px; padding:0px; background:transparent; border:none;")
        .arg(color.name()));
    static_cast<AnnotationTextPanel *>(panel)->setBorderColor(color);
}

void syncOptionButtonState(const QVector<QToolButton *> &buttons,
                           const char *propertyName,
                           const QVariant &expectedValue)
{
    for (QToolButton *button : buttons) {
        if (!button) {
            continue;
        }
        button->setChecked(button->property(propertyName) == expectedValue);
    }
}

}

StickyPinWidget::StickyPinWidget(const QString &imageUrl,
                                 const QRect &physicalRect,
                                 const QImage &image,
                                 StickyImageStore *store,
                                 QWidget *parent)
    : QWidget(parent)
    , m_imageUrl(imageUrl)
    , m_physicalRect(physicalRect.normalized())
    , m_image(image)
    , m_store(store)
{
    m_annotationText.clear();

    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setContextMenuPolicy(Qt::DefaultContextMenu);
    setStyleSheet(loadStickyPinStyleSheet());

    const qreal dpr = screenScaleForRect(m_physicalRect);
    if (!m_image.isNull() && dpr > 0.0 && !qFuzzyCompare(m_image.devicePixelRatio(), dpr)) {
        m_image.setDevicePixelRatio(dpr);
    }

    m_imageDisplaySize = QSize(qMax(1, qRound(m_image.width() / qMax<qreal>(1.0, m_image.devicePixelRatio()))),
                               qMax(1, qRound(m_image.height() / qMax<qreal>(1.0, m_image.devicePixelRatio()))));
    m_baseImageDisplaySize = m_imageDisplaySize;

    m_canvas = new StickyCanvasWidget(this);
    m_canvas->setBackgroundImage(m_image);
    m_canvas->setViewScale(m_zoomFactor);
    m_canvas->setAnnotationBounds(QRect(QPoint(0, 0), m_imageDisplaySize));
    m_canvas->setContentOpacity(m_contentOpacity);
    m_canvas->setPenColor(m_currentPenColor);
    m_canvas->setPenSize(m_currentPenSize);
    m_canvas->setActiveShapeType(PEN);
    m_canvas->setNumberAutoIncrement(m_numberAutoIncrement);
    m_canvas->setNumberValue(m_numberValue);
    connect(m_canvas, &StickyCanvasWidget::numberValueChanged, this, [this](int value) {
        m_numberValue = value;
        if (m_numberValueSpin && m_numberValueSpin->value() != value) {
            m_numberValueSpin->setValue(value);
        }
        if (m_numberLabel) {
            m_numberLabel->setText(QString::number(value));
        }
    });
    connect(m_canvas, &StickyCanvasWidget::textPlacementRequested, this, [this](const QPoint &point) {
        m_inlineEditingExistingText = false;
        m_inlineOriginalText.clear();
        m_annotationText.clear();
        showInlineTextEditor(point, QString());
    });
    connect(m_canvas, &StickyCanvasWidget::textEditRequested, this,
            [this](const QPoint &point,
                   const QString &text,
                   const QColor &color,
                   int size,
                   bool withBackground) {
        m_inlineEditingExistingText = true;
        m_inlineOriginalText = text;
        m_inlineOriginalColor = color;
        m_inlineOriginalSize = size;
        m_inlineOriginalTextBackground = withBackground;
        m_currentPenColor = color;
        m_currentPenSize = size;
        m_annotationText = text;
        syncCanvasToolSettings();
        updateToolOptionsPanel();
        updateLayoutAndSize();
        showInlineTextEditor(point, text);
    });
    m_toolOptions = new QWidget(this);
    m_toolOptions->setObjectName(QStringLiteral("stickyToolOptions"));

    const QVector<QColor> colors {
        QColor("#F43F5E"), QColor("#EF4444"), QColor("#F97316"), QColor("#EAB308"),
        QColor("#22C55E"), QColor("#3B82F6"), QColor("#8B5CF6"), QColor("#000000"), QColor("#FFFFFF")
    };
    for (const QColor &color : colors) {
        auto *button = new QToolButton(m_toolOptions);
        button->setCheckable(true);
        button->setAutoRaise(false);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setCursor(Qt::PointingHandCursor);
        button->setProperty("annotationColor", color);
        button->setStyleSheet(QStringLiteral(
            "QToolButton { border: 1px solid #CBD5E1; border-radius: 9px; background-color: %1; }"
            "QToolButton:checked { border: 2px solid #3B82F6; background-color: %1; }"
        ).arg(color.name()));
        connect(button, &QToolButton::clicked, this, [this, button, color]() {
            for (QToolButton *other : m_colorButtons) {
                if (other != button) {
                    other->setChecked(false);
                }
            }
            button->setChecked(true);
            m_currentPenColor = color;
            syncCanvasToolSettings();
        });
        m_colorButtons.append(button);
    }

    struct SizePreset { int value; QString label; };
    const QVector<SizePreset> sizePresets {
        { 2, tr("细") }, { 4, tr("中") }, { 6, tr("粗") }, { 10, tr("特粗") }
    };
    for (const SizePreset &preset : sizePresets) {
        auto *button = new QToolButton(m_toolOptions);
        button->setCheckable(true);
        button->setText(preset.label);
        button->setCursor(Qt::PointingHandCursor);
        button->setProperty("annotationSize", preset.value);
        connect(button, &QToolButton::clicked, this, [this, button, preset]() {
            for (QToolButton *other : m_sizeButtons) {
                if (other != button) {
                    other->setChecked(false);
                }
            }
            button->setChecked(true);
            m_currentPenSize = preset.value;
            syncCanvasToolSettings();
            if (m_canvas && m_canvas->drawingEnabled() && m_canvas->activeShapeType() == TEXT) {
                updateToolOptionsPanel();
                updateLayoutAndSize();
            }
        });
        m_sizeButtons.append(button);
    }

    m_inlineTextPanel = new AnnotationTextPanel(this);
    m_inlineTextPanel->setObjectName(QStringLiteral("stickyInlineTextPanel"));
    m_inlineTextPanel->setAttribute(Qt::WA_StyledBackground, true);
    m_inlineTextPanel->setAttribute(Qt::WA_TranslucentBackground, true);
    m_inlineTextPanel->hide();
    auto *inlineLayout = new QHBoxLayout(m_inlineTextPanel);
    inlineLayout->setContentsMargins(3, 3, 3, 3);
    inlineLayout->setSpacing(0);

    m_inlineTextEditor = new QPlainTextEdit(m_inlineTextPanel);
    m_inlineTextEditor->setObjectName(QStringLiteral("stickyInlineTextEditor"));
    configureAnnotationEditor(m_inlineTextEditor);
    m_inlineTextEditor->setPlaceholderText(QString());
    m_inlineTextEditor->installEventFilter(this);
    inlineLayout->addWidget(m_inlineTextEditor, 1);
    connect(m_inlineTextEditor, &QPlainTextEdit::textChanged, this, [this]() {
        m_annotationText = normalizeAnnotationDraft(m_inlineTextEditor->toPlainText());
        if (m_inlineTextPanel && m_inlineTextPanel->isVisible()) {
            updateInlineTextEditorGeometry();
        }
    });

    m_numberAutoIncrementCheck = new QCheckBox(tr("自动递增"), m_toolOptions);
    m_numberAutoIncrementCheck->setChecked(m_numberAutoIncrement);
    connect(m_numberAutoIncrementCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_numberAutoIncrement = checked;
        if (m_canvas) {
            m_canvas->setNumberAutoIncrement(checked);
        }
    });

    m_numberMinusButton = new QToolButton(m_toolOptions);
    m_numberMinusButton->setText(QStringLiteral("-"));
    m_numberMinusButton->setCursor(Qt::PointingHandCursor);
    connect(m_numberMinusButton, &QToolButton::clicked, this, [this]() {
        const int nextValue = qMax(1, m_numberValue - 1);
        if (m_numberValueSpin) {
            m_numberValueSpin->setValue(nextValue);
        }
    });

    m_numberLabel = new QLabel(QString::number(m_numberValue), m_toolOptions);
    m_numberLabel->setObjectName(QStringLiteral("stickyNumberLabel"));
    m_numberLabel->setAlignment(Qt::AlignCenter);

    m_numberPlusButton = new QToolButton(m_toolOptions);
    m_numberPlusButton->setText(QStringLiteral("+"));
    m_numberPlusButton->setCursor(Qt::PointingHandCursor);
    connect(m_numberPlusButton, &QToolButton::clicked, this, [this]() {
        const int nextValue = qMin(9999, m_numberValue + 1);
        if (m_numberValueSpin) {
            m_numberValueSpin->setValue(nextValue);
        }
    });

    m_numberValueSpin = new QSpinBox(m_toolOptions);
    m_numberValueSpin->setRange(1, 9999);
    m_numberValueSpin->setValue(m_numberValue);
    connect(m_numberValueSpin, &QSpinBox::valueChanged, this, [this](int value) {
        m_numberValue = value;
        if (m_numberLabel) {
            m_numberLabel->setText(QString::number(value));
        }
        if (m_canvas) {
            m_canvas->setNumberValue(value);
        }
    });

    m_toolbar = new QWidget(this);
    m_toolbar->setObjectName(QStringLiteral("stickyToolbar"));

    m_pencilButton = new QToolButton(m_toolbar);
    m_rectButton = new QToolButton(m_toolbar);
    m_circleButton = new QToolButton(m_toolbar);
    m_arrowButton = new QToolButton(m_toolbar);
    m_highlightButton = new QToolButton(m_toolbar);
    m_mosaicButton = new QToolButton(m_toolbar);
    m_blurButton = new QToolButton(m_toolbar);
    m_textButton = new QToolButton(m_toolbar);
    m_numberButton = new QToolButton(m_toolbar);
    m_undoButton = new QToolButton(m_toolbar);
    m_copyButton = new QToolButton(m_toolbar);
    m_saveButton = new QToolButton(m_toolbar);
    m_zoomLabel = new QLabel(m_toolbar);
    m_toggleToolbarButton = new QToolButton(m_toolbar);
    m_closeButton = new QToolButton(m_toolbar);

    auto setupButton = [](QToolButton *button, const QString &tooltip) {
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setAutoRaise(false);
        button->setIconSize(QSize(18, 18));
        button->setToolTip(tooltip);
        button->setCursor(Qt::PointingHandCursor);
        button->setContentsMargins(0, 0, 0, 0);
    };

    setupButton(m_pencilButton, tr("画笔"));
    setupButton(m_rectButton, tr("矩形"));
    setupButton(m_circleButton, tr("圆形"));
    setupButton(m_arrowButton, tr("箭头"));
    setupButton(m_highlightButton, tr("高亮"));
    setupButton(m_mosaicButton, tr("马赛克"));
    setupButton(m_blurButton, tr("高斯模糊"));
    setupButton(m_textButton, tr("文字"));
    setupButton(m_numberButton, tr("序号"));
    setupButton(m_undoButton, tr("撤销"));
    setupButton(m_copyButton, tr("复制"));
    setupButton(m_saveButton, tr("保存"));
    setupButton(m_toggleToolbarButton, tr("隐藏工具栏"));
    setupButton(m_closeButton, tr("关闭"));

    m_zoomLabel->setAlignment(Qt::AlignCenter);
    m_zoomLabel->setMinimumWidth(52);
    m_zoomLabel->setText(QStringLiteral("100%"));
    m_zoomLabel->setStyleSheet(QStringLiteral(
        "color:#334155;"
        "background:transparent;"
        "border:none;"
        "font-size:12px;"
        "font-weight:500;"
    ));

    m_pencilButton->setCheckable(true);
    m_rectButton->setCheckable(true);
    m_circleButton->setCheckable(true);
    m_arrowButton->setCheckable(true);
    m_highlightButton->setCheckable(true);
    m_mosaicButton->setCheckable(true);
    m_blurButton->setCheckable(true);
    m_textButton->setCheckable(true);
    m_numberButton->setCheckable(true);
    m_pencilButton->setIcon(QIcon(QStringLiteral(":/resource/img/lc_pencil.svg")));
    m_rectButton->setIcon(QIcon(QStringLiteral(":/resource/img/lc_square.svg")));
    m_circleButton->setIcon(QIcon(QStringLiteral(":/resource/img/lc_circle.svg")));
    m_arrowButton->setIcon(QIcon(QStringLiteral(":/resource/img/lc_arrow.svg")));
    m_highlightButton->setIcon(QIcon(QStringLiteral(":/resource/img/lc_highlighter.svg")));
    m_mosaicButton->setIcon(QIcon(QStringLiteral(":/resource/img/lc_mosaic.svg")));
    m_blurButton->setIcon(QIcon(QStringLiteral(":/resource/img/lc_blur.svg")));
    m_textButton->setIcon(QIcon(QStringLiteral(":/resource/img/lc_text.svg")));
    m_numberButton->setIcon(QIcon(QStringLiteral(":/resource/img/lc_list_ordered.svg")));
    m_undoButton->setIcon(QIcon(QStringLiteral(":/resource/img/lc_undo.svg")));
    QIcon copyIcon = QIcon::fromTheme(QStringLiteral("edit-copy"));
    if (copyIcon.isNull()) {
        copyIcon = style()->standardIcon(QStyle::SP_FileDialogDetailedView);
    }
    m_copyButton->setIcon(copyIcon);
    m_saveButton->setIcon(QIcon(QStringLiteral(":/resource/img/lc_save.svg")));
    m_toggleToolbarButton->setIcon(QIcon(QStringLiteral(":/resource/img/lc_close.svg")));
    m_closeButton->setIcon(QIcon(QStringLiteral(":/resource/img/lc_x.svg")));

    const QVector<QToolButton*> drawButtons {
        m_pencilButton, m_rectButton, m_circleButton, m_arrowButton,
        m_highlightButton, m_mosaicButton, m_blurButton, m_textButton, m_numberButton
    };
    auto setOnlyChecked = [drawButtons](QToolButton *activeButton) {
        for (QToolButton *button : drawButtons) {
            if (button != activeButton) {
                button->setChecked(false);
            }
        }
    };

    connect(m_pencilButton, &QToolButton::clicked, this, [=]() { setOnlyChecked(m_pencilButton); setActiveTool(PEN, m_pencilButton); });
    connect(m_rectButton, &QToolButton::clicked, this, [=]() { setOnlyChecked(m_rectButton); setActiveTool(RECTANGLE, m_rectButton); });
    connect(m_circleButton, &QToolButton::clicked, this, [=]() { setOnlyChecked(m_circleButton); setActiveTool(ELLIPSE, m_circleButton); });
    connect(m_arrowButton, &QToolButton::clicked, this, [=]() { setOnlyChecked(m_arrowButton); setActiveTool(ARROW, m_arrowButton); });
    connect(m_highlightButton, &QToolButton::clicked, this, [=]() { setOnlyChecked(m_highlightButton); setActiveTool(HIGHLIGHT, m_highlightButton); });
    connect(m_mosaicButton, &QToolButton::clicked, this, [=]() { setOnlyChecked(m_mosaicButton); setActiveTool(MOSAIC, m_mosaicButton); });
    connect(m_blurButton, &QToolButton::clicked, this, [=]() { setOnlyChecked(m_blurButton); setActiveTool(BLUR, m_blurButton); });
    connect(m_textButton, &QToolButton::clicked, this, [=]() { setOnlyChecked(m_textButton); setActiveTool(TEXT, m_textButton); });
    connect(m_numberButton, &QToolButton::clicked, this, [=]() { setOnlyChecked(m_numberButton); setActiveTool(NUMBER, m_numberButton); });
    connect(m_undoButton, &QToolButton::clicked, this, [this]() {
        if (m_canvas) {
            m_canvas->undo();
        }
    });
    connect(m_copyButton, &QToolButton::clicked, this, [this]() {
        if (m_store) {
            m_store->copyImageToClipboard(m_imageUrl);
        }
    });
    connect(m_saveButton, &QToolButton::clicked, this, [this]() {
        saveCurrentImage();
    });
    connect(m_toggleToolbarButton, &QToolButton::clicked, this, [this]() {
        m_toolbarVisible = !m_toolbarVisible;
        updateLayoutAndSize();
        update();
    });
    connect(m_closeButton, &QToolButton::clicked, this, &QWidget::close);

    const auto wheelAwareWidgets = findChildren<QWidget*>();
    for (QWidget *child : wheelAwareWidgets) {
        if (!child) {
            continue;
        }
        child->installEventFilter(this);
    }

    updateLayoutAndSize();
    if (!m_colorButtons.isEmpty()) {
        m_colorButtons.first()->setChecked(true);
    }
    for (QToolButton *sizeButton : m_sizeButtons) {
        if (sizeButton->text() == tr("粗")) {
            sizeButton->setChecked(true);
            break;
        }
    }
    syncCanvasToolSettings();
    show();
    raise();
    activateWindow();
    applyNativePosition(m_physicalRect);
}

StickyPinWidget::~StickyPinWidget()
{
    releaseStoredImage();
}

void StickyPinWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.fillRect(rect(), Qt::transparent);

    const QRect imageRect = contentRect();
    const qreal contentOpacity = qBound<qreal>(0.05, m_contentOpacity, 1.0);
    if (m_shadowVisible) {
        for (int i = 10; i >= 1; --i) {
            const int alpha = qRound((3 + i * 2) * contentOpacity);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(47, 143, 255, alpha));
            painter.drawRoundedRect(imageRect.adjusted(-i, -i, i, i), 8, 8);
        }
    }

    painter.setPen(QPen(QColor(120, 182, 255, qRound(80 * contentOpacity)), 1));
    painter.setBrush(QColor(255, 255, 255, qRound(255 * contentOpacity)));
    painter.drawRoundedRect(imageRect, 4, 4);

    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(imageRect, 4, 4);
}

void StickyPinWidget::setActiveTool(Shapeype type, QToolButton *activeButton)
{
    const bool activate = activeButton && activeButton->isChecked();
    cancelInlineText();
    if (m_canvas) {
        m_canvas->setDrawingEnabled(activate);
        if (activate) {
            m_canvas->setActiveShapeType(type);
        }
    }
    syncCanvasToolSettings();
    updateToolOptionsPanel();
    updateLayoutAndSize();
    update();
}

void StickyPinWidget::syncCanvasToolSettings()
{
    if (!m_canvas) {
        return;
    }

    QColor penColor = m_currentPenColor;
    int penSize = m_currentPenSize;
    const Shapeype type = m_canvas->activeShapeType();
    if (type == HIGHLIGHT) {
        penColor.setAlpha(100);
        penSize = qMax(8, m_currentPenSize * 2);
    }

    m_canvas->setPenColor(penColor);
    m_canvas->setPenSize(penSize);
    m_canvas->setNumberAutoIncrement(m_numberAutoIncrement);
    m_canvas->setNumberValue(m_numberValue);

    syncOptionButtonState(m_colorButtons, "annotationColor", m_currentPenColor);
    syncOptionButtonState(m_sizeButtons, "annotationSize", m_currentPenSize);
    syncInlineAnnotationEditorAppearance(m_inlineTextPanel, m_inlineTextEditor, m_currentPenColor, m_currentPenSize);
    if (m_inlineTextPanel && m_inlineTextPanel->isVisible()) {
        updateInlineTextEditorGeometry();
    }
}

void StickyPinWidget::updateToolOptionsPanel()
{
    const bool visible = m_toolbarVisible && m_canvas && m_canvas->drawingEnabled();
    if (!m_toolOptions) {
        return;
    }

    m_toolOptions->setVisible(visible);
    if (!visible) {
        cancelInlineText();
        return;
    }

    const Shapeype type = m_canvas->activeShapeType();
    const bool showNumberOptions = (type == NUMBER);
    const bool showColorOptions = (type != MOSAIC && type != BLUR);

    int x = 10;
    int y = 10;
    for (QToolButton *button : m_colorButtons) {
        button->setVisible(showColorOptions);
        button->setGeometry(x, y, 18, 18);
        x += 24;
    }

    y += 26;
    x = 10;
    for (QToolButton *button : m_sizeButtons) {
        button->setVisible(true);
        button->setGeometry(x, y, 48, 28);
        x += 54;
    }

    if (m_numberAutoIncrementCheck && m_numberMinusButton && m_numberLabel && m_numberPlusButton) {
        m_numberAutoIncrementCheck->setVisible(showNumberOptions);
        m_numberMinusButton->setVisible(showNumberOptions);
        m_numberLabel->setVisible(showNumberOptions);
        m_numberPlusButton->setVisible(showNumberOptions);
        m_numberValueSpin->setVisible(false);
        m_numberAutoIncrementCheck->setGeometry(10, 68, 100, 24);
        m_numberMinusButton->setGeometry(118, 66, 28, 28);
        m_numberLabel->setGeometry(152, 66, 48, 28);
        m_numberPlusButton->setGeometry(206, 66, 28, 28);
    }
}

void StickyPinWidget::closeEvent(QCloseEvent *event)
{
    releaseStoredImage();
    QWidget::closeEvent(event);
}

void StickyPinWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QAction *copyAction = menu.addAction(tr("复制"));
    QAction *saveAction = menu.addAction(tr("保存原图"));
    QAction *toggleAction = menu.addAction(m_toolbarVisible ? tr("隐藏工具栏") : tr("显示工具栏"));
    QAction *shadowAction = menu.addAction(m_shadowVisible ? tr("隐藏阴影") : tr("显示阴影"));
    menu.addSeparator();
    QAction *closeAction = menu.addAction(tr("关闭"));
    QAction *chosen = menu.exec(event->globalPos());
    if (chosen == copyAction) {
        if (m_store) {
            m_store->copyImageToClipboard(m_imageUrl);
        }
        return;
    }
    if (chosen == saveAction) {
        saveCurrentImage();
        return;
    }
    if (chosen == toggleAction) {
        m_toolbarVisible = !m_toolbarVisible;
        updateLayoutAndSize();
        update();
        return;
    }
    if (chosen == shadowAction) {
        m_shadowVisible = !m_shadowVisible;
        update();
        return;
    }
    if (chosen == closeAction) {
        close();
    }
}

void StickyPinWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
#ifdef Q_OS_WIN
        if (HWND hwnd = reinterpret_cast<HWND>(winId())) {
            ReleaseCapture();
            SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            event->accept();
            return;
        }
#endif
    }
    QWidget::mousePressEvent(event);
}

void StickyPinWidget::keyPressEvent(QKeyEvent *event)
{
    if (event
        && (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
        && (!m_inlineTextPanel || !m_inlineTextPanel->isVisible())
        && m_canvas
        && m_canvas->deleteSelectedShape()) {
        event->accept();
        return;
    }

    if (event
        && event->matches(QKeySequence::Undo)
        && (!m_inlineTextPanel || !m_inlineTextPanel->isVisible())
        && m_canvas
        && m_canvas->hasAnnotations()) {
        m_canvas->undo();
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

bool StickyPinWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_inlineTextEditor && event) {
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                cancelInlineText();
                return true;
            }
            if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
                && keyEvent->modifiers().testFlag(Qt::ControlModifier)) {
                submitInlineText();
                return true;
            }
            if (keyEvent->matches(QKeySequence::Undo)) {
                return QWidget::eventFilter(watched, event);
            }
        }
        if (event->type() == QEvent::Wheel) {
            auto *wheelEvent = static_cast<QWheelEvent *>(event);
            const int delta = wheelEvent->angleDelta().y();
            if (delta != 0) {
                m_currentPenSize = qBound(3, m_currentPenSize + (delta > 0 ? 1 : -1), 40);
                syncCanvasToolSettings();
                updateToolOptionsPanel();
                updateLayoutAndSize();
                wheelEvent->accept();
                return true;
            }
        }
        if (event->type() == QEvent::FocusOut && m_inlineTextPanel && m_inlineTextPanel->isVisible()) {
            submitInlineText();
        }
    }
    if (event && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->matches(QKeySequence::Undo)
            && watched != m_inlineTextEditor
            && (!m_inlineTextPanel || !m_inlineTextPanel->isVisible())
            && m_canvas
            && m_canvas->hasAnnotations()) {
            m_canvas->undo();
            keyEvent->accept();
            return true;
        }
    }
    if (watched && event && event->type() == QEvent::Wheel) {
        auto *wheelEvent = static_cast<QWheelEvent *>(event);
        if (isAltOpacityAdjustActive(wheelEvent) && handleWheelInteraction(wheelEvent)) {
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void StickyPinWidget::wheelEvent(QWheelEvent *event)
{
    if (handleWheelInteraction(event)) {
        return;
    }
    QWidget::wheelEvent(event);
}

bool StickyPinWidget::handleWheelInteraction(QWheelEvent *event)
{
    if (!event) {
        return false;
    }

    const QPoint angleDelta = event->angleDelta();
    const int primaryDelta = (angleDelta.y() != 0) ? angleDelta.y() : angleDelta.x();
    const bool altActive = isAltOpacityAdjustActive(event);
    if (primaryDelta == 0) {
        return false;
    }

    if (altActive) {
        const qreal delta = primaryDelta > 0 ? 0.05 : -0.05;
        const qreal nextOpacity = qBound<qreal>(0.05, m_contentOpacity + delta, 1.0);
        setContentOpacity(nextOpacity);
        QToolTip::showText(event->globalPosition().toPoint(),
                           tr("透明度 %1%").arg(qRound(nextOpacity * 100)),
                           this);
        event->accept();
        return true;
    }

    const qreal step = primaryDelta > 0 ? 1.1 : 0.9;
    setZoomFactor(m_zoomFactor * step);
    event->accept();
    return true;
}

bool StickyPinWidget::isAltOpacityAdjustActive(const QWheelEvent *event) const
{
    return isAltWheelActive(event);
}

void StickyPinWidget::setContentOpacity(qreal value)
{
    const qreal next = qBound<qreal>(0.05, value, 1.0);
    if (qFuzzyCompare(m_contentOpacity, next)) {
        return;
    }

    m_contentOpacity = next;
    if (m_canvas) {
        m_canvas->setContentOpacity(next);
    }
    update();
}

void StickyPinWidget::setZoomFactor(qreal value)
{
    const qreal next = qBound<qreal>(0.3, value, 4.0);
    if (qFuzzyCompare(m_zoomFactor, next)) {
        return;
    }

    m_zoomFactor = next;
    m_imageDisplaySize = QSize(qMax(1, qRound(m_baseImageDisplaySize.width() * m_zoomFactor)),
                               qMax(1, qRound(m_baseImageDisplaySize.height() * m_zoomFactor)));
    if (m_canvas) {
        m_canvas->setViewScale(m_zoomFactor);
        m_canvas->setBackgroundImage(m_image);
        m_canvas->setContentOpacity(m_contentOpacity);
    }
    updateLayoutAndSize();
    update();
}

void StickyPinWidget::showInlineTextEditor(const QPoint &point, const QString &initialText)
{
    m_pendingTextPoint = point;
    if (!m_inlineTextPanel || !m_inlineTextEditor) {
        return;
    }

    m_annotationText = normalizeAnnotationDraft(initialText);
    if (m_inlineTextEditor->toPlainText() != m_annotationText) {
        m_inlineTextEditor->setPlainText(m_annotationText);
    }
    updateInlineTextEditorGeometry();
    m_inlineTextPanel->show();
    m_inlineTextPanel->raise();
    m_inlineTextEditor->setFocus();
    m_inlineTextEditor->moveCursor(QTextCursor::End);
}

void StickyPinWidget::updateInlineTextEditorGeometry()
{
    if (!m_inlineTextPanel || !m_inlineTextEditor) {
        return;
    }

    const QRect canvasRect = contentRect();
    m_inlineTextEditor->document()->adjustSize();
    const QSize docSize = m_inlineTextEditor->document()->size().toSize();
    const QFontMetrics fm(m_inlineTextEditor->font());
    int panelW = qMax(fm.horizontalAdvance(QStringLiteral(" ")), docSize.width()) + 6;
    int panelH = qMax(fm.lineSpacing(), docSize.height()) + 6;
    panelW = qMin(panelW, qMax(1, canvasRect.width() - 12));
    panelW = qMin(panelW, qMax(1, width() - 12));
    panelH = qMin(panelH, qMax(36, height() - 12));
    const int padY = qMax(2, m_currentPenSize / 2);
    int x = canvasRect.x() + m_pendingTextPoint.x();
    int y = canvasRect.y() + m_pendingTextPoint.y() - fm.ascent() - padY;
    x = qMax(6, qMin(x, width() - panelW - 6));
    y = qMax(6, qMin(y, height() - panelH - 6));

    m_inlineTextPanel->setGeometry(x, y, panelW, panelH);
}

void StickyPinWidget::submitInlineText()
{
    if (!m_inlineTextEditor) {
        return;
    }

    const QString text = finalizeAnnotationText(m_inlineTextEditor->toPlainText());
    if (m_canvas && !text.isEmpty()) {
        m_annotationText = text;
        m_canvas->addTextAnnotation(m_pendingTextPoint,
                                    text,
                                    m_currentPenColor,
                                    m_currentPenSize,
                                    m_inlineEditingExistingText ? m_inlineOriginalTextBackground : false);
    }

    cancelInlineText(false);
}

void StickyPinWidget::cancelInlineText(bool restoreOriginal)
{
    if (restoreOriginal && m_inlineEditingExistingText && m_canvas && !m_inlineOriginalText.isEmpty()) {
        m_annotationText = m_inlineOriginalText;
        m_canvas->addTextAnnotation(m_pendingTextPoint,
                                    m_inlineOriginalText,
                                    m_inlineOriginalColor,
                                    m_inlineOriginalSize,
                                    m_inlineOriginalTextBackground);
    }

    if (m_inlineTextPanel) {
        m_inlineTextPanel->hide();
    }
    m_pendingTextPoint = QPoint();
    m_inlineEditingExistingText = false;
    m_inlineOriginalText.clear();
    m_inlineOriginalColor = QColor("#F43F5E");
    m_inlineOriginalSize = 6;
    m_inlineOriginalTextBackground = false;
}

void StickyPinWidget::releaseStoredImage()
{
    if (m_released || !m_store || m_imageUrl.isEmpty()) {
        return;
    }
    m_released = true;
    m_store->releaseImage(m_imageUrl);
}

bool StickyPinWidget::saveCurrentImage()
{
    if (m_image.isNull()) {
        return false;
    }

    const QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("选择保存位置"),
        QString(),
        tr("PNG (*.png);;JPG (*.jpg);;BMP (*.bmp)"));

    if (filePath.isEmpty()) {
        return false;
    }

    return m_image.save(filePath);
}

qreal StickyPinWidget::screenScaleForRect(const QRect &physicalRect) const
{
    if (physicalRect.isEmpty()) {
        return 1.0;
    }
    return monitorScaleForPoint(physicalRect.center());
}

void StickyPinWidget::applyNativePosition(const QRect &physicalRect)
{
    const QPoint contentOffset = physicalOffsetForLogicalPoint(contentRect().topLeft(),
                                                               physicalRect.topLeft());
    const QPoint nativeTopLeft = physicalRect.topLeft() - contentOffset;
#ifdef Q_OS_WIN
    if (HWND hwnd = reinterpret_cast<HWND>(winId())) {
        SetWindowPos(hwnd,
                     HWND_TOPMOST,
                     nativeTopLeft.x(),
                     nativeTopLeft.y(),
                     0,
                     0,
                     SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
        return;
    }
#endif
    move(nativeTopLeft);
}

void StickyPinWidget::updateLayoutAndSize()
{
    const QPoint contentTopLeft = currentContentTopLeftPhysical();
    const int totalWidth = qMax(m_imageDisplaySize.width(), toolbarWidth()) + shadowPadding() * 2;
    const int totalHeight = m_imageDisplaySize.height()
        + shadowPadding() * 2
        + (m_toolbarVisible ? (toolbarGap() + toolbarHeight()) : 0);
    const int optionsReserve = (m_toolOptions && m_toolOptions->isVisible())
        ? (toolOptionsGap() + toolOptionsHeight())
        : 0;
    const int finalHeight = totalHeight + optionsReserve;

    setFixedSize(totalWidth, finalHeight);

    if (m_toolbar) {
        const QRect barRect = toolbarRect();
        m_toolbar->setVisible(m_toolbarVisible);
        m_toolbar->setGeometry(barRect);
    }

    if (m_canvas) {
        m_canvas->setGeometry(contentRect());
        m_canvas->setAnnotationBounds(QRect(QPoint(0, 0), contentRect().size()));
    }

    if (m_toolOptions) {
        m_toolOptions->setGeometry(toolOptionsRect());
        updateToolOptionsPanel();
    }

    if (m_pencilButton && m_rectButton && m_circleButton && m_arrowButton
        && m_highlightButton && m_mosaicButton && m_blurButton
        && m_textButton && m_numberButton
        && m_undoButton && m_copyButton && m_saveButton && m_zoomLabel
        && m_toggleToolbarButton && m_closeButton) {
        const int buttonH = 32;
        const int buttonW = 32;
        const int zoomW = 52;
        const int buttonY = (toolbarHeight() - buttonH) / 2;
        const int gap = 2;
        int x = 6;
        m_pencilButton->setGeometry(x, buttonY, buttonW, buttonH);
        x += buttonW + gap;
        m_rectButton->setGeometry(x, buttonY, buttonW, buttonH);
        x += buttonW + gap;
        m_circleButton->setGeometry(x, buttonY, buttonW, buttonH);
        x += buttonW + gap;
        m_arrowButton->setGeometry(x, buttonY, buttonW, buttonH);
        x += buttonW + gap;
        m_highlightButton->setGeometry(x, buttonY, buttonW, buttonH);
        x += buttonW + gap;
        m_mosaicButton->setGeometry(x, buttonY, buttonW, buttonH);
        x += buttonW + gap;
        m_blurButton->setGeometry(x, buttonY, buttonW, buttonH);
        x += buttonW + gap;
        m_textButton->setGeometry(x, buttonY, buttonW, buttonH);
        x += buttonW + gap;
        m_numberButton->setGeometry(x, buttonY, buttonW, buttonH);
        x += buttonW + gap;
        m_undoButton->setGeometry(x, buttonY, buttonW, buttonH);
        x += buttonW + gap;
        m_copyButton->setGeometry(x, buttonY, buttonW, buttonH);
        x += buttonW + gap;
        m_saveButton->setGeometry(x, buttonY, buttonW, buttonH);
        x += buttonW + gap;
        m_zoomLabel->setGeometry(x, buttonY, zoomW, buttonH);
        m_zoomLabel->setText(QString::number(qRound(m_zoomFactor * 100)) + QStringLiteral("%"));
        x += zoomW + gap;
        m_toggleToolbarButton->setGeometry(x, buttonY, buttonW, buttonH);
        x += buttonW + gap;
        m_closeButton->setGeometry(x, buttonY, buttonW, buttonH);
        m_toggleToolbarButton->setToolTip(m_toolbarVisible ? tr("隐藏工具栏") : tr("显示工具栏"));
        const char *toggleIconPath = m_toolbarVisible
            ? ":/resource/img/lc_close.svg"
            : ":/resource/img/lc_check.svg";
        m_toggleToolbarButton->setIcon(QIcon(QString::fromLatin1(toggleIconPath)));
    }

    applyNativePosition(QRect(contentTopLeft, m_imageDisplaySize));
}

QPoint StickyPinWidget::currentContentTopLeftPhysical() const
{
#ifdef Q_OS_WIN
    if (HWND hwnd = reinterpret_cast<HWND>(const_cast<StickyPinWidget*>(this)->winId())) {
        RECT rc{};
        if (GetWindowRect(hwnd, &rc)) {
            const QPoint windowTopLeft(rc.left, rc.top);
            const QPoint contentOffset = physicalOffsetForLogicalPoint(contentRect().topLeft(),
                                                                       windowTopLeft);
            return windowTopLeft + contentOffset;
        }
    }
#endif
    if (isVisible()) {
        return geometry().topLeft() + contentRect().topLeft();
    }
    return m_physicalRect.topLeft();
}

QRect StickyPinWidget::contentRect() const
{
    const int x = (width() - m_imageDisplaySize.width()) / 2;
    return QRect(x, shadowPadding(), m_imageDisplaySize.width(), m_imageDisplaySize.height());
}

QRect StickyPinWidget::toolOptionsRect() const
{
    const int w = toolOptionsWidth();
    int x = (width() - w) / 2;
    if (QToolButton *anchorButton = currentActiveToolButton()) {
        const int centerX = toolbarRect().x() + anchorButton->geometry().center().x();
        x = centerX - w / 2;
        x = qMax(shadowPadding(), qMin(x, width() - w - shadowPadding()));
    }
    const int y = toolbarRect().bottom() + 1 + toolOptionsGap();
    return QRect(x, y, w, toolOptionsHeight());
}

QRect StickyPinWidget::toolbarRect() const
{
    const int w = toolbarWidth();
    const int x = (width() - w) / 2;
    const int y = contentRect().bottom() + 1 + toolbarGap();
    return QRect(x, y, w, toolbarHeight());
}

int StickyPinWidget::shadowPadding() const
{
    return 12;
}

int StickyPinWidget::toolbarGap() const
{
    return 8;
}

int StickyPinWidget::toolOptionsGap() const
{
    return 8;
}

int StickyPinWidget::toolOptionsHeight() const
{
    if (m_canvas && m_canvas->drawingEnabled()) {
        const Shapeype type = m_canvas->activeShapeType();
        if (type == NUMBER) {
            return 116;
        }
    }
    return 88;
}

int StickyPinWidget::toolOptionsWidth() const
{
    return 260;
}

QToolButton *StickyPinWidget::currentActiveToolButton() const
{
    const QVector<QToolButton*> drawButtons {
        m_pencilButton, m_rectButton, m_circleButton, m_arrowButton,
        m_highlightButton, m_mosaicButton, m_blurButton, m_textButton, m_numberButton
    };
    for (QToolButton *button : drawButtons) {
        if (button && button->isChecked()) {
            return button;
        }
    }
    return nullptr;
}

int StickyPinWidget::toolbarHeight() const
{
    return 44;
}

int StickyPinWidget::toolbarWidth() const
{
    return 32 * 14 + 52 + 2 * 14 + 12;
}
