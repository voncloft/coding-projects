#include "EditableLabel.h"
#include "TodoItemWidget.h"

#include <QCheckBox>
#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>
#include <QSizePolicy>
#include <QStyle>
#include <QtMath>

namespace {

QColor blendedColor(const QColor &foreground, const QColor &background, qreal amount)
{
    const qreal clamped = qBound<qreal>(0.0, amount, 1.0);
    return QColor::fromRgbF(
        foreground.redF() * (1.0 - clamped) + background.redF() * clamped,
        foreground.greenF() * (1.0 - clamped) + background.greenF() * clamped,
        foreground.blueF() * (1.0 - clamped) + background.blueF() * clamped,
        1.0);
}

} // namespace

class ColorCheckBox : public QCheckBox
{
public:
    explicit ColorCheckBox(QWidget *parent = nullptr)
        : QCheckBox(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setText(QString());
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    void setIndicatorColors(const QColor &background, const QColor &foreground)
    {
        m_background = background;
        m_foreground = foreground;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const int side = qMin(width(), height()) - 4;
        QRectF boxRect((width() - side) / 2.0, (height() - side) / 2.0, side, side);

        painter.setPen(QPen(m_foreground, qMax(2.0, side / 8.0)));
        painter.setBrush(m_background);
        painter.drawRoundedRect(boxRect, 2.0, 2.0);

        if (isChecked()) {
            QPen pen(m_foreground, qMax(2.0, side / 7.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter.setPen(pen);

            const QPointF a(boxRect.left() + side * 0.20, boxRect.top() + side * 0.55);
            const QPointF b(boxRect.left() + side * 0.43, boxRect.top() + side * 0.78);
            const QPointF c(boxRect.left() + side * 0.80, boxRect.top() + side * 0.24);

            painter.drawLine(a, b);
            painter.drawLine(b, c);
        }
    }

private:
    QColor m_background = QColor(QStringLiteral("#f3eddc"));
    QColor m_foreground = QColor(QStringLiteral("#2b241c"));
};

TodoItemWidget::TodoItemWidget(QWidget *parent)
    : QWidget(parent)
    , m_checkBox(new ColorCheckBox(this))
    , m_textLabel(new EditableLabel(tr("Text (editable by double clicking)"), this))
    , m_removeButton(new QPushButton(this))
    , m_layout(new QHBoxLayout(this))
{
    setObjectName(QStringLiteral("todoItem"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_layout->setContentsMargins(0, 4, 0, 4);
    m_layout->setSpacing(12);

    m_checkBox->setToolTip(tr("Mark this item complete"));
    m_checkBox->setFixedSize(24, 24);

    m_textLabel->setObjectName(QStringLiteral("todoText"));
    m_textLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_textLabel->setEditDialogTitle(tr("Edit Todo Item"));
    m_textLabel->setEditFieldLabel(tr("Task text:"));
    m_textLabel->setToolTip(tr("Double-click to edit this item"));
    m_textLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_removeButton->setObjectName(QStringLiteral("removeButton"));
    m_removeButton->setText(tr("Remove"));
    m_removeButton->setToolTip(tr("Remove this checkbox"));
    m_removeButton->setCursor(Qt::PointingHandCursor);
    m_removeButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_layout->addWidget(m_removeButton, 0, Qt::AlignVCenter);
    m_layout->addWidget(m_checkBox, 0, Qt::AlignVCenter);
    m_layout->addWidget(m_textLabel, 1, Qt::AlignVCenter);

    connect(m_checkBox, &QCheckBox::toggled, this, [this]() {
        updateVisualState();
        emit changed();
    });
    connect(m_textLabel, &EditableLabel::valueChanged, this, [this]() {
        updateVisualState();
        emit changed();
    });
    connect(m_removeButton, &QPushButton::clicked, this, [this]() {
        emit removeRequested(this);
    });

    setScaleFactor(1.0);
    updateVisualState();
}

QString TodoItemWidget::text() const
{
    return m_textLabel->value();
}

void TodoItemWidget::setText(const QString &text)
{
    m_textLabel->setValue(text);
    updateVisualState();
}

bool TodoItemWidget::isCompleted() const
{
    return m_checkBox->isChecked();
}

void TodoItemWidget::setCompleted(bool completed)
{
    m_checkBox->setChecked(completed);
    updateVisualState();
}

void TodoItemWidget::focusEditor()
{
    m_textLabel->beginEditing();
}

void TodoItemWidget::setScaleFactor(qreal scaleFactor)
{
    m_scaleFactor = qBound<qreal>(0.85, scaleFactor, 2.1);

    m_layout->setContentsMargins(0, qRound(4 * m_scaleFactor), 0, qRound(4 * m_scaleFactor));
    m_layout->setSpacing(qRound(12 * m_scaleFactor));

    m_checkBox->setFixedSize(qRound(24 * m_scaleFactor), qRound(24 * m_scaleFactor));

    QFont removeFont = m_removeButton->font();
    removeFont.setPointSizeF(m_theme.buttonFontSize * m_scaleFactor);
    m_removeButton->setFont(removeFont);
    const QFontMetrics removeMetrics(removeFont);
    m_removeButton->setFixedWidth(qMax(qRound(110 * m_scaleFactor),
                                       removeMetrics.horizontalAdvance(m_removeButton->text()) + qRound(28 * m_scaleFactor)));
    m_removeButton->setMinimumHeight(qRound(36 * m_scaleFactor));

    QFont textFont = m_textLabel->font();
    textFont.setPointSizeF(m_theme.formFontSize * m_scaleFactor);
    m_textLabel->setFont(textFont);
    m_textLabel->setMinimumHeight(qRound(30 * m_scaleFactor));
    setMinimumHeight(qRound(44 * m_scaleFactor));

    refreshStyleSheet();
    updateVisualState();
}

void TodoItemWidget::setTheme(const TodoItemTheme &theme)
{
    m_theme = theme;
    refreshStyleSheet();
    updateVisualState();
}

void TodoItemWidget::refreshStyleSheet()
{
    const int indicatorSize = qRound(18 * m_scaleFactor);
    const int removeVerticalPadding = qRound(6 * m_scaleFactor);
    const int removeHorizontalPadding = qRound(10 * m_scaleFactor);
    const QString removeHover = m_theme.buttonBackground.darker(108).name();
    const QString removePressed = m_theme.buttonBackground.darker(118).name();
    const QColor fadedText = blendedColor(m_theme.formForeground, m_theme.formBackground, 0.45);
    m_checkBox->setIndicatorColors(m_theme.checkboxBackground, m_theme.checkboxForeground);

    setStyleSheet(QString(
        "#todoItem { background: transparent; border: none; }"
        "#removeButton {"
        "  background: %1;"
        "  color: %2;"
        "  border: 2px solid %3;"
        "  border-radius: 3px;"
        "  padding: %4px %5px;"
        "}"
        "#removeButton:hover { background: %6; }"
        "#removeButton:pressed { background: %7; }"
        "#todoText {"
        "  border: none;"
        "  background: transparent;"
        "  margin: 0;"
        "}"
    ).arg(m_theme.buttonBackground.name(),
          m_theme.buttonForeground.name(),
          m_theme.formForeground.name(),
          QString::number(removeVerticalPadding),
          QString::number(removeHorizontalPadding),
          removeHover,
          removePressed,
          fadedText.name(),
          QString::number(indicatorSize),
          m_theme.checkboxBackground.name(),
          m_theme.checkboxForeground.name(),
          m_theme.strikeThroughForeground.name()));
}

void TodoItemWidget::updateVisualState()
{
    const bool completed = m_checkBox->isChecked();
    const bool placeholderVisible = m_textLabel->value().trimmed().isEmpty();

    QFont font = m_textLabel->font();
    font.setStrikeOut(false);
    font.setItalic(placeholderVisible);
    m_textLabel->setFont(font);
    m_textLabel->setStrikeThroughEnabled(completed);
    m_textLabel->setStrikeThroughColor(m_theme.strikeThroughForeground);
    m_textLabel->setStrikeThroughThickness(qMax<qreal>(2.0, 2.2 * m_scaleFactor));

    QColor textColor;
    if (completed) {
        textColor = m_theme.strikeThroughForeground;
    } else if (placeholderVisible) {
        textColor = blendedColor(m_theme.formForeground, m_theme.formBackground, 0.45);
    } else {
        textColor = m_theme.formForeground;
    }

    QPalette palette = m_textLabel->palette();
    palette.setColor(QPalette::WindowText, textColor);
    palette.setColor(QPalette::Text, textColor);
    palette.setColor(QPalette::PlaceholderText, blendedColor(m_theme.formForeground, m_theme.formBackground, 0.45));
    m_textLabel->setPalette(palette);

    m_textLabel->setStyleSheet(QString(
        "color: %1;"
        "background: transparent;"
        "border: none;"
        "margin: 0;"
    ).arg(textColor.name()));

    m_textLabel->setProperty("completed", completed);
    m_textLabel->update();
}
