#include "EditableLabel.h"
#include "MainWindow.h"
#include "TodoItemWidget.h"

#include <QAction>
#include <QColorDialog>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QShowEvent>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QtMath>

namespace {

QString sanitizedFileName(QString text)
{
    text = text.trimmed();
    if (text.isEmpty()) {
        return QStringLiteral("Todo List");
    }

    text.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|]+)")), QStringLiteral("_"));
    return text.left(64);
}

QString targetToLocalPath(const QString &target)
{
    const QUrl url = QUrl::fromUserInput(target);
    if (url.isValid() && url.isLocalFile()) {
        return url.toLocalFile();
    }

    return target;
}

QString colorHex(const QColor &color)
{
    return color.name(QColor::HexRgb);
}

QString normalizedJsonSavePath(QString path)
{
    if (path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
        return path;
    }

    return path + QStringLiteral(".json");
}

QColor readableButtonText(const QColor &background)
{
    return background.lightnessF() > 0.6 ? QColor(QStringLiteral("#1f1a16"))
                                         : QColor(QStringLiteral("#fffdf7"));
}

QString cssColor(const QColor &color)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(color.alpha());
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_noteFrame(nullptr)
    , m_addButton(nullptr)
    , m_formatButton(nullptr)
    , m_titleLabel(nullptr)
    , m_itemsScrollArea(nullptr)
    , m_itemsWidget(nullptr)
    , m_noteLayout(nullptr)
    , m_itemsLayout(nullptr)
{
    setupUi();
    setupMenus();
    loadDefaultTheme();
    newList(false);
}

void MainWindow::loadTarget(const QString &target)
{
    const QString localPath = targetToLocalPath(target);
    if (localPath.isEmpty()) {
        return;
    }

    const QFileInfo info(localPath);
    if (info.exists() && info.isFile()) {
        loadFromFile(info.absoluteFilePath());
        return;
    }

    if (info.exists() && info.isDir()) {
        m_suggestedDirectory = info.absoluteFilePath();
        updateStatusMessage(tr("Create a new list. Save will default to %1").arg(m_suggestedDirectory));
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!m_currentFile.isEmpty()) {
        saveToFile(m_currentFile, false);
    }

    event->accept();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    applyScale();

    if (m_pendingResizeToFitItems) {
        QTimer::singleShot(0, this, &MainWindow::resizeWindowToFitItems);
    }
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);

    if (m_pendingResizeToFitItems) {
        QTimer::singleShot(0, this, &MainWindow::resizeWindowToFitItems);
    }
}

