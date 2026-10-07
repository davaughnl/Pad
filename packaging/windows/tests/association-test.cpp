#include "../../../src/pad/windowsprofileassociation.h"
#include <QCoreApplication>
#include <QDebug>
#include <QTemporaryDir>
#include <QUuid>

static void require(bool ok, const char *message)
{
    if (!ok) qFatal("%s", message);
}
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    require(temp.isValid(), "Temporary fixture directory failed");
#ifdef Q_OS_WIN
    const QString root = "HKEY_CURRENT_USER/Software/PadAssociationTests/" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto format = QSettings::NativeFormat;
#else
    const QString root = temp.path() + "/registry";
    const auto format = QSettings::IniFormat;
#endif
    const QString exe = "C:/Pad folder/bin/pad.exe";
    QSettings extension(root + "/.amgp", format);
    QSettings upstream(root + "/AntiMicro.amgp", format);
    QSettings otherOpen(root + "/.amgp/OpenWithProgids", format);
    upstream.setValue("Default", "AntiMicro Profile");
    upstream.setValue("shell/open/command/Default", "upstream-command");
    otherOpen.setValue("Other.Profile", "foreign-value");
    upstream.sync(); otherOpen.sync();
    for (const QString &original : {QString(), QString("AntiMicro.amgp"), QString("Other.Profile")}) {
        if (original.isEmpty()) extension.remove("Default");
        else extension.setValue("Default", original);
        extension.sync();
        require(!PadProfileAssociation::contains(root, format), "Foreign association was treated as Pad");
        require(PadProfileAssociation::registerProfile(root, format, exe), "Register failed");
        require(PadProfileAssociation::registerProfile(root, format, exe), "Register is not idempotent");
        require(PadProfileAssociation::contains(root, format), "Pad registration not detected");
        extension.sync(); upstream.sync(); otherOpen.sync();
        require(extension.value("Default").toString() == original, "Extension default was hijacked");
        require(upstream.value("shell/open/command/Default").toString() == "upstream-command", "Upstream command changed");
        require(otherOpen.value("Other.Profile").toString() == "foreign-value", "Foreign Open With changed");
        QSettings program(root + "/" + PadProfileAssociation::progId(), format);
        require(program.value("shell/open/command/Default").toString() == PadProfileAssociation::command(exe), "Command quoting incorrect");
        require(PadProfileAssociation::unregisterProfile(root, format), "Unregister failed");
        require(!PadProfileAssociation::contains(root, format), "Unregister did not remove own Open With");
        extension.sync(); upstream.sync(); otherOpen.sync();
        require(extension.value("Default").toString() == original, "Unregister changed extension default");
        require(upstream.value("Default").toString() == "AntiMicro Profile", "Unregister removed upstream ProgID");
        require(otherOpen.contains("Other.Profile"), "Unregister removed foreign Open With");
    }
    // A user-selected Pad default must remain usable after unregistering Open With.
    extension.setValue("Default", PadProfileAssociation::progId()); extension.sync();
    require(PadProfileAssociation::registerProfile(root, format, exe), "Pad default register failed");
    require(PadProfileAssociation::unregisterProfile(root, format), "Pad default unregister failed");
    QSettings program(root + "/" + PadProfileAssociation::progId(), format);
    require(program.contains("shell/open/command/Default"), "User-selected Pad default now dangles");
    // An unmarked same-name key must not be stolen or deleted.
    program.remove(""); program.setValue("Default", "Unverified same-name registration"); program.sync();
    require(!PadProfileAssociation::registerProfile(root, format, exe), "Unmarked ProgID overwritten");
    require(!PadProfileAssociation::unregisterProfile(root, format), "Unmarked ProgID removed");
    require(program.value("Default").toString() == "Unverified same-name registration", "Unmarked ProgID changed");
#ifdef Q_OS_WIN
    QSettings cleanup(root, format); cleanup.remove(""); cleanup.sync();
#endif
    qInfo() << "PASS: Pad Open With registration preserves all foreign/default associations";
    return 0;
}
