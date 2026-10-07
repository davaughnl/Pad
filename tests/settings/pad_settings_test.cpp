#include "../../src/pad/settingsmigration.h"

#include <QTemporaryDir>
#include <QDateTime>
#include <iostream>
#include <cstdlib>

using namespace PadderCommon;

static void check(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

static void writeFile(const QString &path, const QByteArray &contents)
{
    check(QDir().mkpath(QFileInfo(path).absolutePath()), "create fixture directory");
    QFile file(path);
    check(file.open(QIODevice::WriteOnly), "open fixture file");
    check(file.write(contents) == contents.size(), "write fixture file");
}

static QByteArray readFile(const QString &path)
{
    QFile file(path);
    check(file.open(QIODevice::ReadOnly), "read fixture file");
    return file.readAll();
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("Pad");
    QTemporaryDir directory;
    check(directory.isValid(), "temporary directory");
    const QString root = directory.path();
#ifndef Q_OS_WIN
    qputenv("XDG_CONFIG_HOME", root.toUtf8());
    check(configPath() == root + "/pad", "XDG Pad directory");
    check(configFilePath() == root + "/pad/pad_settings.ini", "Pad settings filename");
    check(upstreamSettingsPaths().first() == root + "/antimicrox/antimicrox_settings.ini", "upstream source path");
#else
    check(configFileName == "pad_settings.ini", "Pad settings filename");
#ifndef WIN_PORTABLE_PACKAGE
    const QSettings location(QSettings::IniFormat, QSettings::UserScope, "Pad", "Pad");
    check(configPath() == QFileInfo(location.fileName()).absolutePath(), "Windows QSettings roaming path");
#endif
#endif
#ifndef Q_OS_WIN
    qunsetenv("XDG_CONFIG_HOME");
    check(configPath() == QDir::homePath() + "/.config/pad", "default XDG fallback directory");
    check(upstreamSettingsPaths().first() == QDir::homePath() + "/.config/antimicrox/antimicrox_settings.ini", "default upstream fallback");
    qputenv("XDG_CONFIG_HOME", root.toUtf8());
#endif
    const QString source = root + "/antimicrox/antimicrox_settings.ini";
    const QString destination = root + "/pad/pad_settings.ini";
    const QByteArray original("[General]\nDefaultProfileDir=/profiles\nNumberRecentProfiles=8\n");
    writeFile(source, original);
    const QDateTime modified = QFileInfo(source).lastModified();
    const auto permissions = QFileInfo(source).permissions();
    const QString sibling = root + "/antimicrox/controller.amgp";
    writeFile(sibling, "profile untouched");
    auto result = importSettings(destination, {source});
    check(result.status == SettingsImportStatus::Imported, "first-run import");
    check(readFile(destination) == original, "byte-identical imported settings");
    check(readFile(source) == original, "source remains unchanged");
    check(QFileInfo(source).lastModified() == modified, "source timestamp unchanged");
    check(QFileInfo(source).permissions() == permissions, "source permissions unchanged");
    check(readFile(sibling) == "profile untouched", "upstream sibling profile untouched");
    check(settingsImportMessage(result).contains(source) && settingsImportMessage(result).contains(destination), "real paths in success dialog");
    QSettings settings(destination, QSettings::IniFormat);
    check(settings.value("NumberRecentProfiles").toInt() == 8, "copied settings readable by QSettings");

    writeFile(destination, "Pad-owned contents");
    result = importSettings(destination, {source});
    check(result.status == SettingsImportStatus::ExistingSettings, "existing Pad skips import");
    check(readFile(destination) == "Pad-owned contents", "Pad file never overwritten");
    check(readFile(source) == original, "source unchanged on repeated launch");

    writeFile(destination, QByteArray());
    result = importSettings(destination, {source});
    check(result.status == SettingsImportStatus::ExistingSettings && readFile(destination).isEmpty(), "empty existing Pad file never replaced");
#ifndef Q_OS_WIN
    const QString linkedDestination = root + "/linked/pad_settings.ini";
    check(QDir().mkpath(QFileInfo(linkedDestination).absolutePath()), "symlink fixture directory");
    check(QFile::link(root + "/nonexistent.ini", linkedDestination), "dangling symlink fixture");
    result = importSettings(linkedDestination, {source});
    check(result.status == SettingsImportStatus::ExistingSettings && QFileInfo(linkedDestination).isSymLink(), "dangling Pad symlink never replaced");
#endif

    const QString emptyDestination = root + "/missing/pad_settings.ini";
    result = importSettings(emptyDestination, {root + "/absent.ini"});
    check(result.status == SettingsImportStatus::NoSource, "missing upstream is no-op");
    check(!QFileInfo::exists(emptyDestination), "no blank settings file created by import");

    const QString older = root + "/antimicroX/antimicroX_settings.ini";
    writeFile(older, "older settings");
    const QString priorityDestination = root + "/priority/pad_settings.ini";
    result = importSettings(priorityDestination, {source, older});
    check(result.status == SettingsImportStatus::Imported && readFile(priorityDestination) == original, "current upstream wins over older files");
    const QString fallbackDestination = root + "/fallback/pad_settings.ini";
    result = importSettings(fallbackDestination, {root + "/absent.ini", older});
    check(result.status == SettingsImportStatus::Imported && readFile(fallbackDestination) == "older settings", "legacy fallback copied");
    check(readFile(older) == "older settings", "legacy fallback untouched");

    const QString blocked = root + "/blocked";
    writeFile(blocked, "not a directory");
    result = importSettings(blocked + "/pad_settings.ini", {source});
    check(result.status == SettingsImportStatus::Failed, "copy failure reported");
    check(readFile(source) == original, "source unchanged on copy failure");
    check(settingsImportMessage(result).contains(result.source) && settingsImportMessage(result).contains(result.destination), "real paths in error dialog");
    check(!settingsImportMessage(result).contains("renaming"), "failure does not suggest destructive migration");
    check(!settingsImportMessage(result).contains("AntiMicroX"), "dialog product identity is Pad");
    std::cout << "PASS: Pad paths, first import, no overwrite, no source, priority, legacy fallback, non-destructiveness, failure, dialog paths\n";
    return 0;
}
