// SPDX-License-Identifier: GPL-3.0-or-later
// Presentation only: battery, gyro aim and stick test controls for the overview bar,
// the stick tester dialog, and the profile-switch toast. Logic stays with the callers.
#ifndef PADDEVICETOOLS_H
#define PADDEVICETOOLS_H
#include <functional>
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QtMath>

class PadDeviceTools : public QWidget
{
public:
    enum class Battery { None, Empty, Low, Medium, Full };
    std::function<void(bool)> onGyroToggled;
    std::function<void()> onGyroSettings;
    std::function<void()> onTestSticks;

    explicit PadDeviceTools(QWidget *parent = nullptr) : QWidget(parent)
    {
        setObjectName("padDeviceTools");
        auto *row = new QHBoxLayout(this); row->setContentsMargins(0, 0, 0, 0); row->setSpacing(8);
        batteryIcon = new QLabel(this); batteryIcon->setFixedSize(16, 16);
        batteryText = new QLabel(this); batteryText->setObjectName("padSubtitle");
        gyroSettings = new QPushButton(this); gyroSettings->setObjectName("padGyroSettings");
        gyroSettings->setIcon(QIcon(QStringLiteral(":/pad/icons/sliders.svg"))); gyroSettings->setIconSize(QSize(14, 14));
        gyroSettings->setToolTip(tr("Gyro sensitivity")); gyroSettings->setFixedWidth(30);
        gyro = new QPushButton(tr("Gyro aim"), this); gyro->setObjectName("padGyroAim"); gyro->setCheckable(true);
        gyro->setProperty("padToggle", true);
        gyro->setIcon(QIcon(QStringLiteral(":/pad/icons/rotate-3d.svg"))); gyro->setIconSize(QSize(14, 14));
        gyro->setToolTip(tr("Move the mouse by tilting the controller"));
        test = new QPushButton(tr("Test sticks"), this); test->setObjectName("padTestSticks");
        test->setIcon(QIcon(QStringLiteral(":/pad/icons/crosshair.svg"))); test->setIconSize(QSize(14, 14));
        test->setToolTip(tr("See live stick values and the deadzone"));
        row->addWidget(batteryIcon); row->addWidget(batteryText); row->addSpacing(4);
        row->addWidget(gyroSettings); row->addWidget(gyro); row->addWidget(test);
        QObject::connect(gyro, &QPushButton::clicked, this, [this](bool on) { gyroSettings->setVisible(on); if (onGyroToggled) onGyroToggled(on); });
        QObject::connect(gyroSettings, &QPushButton::clicked, this, [this]() { if (onGyroSettings) onGyroSettings(); });
        QObject::connect(test, &QPushButton::clicked, this, [this]() { if (onTestSticks) onTestSticks(); });
        setBattery(Battery::None); setGyro(false, false);
    }
    void setBattery(Battery level)
    {
        const bool shown = level != Battery::None;
        batteryIcon->setVisible(shown); batteryText->setVisible(shown);
        if (!shown) return;
        const char *icon = "battery-full"; QString label = tr("Full");
        switch (level)
        {
        case Battery::Empty: icon = "battery-empty"; label = tr("Empty"); break;
        case Battery::Low: icon = "battery-low"; label = tr("Low"); break;
        case Battery::Medium: icon = "battery-good"; label = tr("Medium"); break;
        default: break;
        }
        batteryIcon->setPixmap(QIcon(QStringLiteral(":/pad/icons/%1.svg").arg(QString::fromLatin1(icon))).pixmap(16, 16));
        batteryText->setText(tr("Battery %1").arg(label.toLower()));
        batteryIcon->setToolTip(tr("Controller battery"));
    }
    void setGyro(bool available, bool on)
    {
        gyro->setVisible(available);
        gyro->setChecked(on);
        gyroSettings->setVisible(available && on);
    }
private:
    QLabel *batteryIcon, *batteryText;
    QPushButton *gyro, *gyroSettings, *test;
};

struct PadStickReading { double lx = 0, ly = 0, rx = 0, ry = 0, leftDeadzone = 0, rightDeadzone = 0; };

// One stick plot: crosshair, deadzone disc, live position.
class PadStickPlot : public QWidget
{
public:
    explicit PadStickPlot(QWidget *parent = nullptr) : QWidget(parent) { setFixedSize(220, 220); }
    void setValue(double x, double y, double deadzone) { px = x; py = y; dz = deadzone; update(); }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        const QPointF c(width() / 2.0, height() / 2.0); const double r = width() / 2.0 - 6;
        p.setPen(QPen(QColor("#1c1c20"), 1)); p.setBrush(QColor("#101013")); p.drawEllipse(c, r, r);
        p.drawLine(QPointF(c.x() - r, c.y()), QPointF(c.x() + r, c.y()));
        p.drawLine(QPointF(c.x(), c.y() - r), QPointF(c.x(), c.y() + r));
        p.setPen(QPen(QColor("#333339"), 1, Qt::DashLine)); p.setBrush(QColor("#1a1a1f"));
        p.drawEllipse(c, r * dz, r * dz);
        const double mag = qSqrt(px * px + py * py);
        const double sx = mag > 1 ? px / mag : px, sy = mag > 1 ? py / mag : py;
        const QPointF dot(c.x() + sx * r, c.y() + sy * r);
        const bool outside = mag > dz;
        p.setPen(Qt::NoPen); p.setBrush(outside ? QColor("#f4f4f5") : QColor("#66666e")); p.drawEllipse(dot, 5.5, 5.5);
    }
