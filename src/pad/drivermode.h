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
    NotInstalled,   // files present, no sign of the driver on this PC
    InstalledUnavailable, // filter is registered (or the probe failed) but the driver does not answer: restart pending, in use, or blocked. Never offer a blind reinstall.
    Ready           // driver answers; mouse output can use it
};

Status status(QString *detail = nullptr);
// Runs the bundled driver installer with a Windows administrator prompt. A restart is required afterwards.
// Blocking: call from a worker thread, never the UI thread. stillRunning is set when the helper outlives the wait
// (it may still finish); callers must not start a second helper then.
bool startInstall(bool install, QString *error = nullptr, bool *stillRunning = nullptr);
QString recoveryNotePath(); // RECOVERY.md next to the driver files, empty when absent
QString driverDirectory();

} // namespace PadDriverMode

#endif