void MainWindow::setupUi()
{
    resize(560, 460);
    setMinimumWidth(460);
    setMinimumHeight(220);

    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("deskSurface"));
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_noteFrame = new QFrame(central);
    m_noteFrame->setObjectName(QStringLiteral("noteFrame"));
    m_noteFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_noteLayout = new QVBoxLayout(m_noteFrame);
    m_noteLayout->setContentsMargins(18, 18, 18, 18);
    m_noteLayout->setSpacing(14);

    m_titleLabel = new EditableLabel(tr("Editable Title"), m_noteFrame);
    m_titleLabel->setObjectName(QStringLiteral("titleLabel"));
    m_titleLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    m_titleLabel->setEditDialogTitle(tr("Edit Title"));
    m_titleLabel->setEditFieldLabel(tr("List title:"));
    m_titleLabel->setToolTip(tr("Double-click to edit the title"));

    m_itemsScrollArea = new QScrollArea(m_noteFrame);
    m_itemsScrollArea->setObjectName(QStringLiteral("itemsScrollArea"));
    m_itemsScrollArea->setWidgetResizable(true);
    m_itemsScrollArea->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    m_itemsScrollArea->setFrameShape(QFrame::NoFrame);
    m_itemsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_itemsScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_itemsWidget = new QWidget(m_itemsScrollArea);
    m_itemsWidget->setObjectName(QStringLiteral("itemsContainer"));
    m_itemsWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    m_itemsLayout = new QVBoxLayout(m_itemsWidget);
    m_itemsLayout->setContentsMargins(0, 4, 0, 4);
    m_itemsLayout->setSpacing(10);
    m_itemsLayout->addStretch();

    m_itemsScrollArea->setWidget(m_itemsWidget);

    auto *footerLayout = new QHBoxLayout();
    footerLayout->setContentsMargins(0, 0, 0, 0);
    footerLayout->setSpacing(12);

    m_formatButton = new QPushButton(tr("Format"), m_noteFrame);
    m_formatButton->setObjectName(QStringLiteral("formatButton"));
    m_formatButton->setCursor(Qt::PointingHandCursor);
    footerLayout->addWidget(m_formatButton);
    footerLayout->addStretch();

    m_addButton = new QPushButton(tr("Add Checkbox"), m_noteFrame);
    m_addButton->setObjectName(QStringLiteral("addButton"));
    m_addButton->setCursor(Qt::PointingHandCursor);
    footerLayout->addWidget(m_addButton);

    m_noteLayout->addWidget(m_titleLabel);
    m_noteLayout->addWidget(m_itemsScrollArea, 1);
    m_noteLayout->addLayout(footerLayout);

    rootLayout->addWidget(m_noteFrame, 1);

    setCentralWidget(central);
    statusBar();
    statusBar()->hide();

    connect(m_addButton, &QPushButton::clicked, this, [this]() {
        addItem({}, false, true);
    });
    connect(m_formatButton, &QPushButton::clicked, this, &MainWindow::openFormatDialog);
    connect(m_titleLabel, &EditableLabel::valueChanged, this, [this]() {
        markDirty();
        updateWindowTitle();
    });

    applyTheme();
    applyScale();
}

void MainWindow::setupMenus()
{
    auto *fileMenu = menuBar()->addMenu(tr("&File"));

    auto *newAction = fileMenu->addAction(tr("&New"));
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, [this]() {
        newList(true);
    });

    auto *openAction = fileMenu->addAction(tr("&Open..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openList);

    auto *saveAction = fileMenu->addAction(tr("&Save"));
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::save);

    auto *saveAsAction = fileMenu->addAction(tr("Save &As..."));
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::saveAs);

    fileMenu->addSeparator();

    auto *quitAction = fileMenu->addAction(tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    auto *editMenu = menuBar()->addMenu(tr("&Edit"));

    auto *addAction = editMenu->addAction(tr("Add Checkbox"));
    addAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+A")));
    connect(addAction, &QAction::triggered, this, [this]() {
        addItem({}, false, true);
    });

    auto *uncheckAction = editMenu->addAction(tr("Uncheck All"));
    connect(uncheckAction, &QAction::triggered, this, &MainWindow::uncheckAll);

    auto *removeCheckedAction = editMenu->addAction(tr("Remove Checked"));
    connect(removeCheckedAction, &QAction::triggered, this, &MainWindow::removeChecked);

    auto *formatAction = editMenu->addAction(tr("Format Appearance..."));
    formatAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+F")));
    connect(formatAction, &QAction::triggered, this, &MainWindow::openFormatDialog);
}

void MainWindow::applyScale()
{
    if (!centralWidget() || !m_noteLayout || !m_titleLabel || !m_addButton) {
        return;
    }

    const qreal widthScale = static_cast<qreal>(centralWidget()->width()) / 560.0;
    const qreal heightScale = static_cast<qreal>(centralWidget()->height()) / 680.0;
    const qreal scale = qBound<qreal>(0.85, qMin(widthScale, heightScale), 1.9);

    m_noteLayout->setContentsMargins(qRound(18 * scale), qRound(18 * scale), qRound(18 * scale), qRound(18 * scale));
    m_noteLayout->setSpacing(qRound(14 * scale));
    m_itemsLayout->setContentsMargins(0, qRound(4 * scale), 0, qRound(4 * scale));
    m_itemsLayout->setSpacing(qRound(10 * scale));

    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSizeF((m_theme.formFontSize + 2.5) * scale);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);

    QFont addFont = m_addButton->font();
    addFont.setPointSizeF(m_theme.buttonFontSize * scale);
    addFont.setBold(true);
    m_addButton->setFont(addFont);
    m_addButton->setMinimumSize(qRound(150 * scale), qRound(56 * scale));

    QFont formatFont = m_formatButton->font();
    formatFont.setPointSizeF(m_theme.buttonFontSize * scale);
    formatFont.setBold(true);
    m_formatButton->setFont(formatFont);
    m_formatButton->setMinimumSize(qRound(118 * scale), qRound(50 * scale));

    for (TodoItemWidget *item : m_items) {
        item->setScaleFactor(scale);
    }
}

