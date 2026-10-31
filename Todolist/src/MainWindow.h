#pragma once

#include <QColor>
#include <QMainWindow>
#include <QString>
#include <QVector>

class QResizeEvent;
class QJsonObject;
class QScrollArea;
class QShowEvent;
class QVBoxLayout;
class EditableLabel;
class QFrame;
class QPushButton;
class TodoItemWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    void loadTarget(const QString &target);

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    struct NoteTheme
    {
        QColor formBackground = QColor(QStringLiteral("#f3eddc"));
        QColor formForeground = QColor(QStringLiteral("#2b241c"));
        QColor buttonBackground = QColor(QStringLiteral("#efbf59"));
        QColor buttonForeground = QColor(QStringLiteral("#1f1a16"));
        QColor checkboxBackground = QColor(QStringLiteral("#f3eddc"));
        QColor checkboxForeground = QColor(QStringLiteral("#2b241c"));
        QColor strikeThroughForeground = QColor(QStringLiteral("#7a3d2f"));
        qreal formFontSize = 12.5;
        qreal buttonFontSize = 11.5;
    };

    void setupUi();
    void setupMenus();
    void applyScale();
    void applyTheme();
    void resizeWindowToFitItems();
    void openFormatDialog();
    void loadDefaultTheme();
    void saveCurrentThemeAsDefault() const;
    void clearSavedDefaultTheme() const;
    void updateColorButton(class QPushButton *button, const QColor &color) const;
    QJsonObject themeToJson() const;
    void loadThemeFromJson(const QJsonObject &themeObject);
    void addItem(const QString &text = {}, bool completed = false, bool focusEditor = false);
    void removeItem(TodoItemWidget *item);
    void clearItems();
    void updateWindowTitle();
    void updateStatusMessage(const QString &message = {});
    void markDirty();
    void setDirty(bool dirty);

    bool maybeSave();
    bool save();
    bool saveAs();
    bool saveToFile(const QString &path, bool showErrors = true);
    bool loadFromFile(const QString &path);

    void newList(bool promptToSave = true);
    void openList();
    void uncheckAll();
    void removeChecked();

    QString displayTitle() const;
    QString defaultDirectory() const;
    QString defaultSavePath() const;

    QFrame *m_noteFrame;
    QPushButton *m_addButton;
    QPushButton *m_formatButton;
    EditableLabel *m_titleLabel;
    QScrollArea *m_itemsScrollArea;
    QWidget *m_itemsWidget;
    QVBoxLayout *m_noteLayout;
    QVBoxLayout *m_itemsLayout;
    QVector<TodoItemWidget *> m_items;
    NoteTheme m_theme;

    QString m_currentFile;
    QString m_suggestedDirectory;
    bool m_dirty = false;
    bool m_pendingResizeToFitItems = false;
};
