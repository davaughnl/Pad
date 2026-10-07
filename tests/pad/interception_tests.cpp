#include <pad/interception.h>

#include <QCoreApplication>
#include <QDebug>
#include <QVector>
#include <cstdlib>
#include <cstring>

using namespace PadInterception;

static QVector<MouseStroke> g_sent;
static QVector<int> g_devices;
static int g_created = 0, g_destroyed = 0;
static bool g_driverUp = true;
static int g_mouseSlot = 3; // which mouse slot "has" hardware
static int g_sendResult = 1;
static char g_ctxMarker;

static void *fakeCreate() { if (!g_driverUp) return nullptr; ++g_created; return &g_ctxMarker; }
static void fakeDestroy(void *c) { if (c == &g_ctxMarker) ++g_destroyed; }
static int fakeSend(void *c, int device, const void *s, unsigned n)
{
    if (c != &g_ctxMarker || n != 1) return 0;
    MouseStroke st; std::memcpy(&st, s, sizeof(st));
    g_sent.push_back(st); g_devices.push_back(device);
    return g_sendResult;
}
static unsigned fakeHwId(void *, int device, void *buf, unsigned size)
{
    if (device != FirstMouseDevice + g_mouseSlot) return 0;
    std::strncpy(static_cast<char *>(buf), "HID\\VID_046D&PID_C077", size);
    return 22;
}

static Api fakeApi() { Api a; a.createContext = fakeCreate; a.destroyContext = fakeDestroy; a.send = fakeSend; a.hardwareId = fakeHwId; return a; }

#define CHECK(c) do { if (!(c)) { qCritical() << "FAIL line" << __LINE__ << #c; return false; } } while (0)

static void reset() { g_sent.clear(); g_devices.clear(); g_created = g_destroyed = 0; g_driverUp = true; g_mouseSlot = 3; g_sendResult = 1; }

static bool layout()
{
    // Struct layout must match the C API (state, flags, rolling, x, y, information).
    CHECK(sizeof(MouseStroke) == 20);
    CHECK(offsetof(MouseStroke, x) == 8);
    CHECK(offsetof(MouseStroke, information) == 16);
    return true;
}

static bool pickAndMove()
{
    reset();
    MouseInjector inj(fakeApi());
    CHECK(!inj.active());
    CHECK(inj.init());
    CHECK(inj.active());
    CHECK(inj.device() == FirstMouseDevice + 3);
    CHECK(inj.move(7, -4));
    CHECK(g_sent.size() == 1 && g_sent[0].x == 7 && g_sent[0].y == -4 && g_sent[0].flags == 0 && g_sent[0].state == 0);
    CHECK(g_devices[0] == FirstMouseDevice + 3);
    return true;
}

static bool buttons()
{
    reset();
    MouseInjector inj(fakeApi());
    CHECK(inj.init());
    struct Case { int code; bool down; unsigned short state; short rolling; };
    const Case cases[] = {
        {1, true, LeftDown, 0}, {1, false, LeftUp, 0}, {3, true, RightDown, 0}, {3, false, RightUp, 0},
        {2, true, MiddleDown, 0}, {2, false, MiddleUp, 0}, {8, true, Button4Down, 0}, {9, false, Button5Up, 0},
        {4, true, Wheel, 120}, {5, true, Wheel, -120}, {6, true, HWheel, -120}, {7, true, HWheel, 120}};
    for (const Case &c : cases)
    {
        g_sent.clear();
        CHECK(inj.button(c.code, c.down));
        CHECK(g_sent.size() == 1 && g_sent[0].state == c.state && g_sent[0].rolling == c.rolling);
    }
    g_sent.clear();
    CHECK(inj.button(4, false)); // wheel release sends nothing
    CHECK(g_sent.isEmpty());
    CHECK(!inj.button(42, true)); // unknown code is rejected, not guessed
    return true;
}

static bool driverMissing()
{
    reset(); g_driverUp = false;
    MouseInjector inj(fakeApi());
    QString err;
    CHECK(!inj.init(&err));
    CHECK(!err.isEmpty());
    CHECK(!inj.active());
    CHECK(!inj.move(1, 1)); // caller falls back to SendInput
    CHECK(g_sent.isEmpty());
    return true;
}

static bool noMouse()
{
    reset(); g_mouseSlot = 99;
    MouseInjector inj(fakeApi());
    QString err;
    CHECK(!inj.init(&err));
    CHECK(g_created == 1 && g_destroyed == 1); // context released again
    return true;
}

static bool sendFailureReported()
{
    reset();
    MouseInjector inj(fakeApi());
    CHECK(inj.init());
    g_sendResult = 0;
    CHECK(!inj.move(1, 1)); // caller falls back
    CHECK(!inj.button(1, true));
    return true;
}

static bool lifecycle()
{
    reset();
    {
        MouseInjector inj(fakeApi());
        CHECK(inj.init());
        CHECK(inj.init()); // re-init releases the first context
        CHECK(g_created == 2 && g_destroyed == 1);
    }
    CHECK(g_destroyed == 2); // destructor releases
    Api bad; MouseInjector inj(bad);
    QString err;
    CHECK(!inj.init(&err));
    return true;
}

static bool badLibrary()
{
    Api a; QString err;
    CHECK(!loadApi(QStringLiteral("/nonexistent/interception.dll"), &a, &err));
    CHECK(!err.isEmpty());
    return true;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    struct { const char *name; bool (*fn)(); } tests[] = {
        {"struct-layout", layout}, {"pick-device-and-move", pickAndMove}, {"buttons-and-wheel", buttons},
        {"driver-missing", driverMissing}, {"no-physical-mouse", noMouse}, {"send-failure-falls-back", sendFailureReported},
        {"lifecycle", lifecycle}, {"bad-library-path", badLibrary}};
    for (auto &t : tests)
    {
        if (!t.fn()) { qCritical() << "FAILED" << t.name; return 1; }
        qInfo().noquote() << "PASS" << t.name;
    }
    qInfo() << "PASS interception";
    return 0;
}
