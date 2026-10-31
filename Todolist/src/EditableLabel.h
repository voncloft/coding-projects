#pragma once

#include <QColor>
#include <QLabel>

class EditableLabel : public QLabel
{
    Q_OBJECT

public:
    explicit EditableLabel(const QString &placeholderText = {}, QWidget *parent = nullptr);

    QString value() const;
    void setValue(const QString &value);

    void setPlaceholderText(const QString &placeholderText);
    void setEditDialogTitle(const QString &title);
    void setEditFieldLabel(const QString &label);
    void setStrikeThroughEnabled(bool enabled);
    void setStrikeThroughColor(const QColor &color);
    void setStrikeThroughThickness(qreal thickness);

    void beginEditing();

signals:
    void valueChanged(const QString &value);

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void refresh();

    QString m_value;
    QString m_placeholderText;
    QString m_editDialogTitle;
    QString m_editFieldLabel;
    bool m_strikeThroughEnabled = false;
    QColor m_strikeThroughColor = QColor(QStringLiteral("#000000"));
    qreal m_strikeThroughThickness = 2.0;
};
