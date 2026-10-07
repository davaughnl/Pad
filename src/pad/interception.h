/* Pad - driver mode: mouse output through the Interception filter driver.
 * Opt-in and experimental. Loaded at run time; nothing here runs unless the user turns driver mode on.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef PAD_INTERCEPTION_H
#define PAD_INTERCEPTION_H

#include <QString>

namespace PadInterception {

// Matches InterceptionMouseStroke from the Interception C API (interception.h).
struct MouseStroke
{
    unsigned short state;
    unsigned short flags;
    short rolling;
    int x;
    int y;
    unsigned int information;
};

enum MouseState : unsigned short
{
    LeftDown = 0x001, LeftUp = 0x002,
    RightDown = 0x004, RightUp = 0x008,
    MiddleDown = 0x010, MiddleUp = 0x020,
    Button4Down = 0x040, Button4Up = 0x080,
    Button5Down = 0x100, Button5Up = 0x200,
    Wheel = 0x400, HWheel = 0x800
};

constexpr int FirstMouseDevice = 11; // INTERCEPTION_MOUSE(0)
constexpr int MouseDeviceCount = 10;

// Function table for the functions Pad uses from interception.dll.
struct Api
{
    void *(*createContext)() = nullptr;
    void (*destroyContext)(void *) = nullptr;
    int (*send)(void *, int, const void *, unsigned int) = nullptr;
    unsigned int (*hardwareId)(void *, int, void *, unsigned int) = nullptr;
    bool valid() const { return createContext && destroyContext && send && hardwareId; }
};

// Loads interception.dll (or a test library) from the given path.
bool loadApi(const QString &libraryPath, Api *out, QString *error);

// Sends mouse strokes as the first real mouse the driver reports.
class MouseInjector
{
  public:
    explicit MouseInjector(const Api &api);
    ~MouseInjector();
    MouseInjector(const MouseInjector &) = delete;
    MouseInjector &operator=(const MouseInjector &) = delete;

    bool init(QString *error = nullptr);
    void shutdown();
    bool active() const { return m_context != nullptr; }
    int device() const { return m_device; }

    bool move(int dx, int dy);
    // Pad mouse slot codes: 1 left, 2 middle, 3 right, 4/5 wheel up/down, 6/7 horizontal wheel left/right, 8/9 X1/X2.
    bool button(int code, bool pressed);

  private:
    bool sendStroke(const MouseStroke &stroke);
    Api m_api;
    void *m_context = nullptr;
    int m_device = 0;
};

} // namespace PadInterception

#endif
