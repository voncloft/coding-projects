#include "EditableLabel.h"

#include <QInputDialog>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QPaintEvent>

EditableLabel::EditableLabel(const QString &placeholderText, QWidget *parent)
    : QLabel(parent)
    , m_placeholderText(placeholderText)
    , m_editDialogTitle(tr("Edit Text"))
    , m_editFieldLabel(tr("Text:"))
{
    setWordWrap(true);
    setCursor(Qt::PointingHandCursor);
    setProperty("placeholderVisible", true);
    refresh();
}

QString EditableLabel::value() const
{
    return m_value;
}

void EditableLabel::setValue(const QString &value)
{
    const QString normalized = value.trimmed();
    if (m_value == normalized) {
        refresh();
        return;
    }

    m_value = normalized;
    refresh();
    emit valueChanged(m_value);
}

void EditableLabel::setPlaceholderText(const QString &placeholderText)
{
    m_placeholderText = placeholderText;
    refresh();
}

void EditableLabel::setEditDialogTitle(const QString &title)
{
    m_editDialogTitle = title;
}

void EditableLabel::setEditFieldLabel(const QString &label)
{
    m_editFieldLabel = label;
}

void EditableLabel::setStrikeThroughEnabled(bool enabled)
{
    if (m_strikeThroughEnabled == enabled) {
        return;
    }

    m_strikeThroughEnabled = enabled;
    update();
}

void EditableLabel::setStrikeThroughColor(const QColor &color)
{
    if (m_strikeThroughColor == color) {
        return;
    }

    m_strikeThroughColor = color;
    update();
}

void EditableLabel::setStrikeThroughThickness(qreal thickness)
{
    const qreal normalized = qMax<qreal>(1.0, thickness);
    if (qFuzzyCompare(m_strikeThroughThickness, normalized)) {
        return;
    }

    m_strikeThroughThickness = normalized;
    update();
}

void EditableLabel::beginEditing()
{
    bool accepted = false;
    const QString updatedValue = QInputDialog::getText(
        this,
        m_editDialogTitle,
        m_editFieldLabel,
        QLineEdit::Normal,
        m_value,
        &accepted);

    if (!accepted) {
        return;
    }

    setValue(updatedValue);
}

void EditableLabel::mouseDoubleClickEvent(QMouseEvent *event)
{
    beginEditing();
    event->accept();
}

void EditableLabel::paintEvent(QPaintEvent *event)
{
    QLabel::paintEvent(event);

    if (!m_strikeThroughEnabled || text().trimmed().isEmpty()) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(m_strikeThroughColor, m_strikeThroughThickness, Qt::SolidLine, Qt::RoundCap));

    const QRect availableRect = contentsRect().adjusted(margin(), margin(), -margin(), -margin());
    int textFlags = alignment();
    if (wordWrap()) {
        textFlags |= Qt::TextWordWrap;
    }

    const QFontMetrics metrics(font());
    const QRect textRect = metrics.boundingRect(availableRect, textFlags, text());
    if (textRect.isEmpty()) {
        return;
    }

    const int lineSpacing = qMax(1, metrics.lineSpacing());
    const int lineCount = qMax(1, (textRect.height() + lineSpacing - 1) / lineSpacing);
    const qreal strikeY = metrics.ascent() - metrics.strikeOutPos();

    for (int lineIndex = 0; lineIndex < lineCount; ++lineIndex) {
        const qreal y = textRect.top() + lineIndex * lineSpacing + strikeY;
        painter.drawLine(QPointF(textRect.left(), y), QPointF(textRect.right(), y));
    }
}

void EditableLabel::refresh()
{
    const bool showPlaceholder = m_value.isEmpty();
    setText(showPlaceholder ? m_placeholderText : m_value);
    setProperty("placeholderVisible", showPlaceholder);
    style()->unpolish(this);
    style()->polish(this);
    update();
}
