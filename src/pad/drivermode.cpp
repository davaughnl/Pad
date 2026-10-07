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
    if (!PadInterception::loadApi(dir + QStringLiteral("/interception.dll"), &api, &error))
    {
        if (detail) *detail = error;
        return Status::NotInstalled;
    }
    PadInterception::MouseInjector probe(api);
    if (!probe.init(&error))
    {
        if (detail) *detail = error;
        return Status::NotInstalled;
    }
    return Status::Ready;
#else
    if (detail) *detail = QStringLiteral("Driver mode is only available on Windows.");
    return Status::Unsupported;
#endif
}

bool startInstall(bool install, QString *error)
{
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
    WaitForSingleObject(info.hProcess, 60000);
    GetExitCodeProcess(info.hProcess, &code);
    CloseHandle(info.hProcess);
    if (code != 0)
    {
        if (error) *error = QStringLiteral("The driver installer reported an error.");
        return false;
    }
    return true;
#else
    Q_UNUSED(install);
    if (error) *error = QStringLiteral("Driver mode is only available on Windows.");
    return false;
#endif
}

} // namespace PadDriverMode