void MainWindow::resizeWindowToFitItems()
{
    if (!centralWidget() || !m_itemsScrollArea || !m_itemsLayout || m_items.isEmpty()) {
        return;
    }

    if (!isVisible()) {
        m_pendingResizeToFitItems = true;
        return;
    }

    ensurePolished();
    if (auto *layout = centralWidget()->layout()) {
        layout->activate();
    }
    m_noteLayout->activate();
    m_itemsLayout->activate();

    const int contentHeight = qMax(m_itemsWidget->minimumSizeHint().height(),
                                   m_itemsWidget->sizeHint().height());
    const int viewportHeight = m_itemsScrollArea->viewport()->height();
    if (contentHeight <= 0 || viewportHeight <= 0) {
        m_pendingResizeToFitItems = true;
        return;
    }

    int targetHeight = height() + contentHeight - viewportHeight;
    targetHeight = qMax(minimumHeight(), targetHeight);
    int maxHeight = targetHeight;

    if (QScreen *windowScreen = screen() ? screen() : QGuiApplication::primaryScreen()) {
        const QRect availableGeometry = windowScreen->availableGeometry();
        maxHeight = isVisible()
            ? qMax(minimumHeight(), availableGeometry.bottom() - frameGeometry().top() + 1)
            : availableGeometry.height();
        targetHeight = qMin(targetHeight, maxHeight);
    }

    if (targetHeight != height()) {
        m_pendingResizeToFitItems = true;
        resize(width(), targetHeight);
        return;
    }

    if (m_itemsScrollArea->verticalScrollBar()->maximum() > 0 && height() < maxHeight) {
        m_pendingResizeToFitItems = true;
        QTimer::singleShot(0, this, &MainWindow::resizeWindowToFitItems);
        return;
    }

    m_pendingResizeToFitItems = false;
}

void MainWindow::applyTheme()
{
    const QString buttonHover = m_theme.buttonBackground.darker(108).name();
    const QString buttonPressed = m_theme.buttonBackground.darker(118).name();
    QColor fadedText = m_theme.formForeground;
    fadedText.setAlpha(150);

    setStyleSheet(QString(
        "#deskSurface {"
        "  background: %1;"
        "}"
        "#noteFrame {"
        "  background: transparent;"
        "  border: none;"
        "  border-radius: 0px;"
        "}"
        "#itemsScrollArea, #itemsContainer {"
        "  background: transparent;"
        "  border: none;"
        "}"
        "#titleLabel {"
        "  color: %2;"
        "  border: none;"
        "  background: transparent;"
        "  padding: 2px 10px 8px 10px;"
        "}"
        "#titleLabel[placeholderVisible=\"true\"] {"
        "  color: %3;"
        "  font-style: italic;"
        "}"
        "#addButton {"
        "  background: %4;"
        "  color: %5;"
        "  border: 3px solid %5;"
        "  border-radius: 3px;"
        "  padding: 12px 16px;"
        "}"
        "#addButton:hover { background: %6; }"
        "#addButton:pressed { background: %7; }"
        "#formatButton {"
        "  background: %4;"
        "  color: %5;"
        "  border: 3px solid %5;"
        "  border-radius: 3px;"
        "  padding: 10px 14px;"
        "}"
        "#formatButton:hover { background: %6; }"
        "#formatButton:pressed { background: %7; }"
    ).arg(m_theme.formBackground.name(),
          m_theme.formForeground.name(),
          cssColor(fadedText),
          m_theme.buttonBackground.name(),
          m_theme.buttonForeground.name(),
          buttonHover,
          buttonPressed));

    TodoItemTheme itemTheme;
    itemTheme.formBackground = m_theme.formBackground;
    itemTheme.formForeground = m_theme.formForeground;
    itemTheme.buttonBackground = m_theme.buttonBackground;
    itemTheme.buttonForeground = m_theme.buttonForeground;
    itemTheme.checkboxBackground = m_theme.checkboxBackground;
    itemTheme.checkboxForeground = m_theme.checkboxForeground;
    itemTheme.strikeThroughForeground = m_theme.strikeThroughForeground;
    itemTheme.formFontSize = m_theme.formFontSize;
    itemTheme.buttonFontSize = m_theme.buttonFontSize;

    for (TodoItemWidget *item : m_items) {
        item->setTheme(itemTheme);
    }
}