private:
    double px = 0, py = 0, dz = 0;
};

// Live stick tester. The reader returns normalised values (-1..1) and each stick's deadzone (0..1).
inline QDialog *padCreateStickTester(QWidget *parent, const QString &device, std::function<PadStickReading()> reader)
{
    auto *dialog = new QDialog(parent);
    dialog->setObjectName("padStickTester");
    dialog->setWindowTitle(QObject::tr("Test sticks"));
    dialog->setFixedSize(560, 420);
    auto *root = new QVBoxLayout(dialog); root->setContentsMargins(24, 20, 24, 20); root->setSpacing(12);
    auto *title = new QLabel(QObject::tr("Test sticks"), dialog); title->setStyleSheet("font-size: 20px; color: #f4f4f5;");
    auto *sub = new QLabel(QObject::tr("Move a stick. The dashed circle is the deadzone from your current profile; the dot turns bright once it is outside."), dialog);
    sub->setObjectName("padSubtitle"); sub->setWordWrap(true);
    root->addWidget(title); root->addWidget(sub);
    auto *plots = new QHBoxLayout(); plots->setSpacing(24);
    QPointer<PadStickPlot> plot[2]; QPointer<QLabel> value[2];
    const QString names[2] = {QObject::tr("Left stick"), QObject::tr("Right stick")};
    for (int i = 0; i < 2; ++i)
    {
        auto *col = new QVBoxLayout(); col->setSpacing(6); col->setAlignment(Qt::AlignHCenter);
        auto *head = new QLabel(names[i], dialog); head->setObjectName("padSection"); head->setAlignment(Qt::AlignHCenter);
        plot[i] = new PadStickPlot(dialog);
        value[i] = new QLabel(dialog); value[i]->setObjectName("padSubtitle"); value[i]->setAlignment(Qt::AlignHCenter);
        col->addWidget(head); col->addWidget(plot[i], 0, Qt::AlignHCenter); col->addWidget(value[i]);
        plots->addLayout(col);
    }
    root->addLayout(plots, 1);
    auto *foot = new QHBoxLayout();
    auto *name = new QLabel(device, dialog); name->setObjectName("padSubtitle"); foot->addWidget(name, 1);
    auto *close = new QPushButton(QObject::tr("Close"), dialog);
    close->setIcon(QIcon(QStringLiteral(":/pad/icons/close.svg"))); close->setIconSize(QSize(14, 14));
    QObject::connect(close, &QPushButton::clicked, dialog, &QDialog::accept);
    foot->addWidget(close); root->addLayout(foot);
    auto *tick = new QTimer(dialog); tick->setInterval(16);
    auto update = [=]() {
        const PadStickReading r = reader();
        const double xs[2] = {r.lx, r.rx}, ys[2] = {r.ly, r.ry}, dz[2] = {r.leftDeadzone, r.rightDeadzone};
        for (int i = 0; i < 2; ++i)
        {
            if (!plot[i]) return;
            plot[i]->setValue(xs[i], ys[i], dz[i]);
            value[i]->setText(QObject::tr("X %1   Y %2   Deadzone %3%")
                                  .arg(xs[i], 6, 'f', 2).arg(ys[i], 6, 'f', 2).arg(qRound(dz[i] * 100)));
        }
    };
    QObject::connect(tick, &QTimer::timeout, dialog, update);
    tick->start(); update();
    return dialog;
}

// Short confirmation near the bottom of the window. Replaces any toast already showing.
inline void padShowToast(QWidget *window, const QString &message, int milliseconds = 2200)
{
    if (!window) return;
    delete window->findChild<QFrame *>("padToast", Qt::FindDirectChildrenOnly);
    auto *toast = new QFrame(window); toast->setObjectName("padToast");
    auto *row = new QHBoxLayout(toast); row->setContentsMargins(12, 0, 14, 0); row->setSpacing(8);
    auto *icon = new QLabel(toast); icon->setPixmap(QIcon(QStringLiteral(":/pad/icons/circle-check.svg")).pixmap(16, 16));
    auto *label = new QLabel(message, toast); label->setObjectName("padToastText");
    row->addWidget(icon); row->addWidget(label);
    toast->adjustSize(); toast->setFixedHeight(36);
    toast->move(window->width() - toast->width() - 24, 52);
    toast->show(); toast->raise();
    QTimer::singleShot(milliseconds, toast, [toast]() { toast->hide(); toast->deleteLater(); });
}
#endif
