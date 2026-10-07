// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PADUPDATESTRIP_H
#define PADUPDATESTRIP_H
#include <functional>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

// Presentation only. The updater logic owns the state and calls setState();
// this widget never starts, retries or applies anything itself. Retry calls onRetry.
class PadUpdateStrip : public QWidget
{
public:
    enum class State { Hidden, Checking, UpToDate, Available, Downloading, Verifying, Ready, Installing, Error };

    explicit PadUpdateStrip(QWidget *parent = nullptr) : QWidget(parent)
    {
        setObjectName("padUpdateStrip");
        auto *column = new QVBoxLayout(this); column->setContentsMargins(10, 4, 10, 4); column->setSpacing(6);
        auto *row = new QHBoxLayout(); row->setContentsMargins(0, 0, 0, 0); row->setSpacing(8);
        icon = new QLabel(this); icon->setFixedSize(16, 16); icon->setObjectName("padUpdateIcon");
        label = new QLabel(this); label->setObjectName("padUpdateText"); label->setWordWrap(true);
        row->addWidget(icon, 0, Qt::AlignTop); row->addWidget(label, 1);
        column->addLayout(row);
        progress = new QProgressBar(this); progress->setObjectName("padUpdateProgress");
        progress->setTextVisible(false); progress->setFixedHeight(3); progress->setRange(0, 100);
        column->addWidget(progress);
        retry = new QPushButton(QObject::tr("Retry"), this); retry->setObjectName("padUpdateRetry");
        retry->setFlat(true); retry->setCursor(Qt::PointingHandCursor);
        column->addWidget(retry, 0, Qt::AlignLeft);
        connect(retry, &QPushButton::clicked, this, [this]() { if (onRetry) onRetry(); });
        setState(State::Hidden);
    }

    // detail: version for Available, message for Error. percent: 0-100 for Downloading.
    void setState(State state, const QString &detail = QString(), int percent = 0)
    {
        current = state;
        setVisible(state != State::Hidden);
        const char *iconName = nullptr; QString text; bool bar = false, indeterminate = false, retryVisible = false;
        switch (state)
        {
        case State::Hidden: break;
        case State::Checking: text = QObject::tr("Checking for updates"); bar = indeterminate = true; break;
        case State::UpToDate: text = QObject::tr("Pad is up to date"); iconName = "circle-check"; break;
        case State::Available: text = QObject::tr("Update available: %1").arg(detail); iconName = "download"; break;
        case State::Downloading: text = QObject::tr("Downloading %1%").arg(qBound(0, percent, 100)); bar = true; break;
        case State::Verifying: text = QObject::tr("Verifying"); bar = indeterminate = true; break;
        case State::Ready: text = tr("Update ready: %1").arg(detail); iconName = "circle-check"; break;
        case State::Installing: text = QObject::tr("Installing, Pad will restart"); bar = indeterminate = true; break;
        case State::Error: text = detail; iconName = "triangle-alert"; retryVisible = true; break;
        }
        label->setText(text);
        icon->setPixmap(QPixmap()); // slot stays reserved so text aligns in every state
        if (iconName) icon->setPixmap(QIcon(QStringLiteral(":/pad/icons/%1.svg").arg(QString::fromLatin1(iconName))).pixmap(16, 16));
        progress->setVisible(bar);
        if (bar) { if (indeterminate) progress->setRange(0, 0); else { progress->setRange(0, 100); progress->setValue(qBound(0, percent, 100)); } }
        retry->setVisible(retryVisible);
    }
    State state() const { return current; }
    // Label for the sidebar button that sits above Settings.
    static QString buttonLabel(State state, const QString &version = QString())
    {
        return state == State::Available ? QObject::tr("Update to %1").arg(version) : QObject::tr("Check for updates");
    }

    std::function<void()> onRetry; // owner sets this; Retry only calls it

private:
    QLabel *icon = nullptr, *label = nullptr; QProgressBar *progress = nullptr; QPushButton *retry = nullptr;
    State current = State::Hidden;
};
#endif
