#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <QPointer>
#include <QDialog>

class StorageViewModel;
class LanguageManager;
class GlobalShortcut;
class QListWidget;
class QStackedWidget;
class QLabel;
class QComboBox;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    SettingsDialog(StorageViewModel *storageViewModel,
                   LanguageManager *languageManager,
                   GlobalShortcut *globalShortcut,
                   QWidget *parent = nullptr);

    void showAndActivate();

signals:
    void infoMessageRequested(const QString &title, const QString &message);

private:
    QWidget *buildGeneralPage();
    QWidget *buildHotkeyPage();
    QWidget *createSectionTitle(const QString &title);
    QWidget *createCard();
    QWidget *createHotkeyRow(const QString &label, int actionId, const QString &seq);
    QString shortcutForAction(int actionId) const;
    void refreshHotkeyPage();

    QPointer<StorageViewModel> m_storageViewModel;
    QPointer<LanguageManager> m_languageManager;
    QPointer<GlobalShortcut> m_globalShortcut;

    QListWidget *m_navList = nullptr;
    QStackedWidget *m_stack = nullptr;

    QLabel *m_savePathLabel = nullptr;
    QComboBox *m_languageCombo = nullptr;
    QWidget *m_hotkeyPage = nullptr;
};

#endif // SETTINGS_DIALOG_H
