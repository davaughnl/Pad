#ifndef PAD_WINDOWSPROFILEASSOCIATION_H
#define PAD_WINDOWSPROFILEASSOCIATION_H

#include <QDir>
#include <QSettings>
#include <QString>

// Isolated root/format permit tests without touching real file associations.
// Never write .amgp's default value or Explorer's UserChoice. Windows owns the
// user's default selection; registration only adds Pad to Open With.
namespace PadProfileAssociation {
inline QString progId() { return QStringLiteral("io.github.davaughnl.Pad.amgp"); }
inline QString settingsPath(const QString &root, const QString &subkey, QSettings::Format format)
{
    QString path = root + "/" + subkey;
#ifdef Q_OS_WIN
    if (format == QSettings::NativeFormat)
        path.replace('/', '\\'); // QSettings registry constructor requires backslash separators.
#else
    Q_UNUSED(format)
#endif
    return path;
}
inline QString marker() { return QStringLiteral("io.github.davaughnl.Pad"); }
inline QString command(const QString &executable)
{
    return QStringLiteral("\"%1\" \"%2\"").arg(QDir::toNativeSeparators(executable), QStringLiteral("%1"));
}
inline bool contains(const QString &root, QSettings::Format format)
{
    QSettings program(settingsPath(root, progId(), format), format);
    QSettings openWith(settingsPath(root, ".amgp/OpenWithProgids", format), format);
    return program.value("PadOwner").toString() == marker() && openWith.contains(progId());
}
inline bool registerProfile(const QString &root, QSettings::Format format, const QString &executable)
{
    QSettings program(settingsPath(root, progId(), format), format);
    // A matching name alone is not ownership. Do not overwrite unmarked keys.
    if (!program.allKeys().isEmpty() && program.value("PadOwner").toString() != marker())
        return false;
    program.setValue("PadOwner", marker());
    program.setValue("Default", QStringLiteral("Pad Profile"));
    program.setValue("shell/open/command/Default", command(executable));
    program.setValue("DefaultIcon/Default", QStringLiteral("\"%1\",0").arg(QDir::toNativeSeparators(executable)));
    program.sync();
    if (program.status() != QSettings::NoError)
        return false;
    QSettings openWith(settingsPath(root, ".amgp/OpenWithProgids", format), format);
    openWith.setValue(progId(), QString());
    openWith.sync();
    return openWith.status() == QSettings::NoError;
}
inline bool unregisterProfile(const QString &root, QSettings::Format format)
{
    QSettings program(settingsPath(root, progId(), format), format);
    if (program.value("PadOwner").toString() != marker())
        return false;
    QSettings openWith(settingsPath(root, ".amgp/OpenWithProgids", format), format);
    openWith.remove(progId());
    openWith.sync();
    // Keep Pad's ProgID: Windows UserChoice or a user-selected default may
    // reference it. Removing it would break that choice. No upstream keys or
    // any default association are removed, even during unregistration.
    return openWith.status() == QSettings::NoError;
}
}
#endif