void MainWindow::openFormatDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Format Appearance"));

    auto *layout = new QVBoxLayout(&dialog);
    auto *formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setFormAlignment(Qt::AlignTop);
    formLayout->setSpacing(10);
    layout->addLayout(formLayout);

    auto addColorControl = [&](const QString &label, const QString &role, QColor &color) {
        auto *button = new QPushButton(&dialog);
        button->setProperty("themeRole", role);
        updateColorButton(button, color);
        formLayout->addRow(label, button);
        connect(button, &QPushButton::clicked, &dialog, [this, &dialog, button, &color]() {
            const QColor chosen = QColorDialog::getColor(color, &dialog, tr("Choose Color"));
            if (!chosen.isValid() || chosen == color) {
                return;
            }

            color = chosen;
            updateColorButton(button, color);
            applyTheme();
            markDirty();
        });
    };

    addColorControl(tr("Button Background"), QStringLiteral("buttonBackground"), m_theme.buttonBackground);
    addColorControl(tr("Button Foreground"), QStringLiteral("buttonForeground"), m_theme.buttonForeground);
    addColorControl(tr("Form Background"), QStringLiteral("formBackground"), m_theme.formBackground);
    addColorControl(tr("Form Foreground"), QStringLiteral("formForeground"), m_theme.formForeground);
    addColorControl(tr("Checkbox Background"), QStringLiteral("checkboxBackground"), m_theme.checkboxBackground);
    addColorControl(tr("Checkbox Foreground"), QStringLiteral("checkboxForeground"), m_theme.checkboxForeground);
    addColorControl(tr("Strike-Through Foreground"), QStringLiteral("strikeThroughForeground"), m_theme.strikeThroughForeground);

    auto *formFontSizeSpin = new QDoubleSpinBox(&dialog);
    formFontSizeSpin->setRange(8.0, 32.0);
    formFontSizeSpin->setDecimals(1);
    formFontSizeSpin->setSingleStep(0.5);
    formFontSizeSpin->setValue(m_theme.formFontSize);
    formLayout->addRow(tr("Form Font Size"), formFontSizeSpin);
    connect(formFontSizeSpin, &QDoubleSpinBox::valueChanged, &dialog, [this](double value) {
        m_theme.formFontSize = value;
        applyTheme();
        applyScale();
        markDirty();
    });

    auto *buttonFontSizeSpin = new QDoubleSpinBox(&dialog);
    buttonFontSizeSpin->setRange(8.0, 32.0);
    buttonFontSizeSpin->setDecimals(1);
    buttonFontSizeSpin->setSingleStep(0.5);
    buttonFontSizeSpin->setValue(m_theme.buttonFontSize);
    formLayout->addRow(tr("Button Font Size"), buttonFontSizeSpin);
    connect(buttonFontSizeSpin, &QDoubleSpinBox::valueChanged, &dialog, [this](double value) {
        m_theme.buttonFontSize = value;
        applyTheme();
        applyScale();
        markDirty();
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    auto *saveDefaultButton = buttons->addButton(tr("Set As Default"), QDialogButtonBox::ActionRole);
    connect(saveDefaultButton, &QPushButton::clicked, &dialog, [this]() {
        saveCurrentThemeAsDefault();
    });

    auto *clearDefaultButton = buttons->addButton(tr("Clear Saved Default"), QDialogButtonBox::ActionRole);
    connect(clearDefaultButton, &QPushButton::clicked, &dialog, [this]() {
        clearSavedDefaultTheme();
    });

    auto *resetButton = buttons->addButton(tr("Reset Built-In Appearance"), QDialogButtonBox::ResetRole);
    connect(resetButton, &QPushButton::clicked, &dialog, [this, &dialog, formFontSizeSpin, buttonFontSizeSpin]() {
        m_theme = NoteTheme{};
        applyTheme();
        applyScale();
        markDirty();
        const auto colorButtons = dialog.findChildren<QPushButton *>();
        for (QPushButton *button : colorButtons) {
            const QString role = button->property("themeRole").toString();
            if (role == QStringLiteral("buttonBackground")) {
                updateColorButton(button, m_theme.buttonBackground);
            } else if (role == QStringLiteral("buttonForeground")) {
                updateColorButton(button, m_theme.buttonForeground);
            } else if (role == QStringLiteral("formBackground")) {
                updateColorButton(button, m_theme.formBackground);
            } else if (role == QStringLiteral("formForeground")) {
                updateColorButton(button, m_theme.formForeground);
            } else if (role == QStringLiteral("checkboxBackground")) {
                updateColorButton(button, m_theme.checkboxBackground);
            } else if (role == QStringLiteral("checkboxForeground")) {
                updateColorButton(button, m_theme.checkboxForeground);
            } else if (role == QStringLiteral("strikeThroughForeground")) {
                updateColorButton(button, m_theme.strikeThroughForeground);
            }
        }
        formFontSizeSpin->setValue(m_theme.formFontSize);
        buttonFontSizeSpin->setValue(m_theme.buttonFontSize);
    });
    layout->addWidget(buttons);

    dialog.exec();
}

void MainWindow::loadDefaultTheme()
{
    QSettings settings;
    const QByteArray savedTheme = settings.value(QStringLiteral("appearance/defaultTheme")).toByteArray();
    if (savedTheme.isEmpty()) {
        m_theme = NoteTheme{};
        applyTheme();
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(savedTheme);
    if (!document.isObject()) {
        m_theme = NoteTheme{};
        applyTheme();
        return;
    }

    const NoteTheme builtInTheme;
    const QJsonObject themeObject = document.object();
    auto readColor = [&](const char *key, const QColor &fallback) {
        const QString value = themeObject.value(QLatin1String(key)).toString();
        const QColor parsed(value);
        return parsed.isValid() ? parsed : fallback;
    };
    auto readSize = [&](const char *key, qreal fallback) {
        const QJsonValue value = themeObject.value(QLatin1String(key));
        return value.isDouble() ? value.toDouble(fallback) : fallback;
    };

    const QColor legacyButtonBackground = readColor("buttonBackground",
                                                    readColor("addButtonColor",
                                                              readColor("formatButtonColor",
                                                                        readColor("removeButtonColor", builtInTheme.buttonBackground))));
    m_theme.formBackground = readColor("formBackground",
                                       readColor("backgroundColor",
                                                 readColor("workspaceBackground",
                                                           readColor("noteBackground", builtInTheme.formBackground))));
    m_theme.formForeground = readColor("formForeground",
                                       readColor("primaryText",
                                                 readColor("noteBorder", builtInTheme.formForeground)));
    m_theme.buttonBackground = legacyButtonBackground;
    m_theme.buttonForeground = readColor("buttonForeground", readableButtonText(legacyButtonBackground));
    m_theme.checkboxBackground = readColor("checkboxBackground",
                                           readColor("checkboxFillColor",
                                                     readColor("formBackground", builtInTheme.checkboxBackground)));
    m_theme.checkboxForeground = readColor("checkboxForeground",
                                           readColor("checkboxMarkColor",
                                                     readColor("buttonForeground", builtInTheme.checkboxForeground)));
    m_theme.strikeThroughForeground = readColor("strikeThroughForeground",
                                                m_theme.checkboxForeground);
    m_theme.formFontSize = readSize("formFontSize", builtInTheme.formFontSize);
    m_theme.buttonFontSize = readSize("buttonFontSize", builtInTheme.buttonFontSize);
    applyTheme();
}

void MainWindow::saveCurrentThemeAsDefault() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("appearance/defaultTheme"),
                      QJsonDocument(themeToJson()).toJson(QJsonDocument::Compact));
    settings.sync();
}

