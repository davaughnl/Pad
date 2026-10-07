// SPDX-License-Identifier: GPL-3.0-or-later
#include "padshell.h"
#include <QApplication>
#include <QAction>
#include <QComboBox>
#include <QFile>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QMap>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {
// Standard Qt dialogs must not fall back to colorful platform button icons.
class PadStyle : public QProxyStyle
{
public:
    PadStyle() : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion"))) {}
    QIcon standardIcon(StandardPixmap icon, const QStyleOption *option = nullptr,
                       const QWidget *widget = nullptr) const override
    {
        QString name;
        switch (icon) {
        case SP_DialogOkButton: case SP_DialogApplyButton: case SP_DialogYesButton: name = "check"; break;
        case SP_DialogCancelButton: case SP_DialogCloseButton: case SP_DialogNoButton: name = "close"; break;
        case SP_DialogSaveButton: name = "save"; break;
        case SP_DialogOpenButton: case SP_DirIcon: name = "folder"; break;
        case SP_DialogResetButton: case SP_DialogDiscardButton: name = "undo"; break;
        case SP_DialogHelpButton: case SP_MessageBoxInformation: case SP_MessageBoxQuestion: name = "info"; break;
        case SP_MessageBoxWarning: case SP_MessageBoxCritical: name = "bug"; break;
        case SP_TrashIcon: name = "trash"; break;
        default: return QProxyStyle::standardIcon(icon, option, widget);
        }
        return QIcon(QStringLiteral(":/pad/icons/%1.svg").arg(name));
    }
};
QLabel *text(const QString &value, const char *name, QWidget *parent)
{
    auto *label = new QLabel(value, parent);
    label->setObjectName(QString::fromLatin1(name));
    return label;
}
QPushButton *button(const QString &value, const char *icon, QWidget *parent)
{
    auto *result = new QPushButton(value, parent);
    result->setIcon(QIcon(QStringLiteral(":/pad/icons/%1.svg").arg(QString::fromLatin1(icon))));
    result->setIconSize(QSize(16, 16));
    return result;
}
// A generic controller outline, not a claim about the connected device's physical layout.
class ControllerOutline : public QWidget
{
public:
    explicit ControllerOutline(QWidget *parent) : QWidget(parent)
    {
        setMinimumSize(240, 126);
        setMaximumHeight(160);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        setAccessibleName(tr("Controller illustration"));
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal scale = qMin(width() / 360.0, height() / 190.0);
        p.translate((width() - 320 * scale) / 2, (height() - 185 * scale) / 2);
        p.scale(scale, scale);
        QPainterPath path;
        path.moveTo(72, 20);
        path.cubicTo(30, 15, 25, 45, 12, 115);
        path.cubicTo(0, 175, 28, 180, 66, 139);
        path.lineTo(88, 114); path.lineTo(232, 114); path.lineTo(254, 139);
        path.cubicTo(292, 180, 320, 175, 308, 115);
        path.cubicTo(295, 45, 290, 15, 248, 20); path.closeSubpath();
        p.setPen(QPen(QColor("#62626b"), 1.6)); p.setBrush(QColor("#222226"));
        p.drawPath(path); p.setBrush(QColor("#19191c"));
        for (const QPointF &point : {QPointF(114, 91), QPointF(204, 116)})
        {
            p.drawEllipse(point, 23, 23); p.drawEllipse(point, 16, 16);
        }
        p.drawRoundedRect(QRectF(55, 57, 16, 46), 3, 3);
        p.drawRoundedRect(QRectF(40, 72, 46, 16), 3, 3);
        for (const QPointF &point : {QPointF(256, 59), QPointF(280, 82), QPointF(256, 105), QPointF(232, 82)})
            p.drawEllipse(point, 10, 10);
        p.drawRoundedRect(QRectF(144, 59, 32, 10), 3, 3);
    }
};
void polishController(QWidget *page)
{
    if (!page || page->property("padPolished").toBool()) return;
    page->setProperty("padPolished", true);
    if (page->layout()) page->layout()->setContentsMargins(0, 0, 0, 0);
    const QMap<QString, QString> icons = {
        {"removeButton", "trash"}, {"loadButton", "folder"}, {"saveButton", "save"},
        {"saveAsButton", "copy"}, {"stickAssignPushButton", "sliders"},
        {"gameControllerMappingPushButton", "gamepad"}, {"namesPushButton", "text"},
        {"delayButton", "settings"}, {"resetButton", "undo"}};
    for (auto it = icons.constBegin(); it != icons.constEnd(); ++it)
        if (auto *b = page->findChild<QPushButton *>(it.key()))
        {
            b->setIcon(QIcon(QStringLiteral(":/pad/icons/%1.svg").arg(it.value())));
            b->setIconSize(QSize(16, 16));
        }
    if (auto *box = page->findChild<QComboBox *>("configBox")) box->setMinimumWidth(150);
}
}

