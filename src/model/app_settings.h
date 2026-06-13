#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <QString>
#include <QUrl>

// AppSettings — Model
// -------------------
// 负责应用配置数据的读写（QSettings 持久化）。
// 不继承 QObject，不依赖 QML，纯数据存取。
// 所有 setter 在值变更时自动写入 QSettings。

class AppSettings
{
public:
    AppSettings();

    // 语言配置（"zh_CN" 或 "en"）
    QString language() const;
    void    setLanguage(const QString &lang);

    // 截图保存路径
    QString savePath() const;
    void    setSavePath(const QString &path);

    // 贴图：关闭含未保存标注的贴图前是否弹出确认（默认开启）。
    // 设计为静态：贴图窗口（StickyPinWidget）无法方便地获取 AppSettings 实例，
    // 故以静态方法直接读写 QSettings，集中维护键名与默认值，避免散落的字符串常量。
    static bool confirmCloseStickyWithAnnotations();
    static void setConfirmCloseStickyWithAnnotations(bool enabled);

    // 长截图完成后是否自动打开保存目录（默认关闭）。
    // 结果已复制到剪贴板，多数场景无需弹目录；同为静态读写 QSettings。
    static bool openFolderAfterLongCapture();
    static void setOpenFolderAfterLongCapture(bool enabled);

private:
    QString m_language      = "zh_CN";
    QString m_savePath;
};

#endif // APP_SETTINGS_H
