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
    void polish(QWidget *widget) override
    {
        QProxyStyle::polish(widget);
        const QString sheet = widget->styleSheet();
        if (sheet.contains(QStringLiteral("\"Geist\"")) && qApp->font().family() != QStringLiteral("Geist"))
            widget->setStyleSheet(QString(sheet).replace(QStringLiteral("\"Geist\""),
                QStringLiteral("\"%1\"").arg(qApp->font().family())));
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
// A generic modern controller, not the connected device's physical layout.
class ControllerOutline : public QWidget
{
public:
    explicit ControllerOutline(QWidget *parent) : QWidget(parent)
    {
        setMinimumSize(190, 100);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setAccessibleName(tr("Controller illustration"));
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal scale = qMin(width() / 332.0, height() / 178.0);
        p.translate((width() - 332 * scale) / 2, (height() - 178 * scale) / 2);
        p.scale(scale, scale);
        p.translate(-14, -20);
        // The floating body has broad shoulders and tapering, sculpted grips.
        QPainterPath body;
        body.moveTo(91, 31);
        body.cubicTo(68, 29, 48, 41, 40, 68);
        body.cubicTo(27, 102, 19, 149, 31, 177);
        body.cubicTo(39, 197, 59, 192, 73, 173);
        body.cubicTo(91, 147, 99, 138, 118, 137);
        body.cubicTo(151, 133, 209, 133, 242, 137);
        body.cubicTo(261, 138, 269, 147, 287, 173);
        body.cubicTo(301, 192, 321, 197, 329, 177);
        body.cubicTo(341, 149, 333, 102, 320, 68);
        body.cubicTo(312, 41, 292, 29, 269, 31);
        body.cubicTo(231, 36, 129, 36, 91, 31);
        body.closeSubpath();
        QLinearGradient shoulder(0, 20, 0, 60);
        shoulder.setColorAt(0, QColor("#626262")); shoulder.setColorAt(1, QColor("#222222"));
        p.setBrush(shoulder); p.setPen(QPen(QColor("#727272"), 0.8));
        p.drawRoundedRect(QRectF(65, 22, 64, 22), 8, 8);
        p.drawRoundedRect(QRectF(231, 22, 64, 22), 8, 8);
        QLinearGradient shell(75, 30, 195, 187);
        shell.setColorAt(0, QColor("#4c4c4c")); shell.setColorAt(0.42, QColor("#2c2c2c"));
        shell.setColorAt(1, QColor("#171717"));
        p.setBrush(shell); p.setPen(QPen(QColor("#737373"), 1.2)); p.drawPath(body);
        p.save(); p.setClipPath(body);
        QLinearGradient rim(0, 30, 0, 112);
        rim.setColorAt(0, QColor(255, 255, 255, 100)); rim.setColorAt(1, QColor(255, 255, 255, 0));
        p.setBrush(Qt::NoBrush); p.setPen(QPen(rim, 3)); p.drawPath(body);
        // Grip seams disappear into the underside, not an outlined cartoon.
        p.setPen(QPen(QColor(255, 255, 255, 22), 1));
        p.drawLine(QPointF(79, 116), QPointF(46, 179));
        p.drawLine(QPointF(281, 116), QPointF(314, 179));
        p.restore();
        QLinearGradient panel(0, 47, 0, 90);
        panel.setColorAt(0, QColor("#1d1d1d")); panel.setColorAt(1, QColor("#292929"));
        p.setBrush(panel); p.setPen(QPen(QColor("#515151"), 0.8));
        p.drawRoundedRect(QRectF(135, 48, 90, 43), 10, 10);
        p.setPen(QPen(QColor(255, 255, 255, 25), 5, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(148, 43), QPointF(212, 43));
        p.setPen(QPen(QColor("#d2d2d2"), 1.8, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(148, 43), QPointF(212, 43));
        // Recessed symmetric sticks with matte caps and concentric rims.
        for (const QPointF &c : {QPointF(123, 119), QPointF(237, 119)}) {
            p.setPen(QPen(QColor("#474747"), 1)); p.setBrush(QColor("#131313")); p.drawEllipse(c, 25, 25);
            QRadialGradient well(c - QPointF(5, 6), 24);
            well.setColorAt(0, QColor("#555555")); well.setColorAt(1, QColor("#232323"));
            p.setBrush(well); p.setPen(QPen(QColor("#686868"), 0.8)); p.drawEllipse(c, 19, 19);
            p.setBrush(QColor("#252525")); p.setPen(QPen(QColor("#424242"), 0.8)); p.drawEllipse(c, 15, 15);
        }
        QPainterPath cross;
        cross.moveTo(76, 58); cross.lineTo(89, 58); cross.lineTo(89, 72);
        cross.lineTo(103, 72); cross.lineTo(103, 85); cross.lineTo(89, 85);
        cross.lineTo(89, 99); cross.lineTo(76, 99); cross.lineTo(76, 85);
        cross.lineTo(62, 85); cross.lineTo(62, 72); cross.lineTo(76, 72); cross.closeSubpath();
        p.setBrush(QColor("#151515")); p.setPen(QPen(QColor("#6a6a6a"), 1)); p.drawPath(cross);
        p.setPen(QPen(QColor("#343434"), 1)); p.drawLine(QPointF(78, 79), QPointF(87, 79));
        for (const QPointF &c : {QPointF(278, 60), QPointF(295, 78), QPointF(278, 96), QPointF(261, 78)}) {
            p.setPen(Qt::NoPen); p.setBrush(QColor(255, 255, 255, 12)); p.drawEllipse(c, 12, 12);
            QLinearGradient face(c - QPointF(0, 8), c + QPointF(0, 8));
            face.setColorAt(0, QColor("#5a5a5a")); face.setColorAt(1, QColor("#282828"));
            p.setBrush(face); p.setPen(QPen(QColor("#818181"), 0.8)); p.drawEllipse(c, 8.5, 8.5);
        }
        p.setBrush(QColor("#171717")); p.setPen(QPen(QColor("#616161"), 0.8));
        p.drawRoundedRect(QRectF(113, 61, 12, 7), 3, 3);
        p.drawRoundedRect(QRectF(235, 61, 12, 7), 3, 3);
        p.drawEllipse(QPointF(180, 112), 7, 7);
        p.setPen(QPen(QColor("#bcbcbc"), 1)); p.drawLine(QPointF(177, 112), QPointF(183, 112));
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
    auto *brandRow = new QHBoxLayout(); brandRow->setContentsMargins(10, 4, 0, 4); brandRow->setSpacing(10);
    auto *brandMark = new QLabel(sidebar);
    brandMark->setPixmap(QIcon(QStringLiteral(":/images/pad.png")).pixmap(18, 18));
    brandRow->addWidget(brandMark); brandRow->addWidget(text(QObject::tr("Pad"), "padBrand", sidebar)); brandRow->addStretch(1);
    side->addLayout(brandRow); side->addSpacing(20);
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
    auto *body = new QVBoxLayout(workspace); body->setContentsMargins(28, 24, 28, 16); body->setSpacing(8);
    body->addWidget(text(QObject::tr("Controller mapping"), "padTitle", workspace));
    body->addWidget(text(QObject::tr("Assign keyboard and mouse inputs to your controller."), "padSubtitle", workspace));
    body->addSpacing(8);
    auto *overview = new QFrame(workspace); overview->setObjectName("padOverview");
    auto *overviewLayout = new QHBoxLayout(overview); overviewLayout->setContentsMargins(20, 10, 20, 10);
    auto *details = new QVBoxLayout(); details->setSpacing(2); details->setAlignment(Qt::AlignVCenter);
    auto *deviceName = text(QObject::tr("No controller connected"), "padDeviceName", overview);
    deviceName->setWordWrap(true); details->addWidget(deviceName);
    auto *profileName = text(QString(), "padSubtitle", overview); profileName->setWordWrap(true); details->addWidget(profileName);
    details->addSpacing(10); details->addWidget(text(QObject::tr("Keyboard + mouse"), "padSubtitle", overview));
    overviewLayout->addLayout(details, 1);
    auto *outline = new ControllerOutline(overview); outline->setFixedSize(190, 100);
    overviewLayout->addWidget(outline, 0, Qt::AlignRight | Qt::AlignVCenter);
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
