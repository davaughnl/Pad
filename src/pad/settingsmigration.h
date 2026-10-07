#ifndef PAD_SETTINGSMIGRATION_H
#define PAD_SETTINGSMIGRATION_H

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>

namespace PadderCommon {

inline QString configPath()
{
#if defined(Q_OS_WIN) && defined(WIN_PORTABLE_PACKAGE)
    return QCoreApplication::applicationDirPath();
#elif defined(Q_OS_WIN)
    // QSettings' user-scope INI location is the roaming %APPDATA% directory.
    const QSettings location(QSettings::IniFormat, QSettings::UserScope, "Pad", "Pad");
    return QFileInfo(location.fileName()).absolutePath();
#else
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return QDir(directory).filePath(QStringLiteral("pad"));
#endif
}

const QString configFileName = QStringLiteral("pad_settings.ini");

inline QString configFilePath()
{
    return QDir(configPath()).filePath(configFileName);
}

inline QStringList upstreamSettingsPaths()
{
    QStringList paths;
#ifdef Q_OS_WIN
#ifdef WIN_PORTABLE_PACKAGE
    paths << QDir(QCoreApplication::applicationDirPath()).filePath("antimicrox_settings.ini");
#endif
    // Preserve the exact former paths when looking for an import source.
    const QString localAppData = QString::fromUtf8(qgetenv("LocalAppData"));
    const QString upstreamDirectory = localAppData.isEmpty()
        ? QDir::homePath() + "/.antimicrox" : QDir(localAppData).filePath("antimicrox");
    paths << QDir(upstreamDirectory).filePath("antimicrox_settings.ini");
    const QString antimicroDirectory = localAppData.isEmpty()
        ? QDir::homePath() + "/.antimicro" : QDir(localAppData).filePath("antimicro");
    paths << QDir(antimicroDirectory).filePath("antimicro_settings.ini");
#else
    const QString xdg = QString::fromUtf8(qgetenv("XDG_CONFIG_HOME"));
    const QString directory = xdg.isEmpty() ? QDir::homePath() + "/.config" : xdg;
    paths << QDir(directory).filePath("antimicrox/antimicrox_settings.ini")
          << QDir(directory).filePath("antimicroX/antimicroX_settings.ini")
          << QDir(directory).filePath("antimicro/antimicro_settings.ini");
#endif
    paths.removeDuplicates();
    return paths;
}

enum class SettingsImportStatus { ExistingSettings, NoSource, Imported, Failed };

struct SettingsImportResult
{
    SettingsImportStatus status;
    QString source;
    QString destination;
};

// Destination existence is the one-time guard. QFile::copy never overwrites,
// and no import path renames, removes, edits, or writes to the source.
inline SettingsImportResult importSettings(const QString &destination, const QStringList &sources)
{
    const QFileInfo target(destination);
    if (target.exists() || target.isSymLink())
        return {SettingsImportStatus::ExistingSettings, QString(), target.absoluteFilePath()};

    for (const QString &source : sources)
    {
        const QFileInfo original(source);
        if (!original.exists() || !original.isFile())
            continue;
        const QString from = original.absoluteFilePath();
        const QString to = target.absoluteFilePath();
        const bool copied = QDir().mkpath(target.absolutePath()) && QFile::copy(from, to);
        return {copied ? SettingsImportStatus::Imported : SettingsImportStatus::Failed, from, to};
    }
    return {SettingsImportStatus::NoSource, QString(), target.absoluteFilePath()};
}

inline QString settingsImportMessage(const SettingsImportResult &result)
{
    if (result.status == SettingsImportStatus::Imported)
        return QCoreApplication::translate("SettingsMigration",
            "Your settings have been copied to Pad.\n\nOriginal file:\n%1\n\nPad settings file:\n%2\n\n"
            "The original file has not been changed.").arg(QDir::toNativeSeparators(result.source),
                                                          QDir::toNativeSeparators(result.destination));
    return QCoreApplication::translate("SettingsMigration",
        "Pad could not copy your settings.\n\nOriginal file:\n%1\n\nPad settings file:\n%2\n\n"
        "The original file has not been changed. You can copy it manually to the Pad settings file shown above.")
        .arg(QDir::toNativeSeparators(result.source), QDir::toNativeSeparators(result.destination));
}

} // namespace PadderCommon

#endif // PAD_SETTINGSMIGRATION_H
