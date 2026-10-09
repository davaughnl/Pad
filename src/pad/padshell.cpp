// SPDX-License-Identifier: GPL-3.0-or-later
#include "padshell.h"
#include "common.h"
#include "padupdatestrip.h"
#include "updatemanager.h"
#include <QApplication>
#include <QAction>
#include <QComboBox>
#include <QFile>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QProcess>
#include <QListWidget>
#include <QMainWindow>
#include <QMap>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QProxyStyle>
#include <QStyle>
#include <QStyleFactory>
#include <QIcon>
#include <QPixmap>
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
    void polish(QWidget *widget) override
    {
        QProxyStyle::polish(widget);
        const QString sheet = widget->styleSheet();
        if (sheet.contains(QStringLiteral("\"Geist\"")) && qApp->font().family() != QStringLiteral("Geist"))
            widget->setStyleSheet(QString(sheet).replace(QStringLiteral("\"Geist\""),
                QStringLiteral("\"%1\"").arg(qApp->font().family())));
    }
    int styleHint(StyleHint hint, const QStyleOption *option = nullptr, const QWidget *widget = nullptr,
                  QStyleHintReturn *returnData = nullptr) const override
    {
        if (hint == SH_UnderlineShortcut) return 0; // No mnemonic underlines in the menu bar or buttons.
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
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
// Realistic render of the controller that is actually connected (Xbox or PlayStation).
class ControllerOutline : public QWidget
{
public:
    explicit ControllerOutline(QWidget *parent) : QWidget(parent)
    {
        setMinimumSize(280, 170);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setAccessibleName(tr("Controller illustration"));
        setDevice(QString());
    }
    void setDevice(const QString &name)
    {
        const QString n = name.toLower();
        const bool playstation = n.contains(QStringLiteral("ps4")) || n.contains(QStringLiteral("ps5")) || n.contains(QStringLiteral("dualshock"))
            || n.contains(QStringLiteral("dualsense")) || n.contains(QStringLiteral("playstation")) || n.contains(QStringLiteral("wireless controller"));
        art = QPixmap(playstation ? QStringLiteral(":/images/hero-ps4.png") : QStringLiteral(":/images/hero-xbox.png"));
        update();
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const QPixmap scaled = art.scaled(size() * devicePixelRatioF(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QPixmap out = scaled; out.setDevicePixelRatio(devicePixelRatioF());
        const QSizeF logical = QSizeF(scaled.size()) / devicePixelRatioF();
        p.drawPixmap(QPointF((width() - logical.width()) / 2, (height() - logical.height()) / 2), out);
    }
private:
    QPixmap art;
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
    palette.setColor(QPalette::Window, QColor("#0e0e10"));
    palette.setColor(QPalette::WindowText, QColor("#e8e8eb"));
    palette.setColor(QPalette::Base, QColor("#131316"));
    palette.setColor(QPalette::AlternateBase, QColor("#17171a"));
    palette.setColor(QPalette::Text, QColor("#e8e8eb"));
    palette.setColor(QPalette::Button, QColor("#1f1f24"));
    palette.setColor(QPalette::ButtonText, QColor("#e8e8eb"));
    palette.setColor(QPalette::Highlight, QColor("#333339"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Link, QColor("#e8e8eb"));
    palette.setColor(QPalette::LinkVisited, QColor("#9d9da5"));
    palette.setColor(QPalette::PlaceholderText, QColor("#66666e"));
    palette.setColor(QPalette::ToolTipBase, QColor("#26262c"));
    palette.setColor(QPalette::ToolTipText, QColor("#e8e8eb"));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#5c5c64"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#5c5c64"));
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
    auto *sidebar = new QFrame(shell); sidebar->setObjectName("padSidebar"); sidebar->setFixedWidth(232);
    auto *side = new QVBoxLayout(sidebar); side->setContentsMargins(12, 16, 12, 12); side->setSpacing(6);
    side->addSpacing(8);
    side->addWidget(text(QObject::tr("Controllers"), "padSection", sidebar));
    auto *deviceList = new QListWidget(sidebar); deviceList->setObjectName("padControllers");
    deviceList->setMaximumHeight(102);
    deviceList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    deviceList->setTextElideMode(Qt::ElideRight); side->addWidget(deviceList);
    side->addSpacing(16); side->addWidget(text(QObject::tr("Recent profiles"), "padSection", sidebar));
    auto *profileList = new QListWidget(sidebar); profileList->setObjectName("padControllers");
    side->addWidget(profileList);
    side->addStretch(1);
    auto *refreshButton = button(QObject::tr("Refresh controllers"), "refresh", sidebar);
    auto *settingsButton = button(QObject::tr("Settings"), "settings", sidebar);
    auto *updateStrip = new PadUpdateStrip(sidebar);
    auto *updateButton = button(PadUpdateStrip::buttonLabel(PadUpdateStrip::State::Hidden), "refresh", sidebar);
    for (auto *nav : {refreshButton, updateButton, settingsButton}) nav->setProperty("padNav", true);
    side->addWidget(updateStrip); side->addWidget(refreshButton); side->addWidget(updateButton); side->addWidget(settingsButton);
    QObject::connect(refreshButton, &QPushButton::clicked, refresh, &QAction::trigger);
    // Updater: this block only presents UpdateManager state and forwards clicks.
    auto *updates = new UpdateManager(PadderCommon::releaseVersion, window);
    updates->setInstallerLauncher([](const QString &path) { return QProcess::startDetached(path, QStringList()); });
    auto *updateAction = new QAction(QObject::tr("Check for updates"), window);
    if (auto *appMenu = window->findChild<QMenu *>("menuQuit"))
    {
        auto *first = appMenu->actions().value(0);
        appMenu->insertAction(first, updateAction); appMenu->insertSeparator(first);
    }
    auto present = [updates, updateStrip, updateButton, updateAction](int percent) {
        using S = PadUpdateStrip::State;
        const QString version = updates->release().version;
        QString label = QObject::tr("Check for updates"); const char *icon = "refresh"; bool enabled = true;
        switch (updates->state())
        {
        case UpdateManager::Idle: updateStrip->setState(S::Hidden); break;
        case UpdateManager::Checking: updateStrip->setState(S::Checking); enabled = false; break;
        case UpdateManager::UpToDate: updateStrip->setState(S::UpToDate); break;
        case UpdateManager::Available: updateStrip->setState(S::Available, version); label = QObject::tr("Update to %1").arg(version); icon = "download"; break;
        case UpdateManager::Downloading: updateStrip->setState(S::Downloading, QString(), percent); label = QObject::tr("Update to %1").arg(version); icon = "download"; enabled = false; break;
        case UpdateManager::Verifying: updateStrip->setState(S::Verifying); label = QObject::tr("Update to %1").arg(version); icon = "download"; enabled = false; break;
        case UpdateManager::Ready: QTimer::singleShot(0, updates, [updates]() { if (updates->state() == UpdateManager::Ready) updates->install(); }); updateStrip->setState(S::Ready, version); label = QObject::tr("Install and restart"); icon = "download"; break;
        case UpdateManager::Installing: updateStrip->setState(S::Installing); label = QObject::tr("Install and restart"); icon = "download"; enabled = false; break;
        case UpdateManager::Failed: updateStrip->setState(S::Error, updates->errorText()); break;
        }
        updateButton->setText(label); updateButton->setEnabled(enabled);
        updateButton->setIcon(QIcon(QStringLiteral(":/pad/icons/%1.svg").arg(QString::fromLatin1(icon))));
        updateAction->setEnabled(enabled && updates->state() != UpdateManager::Ready);
    };
    QObject::connect(updates, &UpdateManager::stateChanged, window, [present](UpdateManager::State) { present(0); });
    QObject::connect(updates, &UpdateManager::progress, window, [present](int percent) { present(percent); });
    QObject::connect(updates, &UpdateManager::quitRequested, qApp, &QApplication::quit);
    auto act = [updates]() {
        switch (updates->state())
        {
        case UpdateManager::Available: updates->download(); break;
        case UpdateManager::Ready: updates->install(); break;
        case UpdateManager::Idle: case UpdateManager::UpToDate: case UpdateManager::Failed: updates->check(); break;
        default: break;
        }
    };
    QObject::connect(updateButton, &QPushButton::clicked, window, act);
    QObject::connect(updateAction, &QAction::triggered, window, [updates]() {
        if (updates->state() != UpdateManager::Checking && updates->state() != UpdateManager::Downloading
            && updates->state() != UpdateManager::Verifying && updates->state() != UpdateManager::Installing) updates->check();
    });
    updateStrip->onRetry = [updates]() { updates->check(); };
    present(0);
    QObject::connect(settingsButton, &QPushButton::clicked, settings, &QAction::trigger);
    QObject::connect(deviceList, &QListWidget::currentRowChanged, controllers, &QTabWidget::setCurrentIndex);
    QObject::connect(profileList, &QListWidget::itemActivated, shell, [controllers, profileList](QListWidgetItem *item) {
        if (auto *page = controllers->currentWidget())
            if (auto *box = page->findChild<QComboBox *>("configBox"))
                box->setCurrentIndex(profileList->row(item));
    });
    layout->addWidget(sidebar);
    auto *workspace = new QWidget(shell); workspace->setObjectName("padWorkspace");
    auto *body = new QVBoxLayout(workspace); body->setContentsMargins(28, 24, 28, 16); body->setSpacing(8);
    auto *hero = new QHBoxLayout(); hero->setContentsMargins(0, 0, 0, 0);
    auto *heroText = new QVBoxLayout(); heroText->setSpacing(8); heroText->setAlignment(Qt::AlignVCenter);
    heroText->addWidget(text(QObject::tr("Controller mapping"), "padHeadline", workspace));
    heroText->addWidget(text(QObject::tr("Assign keyboard and mouse inputs to your controller."), "padSubtitle", workspace));
    hero->addLayout(heroText, 1);
    auto *outline = new ControllerOutline(workspace); outline->setFixedSize(360, 214);
    hero->addWidget(outline, 0, Qt::AlignRight | Qt::AlignVCenter);
    body->addLayout(hero);
    auto *overview = new QFrame(workspace); overview->setObjectName("padOverview");
    auto *overviewLayout = new QHBoxLayout(overview); overviewLayout->setContentsMargins(24, 14, 24, 14);
    auto *details = new QVBoxLayout(); details->setSpacing(2); details->setAlignment(Qt::AlignVCenter);
    auto *deviceName = text(QObject::tr("No controller connected"), "padDeviceName", overview);
    deviceName->setWordWrap(true); details->addWidget(deviceName);
    auto *profileName = text(QString(), "padSubtitle", overview); profileName->setWordWrap(true); details->addWidget(profileName);
    overviewLayout->addLayout(details, 1);
    overviewLayout->addWidget(text(QObject::tr("Keyboard + mouse"), "padSubtitle", overview), 0, Qt::AlignRight | Qt::AlignVCenter);
    body->addWidget(overview); body->addWidget(stack, 1); layout->addWidget(workspace, 1);
    root->insertWidget(0, shell, 1);
    if (auto *bar = window->menuBar())
    {
        auto *logo = new QLabel(bar); QPixmap mark(QStringLiteral(":/images/pad-mark.png")); logo->setPixmap(mark.scaled(22, 22, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        logo->setContentsMargins(12, 0, 6, 0); bar->setCornerWidget(logo, Qt::TopLeftCorner);
        auto *status = new QLabel(bar); status->setObjectName("padConnection"); status->setContentsMargins(0, 0, 14, 0);
        bar->setCornerWidget(status, Qt::TopRightCorner); logo->show(); status->show();
        auto *tick = new QTimer(status); tick->setInterval(400);
        QObject::connect(tick, &QTimer::timeout, status, [status, controllers]() {
            const bool on = controllers->count() > 0;
            status->setText(on ? QObject::tr("\u25CF  Connected") : QObject::tr("\u25CF  No controller"));
            status->setProperty("connected", on); status->style()->unpolish(status); status->style()->polish(status);
        });
        tick->start(); emit tick->timeout();
    }
    if (auto *bar = controllers->findChild<QTabBar *>()) bar->hide();
    if (auto *empty = stack->findChild<QLabel *>("label"))
    {
        empty->setObjectName("padEmpty");
        empty->setAlignment(Qt::AlignCenter);
        empty->setText(QObject::tr("Connect a controller to get started.\nThen choose Refresh controllers in the sidebar."));
    }
    auto sync = [controllers, deviceList, profileList, deviceName, profileName, overview, refresh, refreshButton, outline]() {
        QStringList devices;
        for (int i = 0; i < controllers->count(); ++i)
        {
            devices.append(controllers->tabText(i)); polishController(controllers->widget(i));
        }
        QStringList previous;
        for (int i = 0; i < deviceList->count(); ++i) previous.append(deviceList->item(i)->text());
        QSignalBlocker blocker(deviceList);
        if (previous != devices) { deviceList->clear(); deviceList->addItems(devices); }
        deviceList->setFixedHeight(qMax(36, qMin(102, devices.size() * 34 + 2)));
        deviceList->setCurrentRow(controllers->currentIndex());
        QStringList profiles; QComboBox *box = nullptr;
        if (controllers->currentWidget()) box = controllers->currentWidget()->findChild<QComboBox *>("configBox");
        if (box) for (int i = 0; i < box->count(); ++i) profiles.append(box->itemText(i));
        QStringList oldProfiles;
        for (int i = 0; i < profileList->count(); ++i) oldProfiles.append(profileList->item(i)->text());
        if (oldProfiles != profiles) { profileList->clear(); profileList->addItems(profiles); }
        profileList->setCurrentRow(box ? box->currentIndex() : -1);
        profileList->setEnabled(box != nullptr);
        overview->setVisible(controllers->count() > 0); outline->setVisible(controllers->count() > 0);
        deviceName->setText(controllers->currentIndex() >= 0 ? controllers->tabText(controllers->currentIndex()) : QObject::tr("No controller connected"));
        profileName->setText(box ? box->currentText() : QString());
        outline->setDevice(controllers->currentIndex() >= 0 ? controllers->tabText(controllers->currentIndex()) : QString());
        refreshButton->setEnabled(refresh->isEnabled());
    };
    auto *timer = new QTimer(shell); timer->setInterval(400);
    QObject::connect(timer, &QTimer::timeout, shell, sync);
    QObject::connect(controllers, &QTabWidget::currentChanged, shell, [sync](int) { sync(); });
    sync(); timer->start();
}
