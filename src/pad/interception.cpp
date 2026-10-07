#include "interception.h"

#include <QLibrary>

namespace PadInterception {

bool loadApi(const QString &libraryPath, Api *out, QString *error)
{
    QLibrary lib(libraryPath);
    // Keep the library loaded for the life of the process.
    lib.setLoadHints(QLibrary::PreventUnloadHint);
    if (!lib.load())
    {
        if (error) *error = lib.errorString();
        return false;
    }
    Api api;
    api.createContext = reinterpret_cast<void *(*)()>(lib.resolve("interception_create_context"));
    api.destroyContext = reinterpret_cast<void (*)(void *)>(lib.resolve("interception_destroy_context"));
    api.send = reinterpret_cast<int (*)(void *, int, const void *, unsigned int)>(lib.resolve("interception_send"));
    api.hardwareId = reinterpret_cast<unsigned int (*)(void *, int, void *, unsigned int)>(lib.resolve("interception_get_hardware_id"));
    if (!api.valid())
    {
        if (error) *error = QStringLiteral("The driver library is missing required functions.");
        return false;
    }
    *out = api;
    return true;
}

MouseInjector::MouseInjector(const Api &api) : m_api(api) {}

MouseInjector::~MouseInjector() { shutdown(); }

bool MouseInjector::init(QString *error)
{
    shutdown();
    if (!m_api.valid())
    {
        if (error) *error = QStringLiteral("Driver library not loaded.");
        return false;
    }
    // Fails when the driver is not installed or the PC has not restarted since installing it.
    void *ctx = m_api.createContext();
    if (!ctx)
    {
        if (error) *error = QStringLiteral("The input driver is not running. Install it and restart the PC.");
        return false;
    }
    int found = 0;
    for (int i = 0; i < MouseDeviceCount && !found; ++i)
    {
        const int device = FirstMouseDevice + i;
        char id[512] = {};
        if (m_api.hardwareId(ctx, device, id, sizeof(id)) > 0) found = device;
    }
    if (!found)
    {
        m_api.destroyContext(ctx);
        if (error) *error = QStringLiteral("The driver did not report a physical mouse.");
        return false;
    }
    m_context = ctx;
    m_device = found;
    return true;
}

void MouseInjector::shutdown()
{
    if (m_context)
    {
        m_api.destroyContext(m_context);
        m_context = nullptr;
        m_device = 0;
    }
}

bool MouseInjector::sendStroke(const MouseStroke &stroke)
{
    if (!m_context) return false;
    return m_api.send(m_context, m_device, &stroke, 1) > 0;
}

bool MouseInjector::move(int dx, int dy)
{
    MouseStroke s = {};
    s.flags = 0; // INTERCEPTION_MOUSE_MOVE_RELATIVE
    s.x = dx;
    s.y = dy;
    return sendStroke(s);
}

bool MouseInjector::button(int code, bool pressed)
{
    MouseStroke s = {};
    switch (code)
    {
    case 1: s.state = pressed ? LeftDown : LeftUp; break;
    case 2: s.state = pressed ? MiddleDown : MiddleUp; break;
    case 3: s.state = pressed ? RightDown : RightUp; break;
    case 4: if (!pressed) return true; s.state = Wheel; s.rolling = 120; break;
    case 5: if (!pressed) return true; s.state = Wheel; s.rolling = -120; break;
    case 6: if (!pressed) return true; s.state = HWheel; s.rolling = -120; break;
    case 7: if (!pressed) return true; s.state = HWheel; s.rolling = 120; break;
    case 8: s.state = pressed ? Button4Down : Button4Up; break;
    case 9: s.state = pressed ? Button5Down : Button5Up; break;
    default: return false;
    }
    return sendStroke(s);
}

} // namespace PadInterception