void MainWindow::clearSavedDefaultTheme() const
{
    QSettings settings;
    settings.remove(QStringLiteral("appearance/defaultTheme"));
    settings.sync();
}

void MainWindow::updateColorButton(QPushButton *button, const QColor &color) const
{
    button->setText(colorHex(color).toUpper());
    button->setStyleSheet(QString(
        "QPushButton {"
        "  min-width: 120px;"
        "  padding: 6px 10px;"
        "  background: %1;"
        "  color: %2;"
        "  border: 2px solid %3;"
        "  border-radius: 3px;"
        "}"
    ).arg(color.name(),
          readableButtonText(color).name(),
          m_theme.formForeground.name()));
}

QJsonObject MainWindow::themeToJson() const
{
    QJsonObject themeObject;
    themeObject.insert(QStringLiteral("buttonBackground"), colorHex(m_theme.buttonBackground));
    themeObject.insert(QStringLiteral("buttonForeground"), colorHex(m_theme.buttonForeground));
    themeObject.insert(QStringLiteral("formBackground"), colorHex(m_theme.formBackground));
    themeObject.insert(QStringLiteral("formForeground"), colorHex(m_theme.formForeground));
    themeObject.insert(QStringLiteral("checkboxBackground"), colorHex(m_theme.checkboxBackground));
    themeObject.insert(QStringLiteral("checkboxForeground"), colorHex(m_theme.checkboxForeground));
    themeObject.insert(QStringLiteral("strikeThroughForeground"), colorHex(m_theme.strikeThroughForeground));
    themeObject.insert(QStringLiteral("formFontSize"), m_theme.formFontSize);
    themeObject.insert(QStringLiteral("buttonFontSize"), m_theme.buttonFontSize);
    return themeObject;
}

