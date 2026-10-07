#ifndef WININTERCEPTIONEVENTHANDLER_H
#define WININTERCEPTIONEVENTHANDLER_H

#include "winsendinputeventhandler.h"

#include <pad/interception.h>

#include <memory>

/**
 * @brief Driver mode (experimental): mouse output goes through the Interception driver.
 *
 * Keyboard output and everything the driver cannot do stay on SendInput. If the driver is not
 * available the handler behaves exactly like SendInput and reports driverActive() == false.
 */
class WinInterceptionEventHandler : public WinSendInputEventHandler
{
    Q_OBJECT
  public:
    explicit WinInterceptionEventHandler(QObject *parent = nullptr);
    ~WinInterceptionEventHandler() override;

    bool init() override;
    bool cleanup() override;
    void sendMouseButtonEvent(JoyButtonSlot *slot, bool pressed) override;
    void sendMouseEvent(int xDis, int yDis) override;
    QString getName() override;
    QString getIdentifier() override; // "sendinput": keyboard mapping is shared with SendInput

    bool driverActive() const { return m_injector && m_injector->active(); }
    static QString driverLibraryPath();

  private:
    std::unique_ptr<PadInterception::MouseInjector> m_injector;
};

#endif