void PadUi::initializeApplicationStyle()
{
    static bool initialized = false;
    if (initialized) return;
    initialized = true;
    qApp->setStyle(new PadStyle);
    qApp->setWindowIcon(QIcon(QStringLiteral(":/images/pad.png")));
    const int geistId = QFontDatabase::addApplicationFont(QStringLiteral(":/pad/Geist.ttf"));
    const QStringList families = QFontDatabase::applicationFontFamilies(geistId);
    QFont appFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    if (!families.isEmpty()) appFont.setFamily(families.first());
    appFont.setPointSize(10);
    appFont.setWeight(QFont::Normal);
    appFont.setBold(false);
    qApp->setFont(appFont);
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#111113"));
    palette.setColor(QPalette::WindowText, QColor("#e8e8eb"));
    palette.setColor(QPalette::Base, QColor("#19191c"));
    palette.setColor(QPalette::AlternateBase, QColor("#202024"));
    palette.setColor(QPalette::Text, QColor("#e8e8eb"));
    palette.setColor(QPalette::Button, QColor("#242428"));
    palette.setColor(QPalette::ButtonText, QColor("#e8e8eb"));
    palette.setColor(QPalette::Highlight, QColor("#484850"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#67676f"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#67676f"));
    qApp->setPalette(palette);
    QFile sheet(QStringLiteral(":/pad/pad.qss"));
    if (sheet.open(QIODevice::ReadOnly)) qApp->setStyleSheet(QString::fromUtf8(sheet.readAll()).replace(QStringLiteral("\"Geist\""), QStringLiteral("\"%1\"").arg(appFont.family())));
}

void PadUi::install(QMainWindow *window, QWidget *central, QStackedWidget *stack,
                    QTabWidget *controllers, QAction *refresh, QAction *settings)
{
    initializeApplicationStyle();
    window->setStyleSheet(QString()); // Remove the upstream blue/green widget overrides.
    window->setMinimumSize(940, 680);
    window->resize(1180, 800);
    auto *root = qobject_cast<QVBoxLayout *>(central->layout());
    if (!root) return;
    root->removeWidget(stack); root->setContentsMargins(0, 0, 0, 0); root->setSpacing(0);
    auto *shell = new QWidget(central);
    auto *layout = new QHBoxLayout(shell); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(0);
    auto *sidebar = new QFrame(shell); sidebar->setObjectName("padSidebar"); sidebar->setFixedWidth(224);
    auto *side = new QVBoxLayout(sidebar); side->setContentsMargins(16, 24, 16, 20); side->setSpacing(8);
    side->addWidget(text(QObject::tr("Pad"), "padBrand", sidebar)); side->addSpacing(28);
    side->addWidget(text(QObject::tr("Controllers"), "padSection", sidebar));
    auto *deviceList = new QListWidget(sidebar); deviceList->setObjectName("padControllers");
    deviceList->setMaximumHeight(110);
    deviceList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    deviceList->setTextElideMode(Qt::ElideRight); side->addWidget(deviceList);
    side->addSpacing(12); side->addWidget(text(QObject::tr("Recent profiles"), "padSection", sidebar));
    auto *profileList = new QListWidget(sidebar); profileList->setObjectName("padControllers");
    side->addWidget(profileList);
    side->addStretch(1);
    auto *refreshButton = button(QObject::tr("Refresh controllers"), "refresh", sidebar);
    auto *settingsButton = button(QObject::tr("Settings"), "settings", sidebar);
    side->addWidget(refreshButton); side->addWidget(settingsButton);
    QObject::connect(refreshButton, &QPushButton::clicked, refresh, &QAction::trigger);
    QObject::connect(settingsButton, &QPushButton::clicked, settings, &QAction::trigger);
    QObject::connect(deviceList, &QListWidget::currentRowChanged, controllers, &QTabWidget::setCurrentIndex);
    QObject::connect(profileList, &QListWidget::itemActivated, shell, [controllers, profileList](QListWidgetItem *item) {
        if (auto *page = controllers->currentWidget())
            if (auto *box = page->findChild<QComboBox *>("configBox"))
                box->setCurrentIndex(profileList->row(item));
    });
    layout->addWidget(sidebar);
    auto *workspace = new QWidget(shell); workspace->setObjectName("padWorkspace");
    auto *body = new QVBoxLayout(workspace); body->setContentsMargins(24, 22, 24, 16); body->setSpacing(16);
    body->addWidget(text(QObject::tr("Controller mapping"), "padTitle", workspace));
    body->addWidget(text(QObject::tr("Assign keyboard and mouse inputs to your controller."), "padSubtitle", workspace));
    auto *overview = new QFrame(workspace); overview->setObjectName("padOverview");
    auto *overviewLayout = new QHBoxLayout(overview); overviewLayout->setContentsMargins(20, 12, 20, 12);
    auto *details = new QVBoxLayout();
    auto *deviceName = text(QObject::tr("No controller connected"), "padDeviceName", overview);
    deviceName->setWordWrap(true); details->addWidget(deviceName);
    auto *profileName = text(QString(), "padSubtitle", overview); profileName->setWordWrap(true); details->addWidget(profileName);
    details->addStretch(); details->addWidget(text(QObject::tr("Keyboard + mouse"), "padSubtitle", overview));
    overviewLayout->addLayout(details, 1); overviewLayout->addWidget(new ControllerOutline(overview), 1);
    body->addWidget(overview); body->addWidget(stack, 1); layout->addWidget(workspace, 1);
    root->insertWidget(0, shell, 1);
    if (auto *bar = controllers->findChild<QTabBar *>()) bar->hide();
    if (auto *empty = stack->findChild<QLabel *>("label"))
    {
        empty->setObjectName("padEmpty");
        empty->setAlignment(Qt::AlignCenter);
        empty->setText(QObject::tr("Connect a controller to get started.\nThen choose Refresh controllers in the sidebar."));
    }
    auto sync = [controllers, deviceList, profileList, deviceName, profileName, overview, refresh, refreshButton]() {
        QStringList devices;
        for (int i = 0; i < controllers->count(); ++i)
        {
            devices.append(controllers->tabText(i)); polishController(controllers->widget(i));
        }
        QStringList previous;
        for (int i = 0; i < deviceList->count(); ++i) previous.append(deviceList->item(i)->text());
        QSignalBlocker blocker(deviceList);
        if (previous != devices) { deviceList->clear(); deviceList->addItems(devices); }
        deviceList->setFixedHeight(qMax(44, qMin(110, devices.size() * 42)));
        deviceList->setCurrentRow(controllers->currentIndex());
        QStringList profiles; QComboBox *box = nullptr;
        if (controllers->currentWidget()) box = controllers->currentWidget()->findChild<QComboBox *>("configBox");
        if (box) for (int i = 0; i < box->count(); ++i) profiles.append(box->itemText(i));
        QStringList oldProfiles;
        for (int i = 0; i < profileList->count(); ++i) oldProfiles.append(profileList->item(i)->text());
        if (oldProfiles != profiles) { profileList->clear(); profileList->addItems(profiles); }
        profileList->setCurrentRow(box ? box->currentIndex() : -1);
        profileList->setEnabled(box != nullptr);
        overview->setVisible(controllers->count() > 0);
        deviceName->setText(controllers->currentIndex() >= 0 ? controllers->tabText(controllers->currentIndex()) : QObject::tr("No controller connected"));
        profileName->setText(box ? box->currentText() : QString());
        refreshButton->setEnabled(refresh->isEnabled());
    };
    auto *timer = new QTimer(shell); timer->setInterval(400);
    QObject::connect(timer, &QTimer::timeout, shell, sync);
    QObject::connect(controllers, &QTabWidget::currentChanged, shell, [sync](int) { sync(); });
    sync(); timer->start();
}