void MainWindow::loadThemeFromJson(const QJsonObject &themeObject)
{
    if (themeObject.isEmpty()) {
        loadDefaultTheme();
        return;
    }

    auto readColor = [&](const char *key, const QColor &fallback) {
        const QString value = themeObject.value(QLatin1String(key)).toString();
        const QColor parsed(value);
        return parsed.isValid() ? parsed : fallback;
    };
    auto readSize = [&](const char *key, qreal fallback) {
        const QJsonValue value = themeObject.value(QLatin1String(key));
        return value.isDouble() ? value.toDouble(fallback) : fallback;
    };

    const NoteTheme builtInTheme;
    const QColor legacyButtonBackground = readColor("buttonBackground",
                                                    readColor("addButtonColor",
                                                              readColor("formatButtonColor",
                                                                        readColor("removeButtonColor", builtInTheme.buttonBackground))));
    m_theme.formBackground = readColor("formBackground",
                                       readColor("backgroundColor",
                                                 readColor("workspaceBackground",
                                                           readColor("noteBackground", builtInTheme.formBackground))));
    m_theme.formForeground = readColor("formForeground",
                                       readColor("primaryText",
                                                 readColor("noteBorder", builtInTheme.formForeground)));
    m_theme.buttonBackground = legacyButtonBackground;
    m_theme.buttonForeground = readColor("buttonForeground", readableButtonText(legacyButtonBackground));
    m_theme.checkboxBackground = readColor("checkboxBackground",
                                           readColor("checkboxFillColor",
                                                     readColor("formBackground", builtInTheme.checkboxBackground)));
    m_theme.checkboxForeground = readColor("checkboxForeground",
                                           readColor("checkboxMarkColor",
                                                     readColor("buttonForeground", builtInTheme.checkboxForeground)));
    m_theme.strikeThroughForeground = readColor("strikeThroughForeground",
                                                m_theme.checkboxForeground);
    m_theme.formFontSize = readSize("formFontSize", builtInTheme.formFontSize);
    m_theme.buttonFontSize = readSize("buttonFontSize", builtInTheme.buttonFontSize);
    applyTheme();
}

