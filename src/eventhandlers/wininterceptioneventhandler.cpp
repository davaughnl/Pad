#include "wininterceptioneventhandler.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

WinInterceptionEventHandler::WinInterceptionEventHandler(QObject *parent) : WinSendInputEventHandler(parent) {}

WinInterceptionEventHandler::~WinInterceptionEventHandler() = default;

QString WinInterceptionEventHandler::driverLibraryPath()
{
    const QDir app(QCoreApplication::applicationDirPath());
    for (const QString &rel : {QStringLiteral("driver/interception.dll"), QStringLiteral("../driver/interception.dll"),
                               QStringLiteral("interception.dll")})
    {
        if (QFileInfo::exists(app.filePath(rel))) return QDir::cleanPath(app.filePath(rel));
    }
    return QString();
}

bool WinInterceptionEventHandler::init()
{
    WinSendInputEventHandler::init();
    const QString path = driverLibraryPath();
    PadInterception::Api api;
    QString error;
    if (path.isEmpty())
        error = QStringLiteral("Driver files are not installed with this copy of Pad.");
    else if (PadInterception::loadApi(path, &api, &error))
    {
        auto injector = std::make_unique<PadInterception::MouseInjector>(api);
        if (injector->init(&error)) m_injector = std::move(injector);
    }
    if (m_injector)
        qInfo() << "Driver mode active; mouse output uses the input driver (device" << m_injector->device() << ").";
    else
        qWarning() << "Driver mode unavailable, using standard input instead:" << error;
    return true; // Never block start-up: fall back to SendInput.
}

bool WinInterceptionEventHandler::cleanup()
{
    m_injector.reset();
    return WinSendInputEventHandler::cleanup();
}

void WinInterceptionEventHandler::sendMouseButtonEvent(JoyButtonSlot *slot, bool pressed)
{
    if (m_injector && m_injector->button(slot->getSlotCode(), pressed)) return;
    WinSendInputEventHandler::sendMouseButtonEvent(slot, pressed);
}

void WinInterceptionEventHandler::sendMouseEvent(int xDis, int yDis)
{
    if (m_injector && m_injector->move(xDis, yDis)) return;
    WinSendInputEventHandler::sendMouseEvent(xDis, yDis);
}

QString WinInterceptionEventHandler::getName() { return QStringLiteral("Driver mode (experimental)"); }

QString WinInterceptionEventHandler::getIdentifier() { return QStringLiteral("sendinput"); }
