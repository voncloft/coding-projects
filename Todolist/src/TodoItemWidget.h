#pragma once

#include <QColor>
#include <QWidget>

class ColorCheckBox;
class QHBoxLayout;
class QPushButton;
class EditableLabel;

struct TodoItemTheme
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

class TodoItemWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TodoItemWidget(QWidget *parent = nullptr);

    QString text() const;
    void setText(const QString &text);

    bool isCompleted() const;
    void setCompleted(bool completed);

    void focusEditor();
    void setScaleFactor(qreal scaleFactor);
    void setTheme(const TodoItemTheme &theme);

signals:
    void changed();
    void removeRequested(TodoItemWidget *item);

private:
    void refreshStyleSheet();
    void updateVisualState();

    ColorCheckBox *m_checkBox;
    EditableLabel *m_textLabel;
    QPushButton *m_removeButton;
    QHBoxLayout *m_layout;
    TodoItemTheme m_theme;
    qreal m_scaleFactor = 1.0;
};
