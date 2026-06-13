#include "app/tzshot_widget_runtime.h"

#include "app/tzshot_services.h"

TZShotWidgetRuntime::TZShotWidgetRuntime(TZShotServices &services, QObject *parent)
    : QObject(parent)
    , m_captureOverlayWidget(&services.screenshotViewModel(),
                             &services.stickyViewModel(),
                             &services.storageViewModel())
    , m_settingsDialog(&services.storageViewModel(),
                       &services.languageManager(),
                       &services.globalShortcut())
    , m_aboutDialog()
    , m_longCaptureController(&services.scrollCaptureViewModel())
    , m_widgetWindowBridge(&m_settingsDialog,
                           &m_captureOverlayWidget,
                           &m_longCaptureController,
                           this)
    , m_trayHelper(this)
{
    m_captureOverlayWidget.setWidgetWindowBridge(&m_widgetWindowBridge);
    m_trayHelper.setIcon(QStringLiteral(":/resource/img/tray_icon.svg"));
    m_trayHelper.show();
}
