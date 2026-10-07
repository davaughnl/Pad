#include "drivermode.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#ifdef Q_OS_WIN
    #include <windows.h>
    #include <shellapi.h>
    #include "pad/interception.h"
#endif

namespace PadDriverMode {

QString driverDirectory()
{
    const QDir app(QCoreApplication::applicationDirPath());
    for (const QString &rel : {QStringLiteral("driver"), QStringLiteral("../driver")})
    {
        if (QFileInfo::exists(app.filePath(rel + QStringLiteral("/interception.dll"))))
            return QDir::cleanPath(app.filePath(rel));
    }
    return QString();
}

QString recoveryNotePath()
{
    const QString dir = driverDirectory();
    if (dir.isEmpty()) return QString();
    const QString path = dir + QStringLiteral("/RECOVERY.md");
    return QFileInfo::exists(path) ? path : QString();
}

#ifdef Q_OS_WIN
// Registry evidence only; a hint that a filter is registered, not proof it is Interception.
static bool filterRegistered()
{
    for (const wchar_t *name : {L"SYSTEM\\CurrentControlSet\\Services\\mouse", L"SYSTEM\\CurrentControlSet\\Services\\keyboard"})
    {
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, name, 0, KEY_READ, &key) == ERROR_SUCCESS)
        {
            RegCloseKey(key);
            return true;
        }
    }
    return false;
}
#endif

Status status(QString *detail)
{
#ifdef Q_OS_WIN
    const QString dir = driverDirectory();
    if (dir.isEmpty())
    {
        if (detail) *detail = QStringLiteral("Driver files are not included in this copy of Pad.");
        return Status::NotBundled;
    }
    PadInterception::Api api;
    QString error;
    // A failed probe is not proof the driver is absent: it may be waiting for a restart or in use.
    const bool registered = filterRegistered();
    if (!PadInterception::loadApi(dir + QStringLiteral("/interception.dll"), &api, &error))
    {
        if (detail) *detail = error;
        return registered ? Status::InstalledUnavailable : Status::NotInstalled;
    }
    PadInterception::MouseInjector probe(api); // opens and closes device handles only; sends nothing
    if (!probe.init(&error))
    {
        if (detail) *detail = error;
        return registered ? Status::InstalledUnavailable : Status::NotInstalled;
    }
    return Status::Ready;
#else
    if (detail) *detail = QStringLiteral("Driver mode is only available on Windows.");
    return Status::Unsupported;
#endif
}

bool startInstall(bool install, QString *error, bool *stillRunning)
{
    if (stillRunning) *stillRunning = false;
#ifdef Q_OS_WIN
    const QString dir = driverDirectory();
    const QString exe = dir + QStringLiteral("/install-interception.exe");
    if (dir.isEmpty() || !QFileInfo::exists(exe))
    {
        if (error) *error = QStringLiteral("Driver files are not included in this copy of Pad.");
        return false;
    }
    const std::wstring file = QDir::toNativeSeparators(exe).toStdWString();
    const std::wstring params = install ? L"/install" : L"/uninstall";
    const std::wstring work = QDir::toNativeSeparators(dir).toStdWString();
    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = L"runas"; // Windows administrator prompt
    info.lpFile = file.c_str();
    info.lpParameters = params.c_str();
    info.lpDirectory = work.c_str();
    info.nShow = SW_HIDE;
    if (!ShellExecuteExW(&info))
    {
        if (error) *error = QStringLiteral("The administrator prompt was cancelled or failed.");
        return false;
    }
    DWORD code = 1;
    // The user may spend a while on the administrator prompt. Do not kill the helper and never relaunch it on timeout.
    if (WaitForSingleObject(info.hProcess, 180000) == WAIT_TIMEOUT)
    {
        CloseHandle(info.hProcess);
        if (stillRunning) *stillRunning = true;
        if (error) *error = QStringLiteral("The driver installer is still running. Wait for it to finish, then restart Windows.");
        return false;
    }
    GetExitCodeProcess(info.hProcess, &code);
    CloseHandle(info.hProcess);
    if (code != 0)
    {
        if (error) *error = QStringLiteral("The driver installer reported an error (code %1). Nothing was retried.").arg(code);
        return false;
    }
    return true;
#else
    Q_UNUSED(install);
    if (stillRunning) *stillRunning = false;
    if (error) *error = QStringLiteral("Driver mode is only available on Windows.");
    return false;
#endif
}

} // namespace PadDriverMode