void MainWindow::addItem(const QString &text, bool completed, bool focusEditor)
{
    auto *item = new TodoItemWidget(m_itemsWidget);
    item->setText(text);
    item->setCompleted(completed);

    connect(item, &TodoItemWidget::changed, this, &MainWindow::markDirty);
    connect(item, &TodoItemWidget::removeRequested, this, &MainWindow::removeItem);

    m_items.append(item);
    m_itemsLayout->insertWidget(m_itemsLayout->count() - 1, item);
    applyTheme();
    applyScale();

    markDirty();
    if (focusEditor) {
        item->focusEditor();
    }
}

void MainWindow::removeItem(TodoItemWidget *item)
{
    const int index = m_items.indexOf(item);
    if (index < 0) {
        return;
    }

    m_items.removeAt(index);
    m_itemsLayout->removeWidget(item);
    item->deleteLater();
    applyScale();
    markDirty();
}

void MainWindow::clearItems()
{
    while (!m_items.isEmpty()) {
        TodoItemWidget *item = m_items.takeLast();
        m_itemsLayout->removeWidget(item);
        item->deleteLater();
    }

    applyScale();
}

void MainWindow::updateWindowTitle()
{
    const QString baseName = displayTitle();
    const QString dirtySuffix = m_dirty ? QStringLiteral(" *") : QString();
    setWindowTitle(QStringLiteral("%1%2 - Todo List").arg(baseName, dirtySuffix));
}

void MainWindow::updateStatusMessage(const QString &message)
{
    if (!message.isEmpty()) {
        statusBar()->showMessage(message, 4000);
        return;
    }

    if (m_currentFile.isEmpty()) {
        statusBar()->showMessage(tr("Unsaved list"));
        return;
    }

    statusBar()->showMessage(tr("Saved to %1").arg(m_currentFile));
}

void MainWindow::markDirty()
{
    setDirty(true);
}

void MainWindow::setDirty(bool dirty)
{
    if (m_dirty == dirty) {
        if (!dirty) {
            updateWindowTitle();
        }
        return;
    }

    m_dirty = dirty;
    updateWindowTitle();
    if (!m_dirty) {
        updateStatusMessage();
    }
}

