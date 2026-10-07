// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PADDRIVERSETTINGS_H
#define PADDRIVERSETTINGS_H
#include <functional>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include "drivermode.h"

// Settings section for the experimental driver mode. Presentation and thin glue:
// it reads PadDriverMode::status(), calls PadDriverMode::startInstall() and
// exposes the toggle; the dialog writes PadDriverMode::SettingsKey on accept.
namespace PadUi {

// Test seams only. Production leaves both empty.
inline std::function<PadDriverMode::Status(QString *)> &driverStatusSource()
{
    static std::function<PadDriverMode::Status(QString *)> source;
    return source;
}
inline std::function<bool(bool, QString *)> &driverInstallSource()
{
    static std::function<bool(bool, QString *)> source;
    return source;
}

class DriverModeSection : public QWidget
{
public:
    explicit DriverModeSection(bool enabled, QWidget *parent = nullptr) : QWidget(parent)
    {
        setObjectName("padDriverSection");
        auto *column = new QVBoxLayout(this); column->setContentsMargins(0, 0, 0, 0); column->setSpacing(8);
        auto *title = new QLabel(QObject::tr("Experimental: driver mode"), this); title->setObjectName("padSection");
        column->addWidget(title);
        toggle = new QCheckBox(QObject::tr("Send mouse input through the driver"), this);
        toggle->setObjectName("driverModeCheckBox"); toggle->setChecked(enabled);
        column->addWidget(toggle);
        restartLine = new QLabel(QObject::tr("Restart Pad to apply."), this);
        restartLine->setObjectName("padDriverRestart"); restartLine->setVisible(false); column->addWidget(restartLine);
        QObject::connect(toggle, &QCheckBox::toggled, this, [this, enabled](bool on) { restartLine->setVisible(on != enabled); });
        risk = new QLabel(QObject::tr("Driver mode is experimental. It sends mouse input through a system driver so games that ignore normal input can see it. Your anti-cheat may block your mouse and keyboard or flag your account. Requires administrator approval and a restart. Off by default."), this);
        risk->setObjectName("padDriverRisk"); risk->setWordWrap(true); column->addWidget(risk);
        status = new QLabel(this); status->setObjectName("padDriverStatus"); status->setWordWrap(true); column->addWidget(status);
        button = new QPushButton(this); button->setObjectName("driverInstallButton"); button->setFixedHeight(32);
        column->addWidget(button, 0, Qt::AlignLeft);
        QObject::connect(button, &QPushButton::clicked, this, [this]() { runInstall(); });
        refresh();
    }
    bool isChecked() const { return toggle->isChecked(); }
    bool supported() const { return currentStatus() != PadDriverMode::Status::Unsupported; }

private:
    PadDriverMode::Status currentStatus(QString *detail = nullptr) const
    {
        auto &source = driverStatusSource();
        return source ? source(detail) : PadDriverMode::status(detail);
    }
    void refresh()
    {
        using S = PadDriverMode::Status;
        const S state = currentStatus();
        if (state == S::Unsupported) { hide(); return; }
        QString text; QString action; bool install = true;
        switch (state)
        {
        case S::NotBundled: text = QObject::tr("Driver files are not included in this copy of Pad."); break;
        case S::NotInstalled: text = QObject::tr("Driver not installed."); action = QObject::tr("Install driver"); break;
        case S::Ready: text = QObject::tr("Driver installed and running."); action = QObject::tr("Remove driver"); install = false; break;
        case S::Unsupported: break;
        }
        installing = install;
        if (restartNeeded) text = restartText;
        status->setText(text);
        // The toggle only works once the driver answers. Until then it stays off and says why.
        const bool usable = state == S::Ready && !restartNeeded;
        toggle->setEnabled(usable);
        toggle->setToolTip(usable ? QString() : QObject::tr("Available after the driver is installed and Windows restarts."));
        button->setText(action); button->setVisible(!action.isEmpty() && !restartNeeded);
    }
    void runInstall()
    {
        QString error; auto &source = driverInstallSource();
        const bool ok = source ? source(installing, &error) : PadDriverMode::startInstall(installing, &error);
        if (ok)
        {
            restartNeeded = true;
            restartText = installing ? QObject::tr("Driver installed. Restart Windows to finish.") : QObject::tr("Driver removed. Restart Windows to finish.");
        }
        else
        {
            restartNeeded = false;
            status->setText(error.isEmpty() ? QObject::tr("The driver change did not complete.") : error);
            return;
        }
        refresh();
    }
    QCheckBox *toggle = nullptr; QLabel *risk = nullptr, *restartLine = nullptr, *status = nullptr; QPushButton *button = nullptr;
    bool installing = true, restartNeeded = false; QString restartText;
};
}
#endif
