#ifndef PAD_DRIVERMODE_H
#define PAD_DRIVERMODE_H

#include <QString>

// Driver mode (experimental, off by default): settings key and driver management for the UI.
namespace PadDriverMode {

constexpr const char *SettingsKey = "DriverMode"; // bool, stored in Pad's settings file

enum class Status
{
    Unsupported,    // not Windows
    NotBundled,     // this copy of Pad has no driver files (portable zip without them)
    NotInstalled,   // files present, driver not installed or not running yet
    Ready           // driver answers; mouse output can use it
};

Status status(QString *detail = nullptr);
// Runs the bundled driver installer with a Windows administrator prompt. A restart is required afterwards.
bool startInstall(bool install, QString *error = nullptr);
QString driverDirectory();

} // namespace PadDriverMode

#endif