bool MainWindow::maybeSave()
{
    if (!m_dirty) {
        return true;
    }

    const QMessageBox::StandardButton result = QMessageBox::warning(
        this,
        tr("Unsaved Changes"),
        tr("This todo list has unsaved changes. Save before continuing?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);

    if (result == QMessageBox::Cancel) {
        return false;
    }

    if (result == QMessageBox::Discard) {
        return true;
    }

    return save();
}

bool MainWindow::save()
{
    if (m_currentFile.isEmpty()) {
        return saveAs();
    }

    return saveToFile(m_currentFile);
}

bool MainWindow::saveAs()
{
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Save Todo List"),
        defaultSavePath(),
        tr("JSON Files (*.json);;All Files (*)"));

    if (filePath.isEmpty()) {
        return false;
    }

    return saveToFile(normalizedJsonSavePath(filePath));
}

bool MainWindow::saveToFile(const QString &path, bool showErrors)
{
    QJsonArray itemsArray;

    for (const TodoItemWidget *item : m_items) {
        QJsonObject entry;
        entry.insert(QStringLiteral("text"), item->text());
        entry.insert(QStringLiteral("completed"), item->isCompleted());
        itemsArray.append(entry);
    }

    QJsonObject root;
    root.insert(QStringLiteral("title"), m_titleLabel->value());
    root.insert(QStringLiteral("items"), itemsArray);
    root.insert(QStringLiteral("theme"), themeToJson());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (showErrors) {
            QMessageBox::critical(this, tr("Save Failed"), tr("Could not write %1").arg(path));
        }
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();

    m_currentFile = QFileInfo(path).absoluteFilePath();
    m_suggestedDirectory = QFileInfo(path).absolutePath();
    setDirty(false);
    updateStatusMessage(tr("Saved to %1").arg(m_currentFile));
    return true;
}

bool MainWindow::loadFromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("Open Failed"), tr("Could not open %1").arg(path));
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!document.isObject()) {
        QMessageBox::critical(this, tr("Invalid File"), tr("%1 is not a valid todo list file.").arg(path));
        return false;
    }

    const QJsonObject root = document.object();
    loadThemeFromJson(root.value(QStringLiteral("theme")).toObject());
    clearItems();
    {
        const QSignalBlocker blocker(m_titleLabel);
        m_titleLabel->setValue(root.value(QStringLiteral("title")).toString());
    }

    const QJsonArray itemsArray = root.value(QStringLiteral("items")).toArray();
    for (const QJsonValue &value : itemsArray) {
        const QJsonObject item = value.toObject();
        addItem(item.value(QStringLiteral("text")).toString(),
                item.value(QStringLiteral("completed")).toBool(),
                false);
    }

    if (m_items.isEmpty()) {
        addItem({}, false, false);
    }

    m_currentFile = QFileInfo(path).absoluteFilePath();
    m_suggestedDirectory = QFileInfo(path).absolutePath();
    setDirty(false);
    updateStatusMessage(tr("Opened %1").arg(m_currentFile));
    m_pendingResizeToFitItems = true;
    QTimer::singleShot(0, this, &MainWindow::resizeWindowToFitItems);
    return true;
}

void MainWindow::newList(bool promptToSave)
{
    if (promptToSave && !maybeSave()) {
        return;
    }

    clearItems();
    {
        const QSignalBlocker blocker(m_titleLabel);
        m_titleLabel->setValue({});
    }
    applyTheme();
    m_currentFile.clear();
    addItem({}, false, false);
    setDirty(false);
    updateStatusMessage(tr("New list"));
}

void MainWindow::openList()
{
    if (!maybeSave()) {
        return;
    }

    const QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open Todo List"),
        defaultDirectory(),
        tr("JSON Files (*.json *.todo.json);;All Files (*)"));

    if (filePath.isEmpty()) {
        return;
    }

    loadFromFile(filePath);
}

void MainWindow::uncheckAll()
{
    bool changed = false;
    for (TodoItemWidget *item : m_items) {
        if (!item->isCompleted()) {
            continue;
        }

        item->setCompleted(false);
        changed = true;
    }

    if (changed) {
        markDirty();
    }
}

void MainWindow::removeChecked()
{
    QVector<TodoItemWidget *> completedItems;
    for (TodoItemWidget *item : m_items) {
        if (item->isCompleted()) {
            completedItems.append(item);
        }
    }

    for (TodoItemWidget *item : completedItems) {
        removeItem(item);
    }
}

QString MainWindow::displayTitle() const
{
    const QString title = m_titleLabel->value().trimmed();
    if (!title.isEmpty()) {
        return title;
    }

    if (!m_currentFile.isEmpty()) {
        QString fileName = QFileInfo(m_currentFile).fileName();
        if (fileName.endsWith(QStringLiteral(".todo.json"))) {
            fileName.chop(10);
            return fileName;
        }

        return QFileInfo(m_currentFile).completeBaseName();
    }

    return QStringLiteral("Todo List");
}

QString MainWindow::defaultDirectory() const
{
    if (!m_currentFile.isEmpty()) {
        return QFileInfo(m_currentFile).absolutePath();
    }

    if (!m_suggestedDirectory.isEmpty()) {
        return m_suggestedDirectory;
    }

    const QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (!desktop.isEmpty()) {
        return desktop;
    }

    return QDir::homePath();
}

QString MainWindow::defaultSavePath() const
{
    const QString baseDirectory = defaultDirectory();
    const QString fileName = sanitizedFileName(m_titleLabel->value());
    return QDir(baseDirectory).filePath(fileName + QStringLiteral(".json"));
}
