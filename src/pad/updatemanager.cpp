// SPDX-License-Identifier: GPL-3.0-or-later
#include "updatemanager.h"

#include <QSslSocket>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {
const qint64 maxHashBytes = 4096;
const qint64 maxApiBytes = 1024 * 1024;
QString tr(const char *text) { return QObject::tr(text); }
}

QUrl UpdateManager::defaultApiUrl()
{
    return QUrl(QStringLiteral("https://api.github.com/repos/davaughnl/Pad/releases/latest"));
}

UpdateManager::UpdateManager(const QString &currentVersion, QObject *parent, const QUrl &apiUrl, const QString &testHost)
    : QObject(parent), m_current(currentVersion), m_apiUrl(apiUrl), m_testHost(testHost),
      m_net(new QNetworkAccessManager(this))
{
    m_launcher = [](const QString &path) {
#ifdef Q_OS_WIN
        return QProcess::startDetached(path, {QStringLiteral("/S")});
#else
        Q_UNUSED(path);
        return false;
#endif
    };
}

UpdateManager::~UpdateManager()
{
    if (m_reply) { m_reply->disconnect(this); m_reply->abort(); }
    delete m_file;
}

bool UpdateManager::canInstallHere() const
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

bool UpdateManager::hostAllowed(const QUrl &url, const QString &testHost)
{
    const QString host = url.host().toLower();
    if (!testHost.isEmpty() && host == testHost.toLower()) return true;
    if (url.scheme() != QLatin1String("https")) return false;
    if (host == QLatin1String("api.github.com") || host == QLatin1String("github.com")) return true;
    return host == QLatin1String("githubusercontent.com") || host.endsWith(QLatin1String(".githubusercontent.com"));
}

bool UpdateManager::parseVersion(const QString &text, QVector<int> &core, QString &prerelease)
{
    static const QRegularExpression re(QStringLiteral("^v?(\\d+)\\.(\\d+)\\.(\\d+)(?:-([0-9A-Za-z.-]+))?(?:\\+[0-9A-Za-z.-]+)?$"));
    const auto m = re.match(text.trimmed());
    if (!m.hasMatch()) return false;
    core = {m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt()};
    prerelease = m.captured(4);
    return true;
}

int UpdateManager::compareVersions(const QString &current, const QString &candidate)
{
    QVector<int> a, b; QString pa, pb;
    const bool haveB = parseVersion(candidate, b, pb);
    if (!haveB) return 0;
    if (!parseVersion(current, a, pa)) return 1;
    for (int i = 0; i < 3; ++i)
        if (a[i] != b[i]) return b[i] > a[i] ? 1 : -1;
    if (pa == pb) return 0;
    if (pa.isEmpty()) return -1;   // current is the release, candidate is a prerelease
    if (pb.isEmpty()) return 1;
    return QString::compare(pb, pa) > 0 ? 1 : -1;
}

bool UpdateManager::parseRelease(const QByteArray &json, Release *out, QString *error)
{
    auto bad = [error](const QString &why) { if (error) *error = why; return false; };
    QJsonParseError parse;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject()) return bad(tr("The release information was not readable."));
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("draft")).toBool() || root.value(QStringLiteral("prerelease")).toBool())
        return bad(tr("No published release is available."));
    QString tag = root.value(QStringLiteral("tag_name")).toString();
    QVector<int> core; QString pre;
    if (!parseVersion(tag, core, pre) || !pre.isEmpty()) return bad(tr("The release has no usable version."));
    if (tag.startsWith(QLatin1Char('v'))) tag.remove(0, 1);
    const QString setupName = QStringLiteral("Pad-%1-Windows-x64-Setup.exe").arg(tag);
    const QString hashName = setupName + QStringLiteral(".sha256");
    Release r; r.version = tag; r.setupName = setupName;
    for (const QJsonValue &v : root.value(QStringLiteral("assets")).toArray())
    {
        const QJsonObject asset = v.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        const QUrl url(asset.value(QStringLiteral("browser_download_url")).toString());
        if (name == setupName) { r.setupUrl = url; r.size = static_cast<qint64>(asset.value(QStringLiteral("size")).toDouble()); }
        else if (name == hashName) r.hashUrl = url;
    }
    if (!r.setupUrl.isValid() || !r.hashUrl.isValid()) return bad(tr("The release does not include a verified installer."));
    if (r.size <= 0 || r.size > maxInstallerBytes) return bad(tr("The installer size is not valid."));
    if (out) *out = r;
    return true;
}

QString UpdateManager::parseSha256(const QByteArray &text, const QString &fileName)
{
    static const QRegularExpression re(QStringLiteral("^([0-9A-Fa-f]{64})(?:\\s+\\*?(.+?))?\\s*$"));
    const QStringList lines = QString::fromUtf8(text).split(QLatin1Char('\n'), QString::SkipEmptyParts);
    for (const QString &line : lines)
    {
        const auto m = re.match(line.trimmed());
        if (!m.hasMatch()) continue;
        if (m.captured(2).isEmpty() || m.captured(2) == fileName) return m.captured(1).toLower();
    }
    return QString();
}

void UpdateManager::setState(State state)
{
    if (m_state == state) return;
    m_state = state;
    emit stateChanged(state);
}

void UpdateManager::fail(const QString &message)
{
    m_error = message;
    if (m_file) { m_file->close(); m_file->remove(); delete m_file; m_file = nullptr; }
    setState(Failed);
}

QNetworkReply *UpdateManager::get(const QUrl &url, bool binary)
{
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Pad-Updater");
    request.setRawHeader("Accept", binary ? "application/octet-stream" : "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::UserVerifiedRedirectPolicy);
    request.setTransferTimeout(30000);
    QNetworkReply *reply = m_net->get(request);
    connect(reply, &QNetworkReply::redirected, this, [this, reply](const QUrl &target) {
        if (hostAllowed(target, m_testHost)) emit reply->redirectAllowed();
        else reply->abort();
    });
    m_reply = reply;
    return reply;
}

void UpdateManager::check()
{
    if (m_state == Checking || m_state == Downloading || m_state == Verifying || m_state == Installing) return;
    m_error.clear();
    if (!hostAllowed(m_apiUrl, m_testHost)) { fail(tr("The update address is not allowed.")); return; }
    setState(Checking);
    QNetworkReply *reply = get(m_apiUrl, false);
    connect(reply, &QNetworkReply::finished, this, &UpdateManager::onCheckFinished);
    connect(reply, &QNetworkReply::downloadProgress, this, [reply](qint64 got, qint64) { if (got > maxApiBytes) reply->abort(); });
}

void UpdateManager::onCheckFinished()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) return;
    reply->deleteLater();
    if (reply != m_reply) return;
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    // GitHub answers 404 for releases/latest while no release is published yet (drafts do not count): nothing newer exists.
    if (status == 404) { m_release = Release(); setState(UpToDate); return; }
    if (reply->error() != QNetworkReply::NoError)
    {
        const auto e = reply->error();
        if (e == QNetworkReply::SslHandshakeFailedError)
            fail(tr("Could not check for updates: the secure connection failed."));
        else if (e == QNetworkReply::ProtocolUnknownError && !QSslSocket::supportsSsl())
            fail(tr("Could not check for updates: secure connections are not available."));
        else
            fail(tr("Could not check for updates."));
        return;
    }
    if (status != 200) { fail(tr("Could not check for updates.")); return; }
    Release release; QString why;
    if (!parseRelease(reply->read(maxApiBytes), &release, &why)) { fail(why); return; }
    m_release = release;
    setState(compareVersions(m_current, release.version) > 0 ? Available : UpToDate);
}

QString UpdateManager::downloadDir() const
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath(QStringLiteral("pad-update"));
}

void UpdateManager::download()
{
    if (m_state != Available) return;
    if (!hostAllowed(m_release.hashUrl, m_testHost) || !hostAllowed(m_release.setupUrl, m_testHost)) { fail(tr("The download address is not allowed.")); return; }
    m_error.clear(); m_hashBytes.clear();
    setState(Downloading);
    emit progress(0);
    QNetworkReply *reply = get(m_release.hashUrl, false);
    connect(reply, &QNetworkReply::finished, this, &UpdateManager::onHashFinished);
    connect(reply, &QNetworkReply::downloadProgress, this, [reply](qint64 got, qint64) { if (got > maxHashBytes) reply->abort(); });
}

void UpdateManager::onHashFinished()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) return;
    reply->deleteLater();
    if (reply != m_reply) return;
    if (reply->error() != QNetworkReply::NoError || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200)
    { fail(tr("Download failed.")); return; }
    m_expectedHash = parseSha256(reply->read(maxHashBytes), m_release.setupName);
    if (m_expectedHash.isEmpty()) { fail(tr("The installer checksum was not readable.")); return; }
    startSetupDownload();
}

void UpdateManager::startSetupDownload()
{
    QDir dir(downloadDir());
    dir.removeRecursively();
    if (!QDir().mkpath(downloadDir())) { fail(tr("Download failed.")); return; }
    m_installerPath = dir.filePath(m_release.setupName);
    m_file = new QFile(m_installerPath, this);
    if (!m_file->open(QIODevice::WriteOnly)) { fail(tr("Download failed.")); return; }
    QNetworkReply *reply = get(m_release.setupUrl, true);
    connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
        const QByteArray chunk = reply->readAll();
        if (!m_file) return;
        if (m_file->size() + chunk.size() > m_release.size) { reply->abort(); return; }
        if (m_file->write(chunk) != chunk.size()) reply->abort();
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 got, qint64 total) {
        const qint64 whole = total > 0 ? total : m_release.size;
        if (whole > 0) emit progress(static_cast<int>(qMin<qint64>(100, got * 100 / whole)));
    });
    connect(reply, &QNetworkReply::finished, this, &UpdateManager::onDownloadFinished);
}

void UpdateManager::onDownloadFinished()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) return;
    reply->deleteLater();
    if (reply != m_reply || !m_file) return;
    if (reply->error() != QNetworkReply::NoError || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200)
    { fail(tr("Download failed.")); return; }
    m_file->close();
    setState(Verifying);
    QFile file(m_installerPath);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!file.open(QIODevice::ReadOnly) || file.size() != m_release.size || !hash.addData(&file))
    { file.close(); fail(tr("Update file failed verification.")); return; }
    file.close();
    if (QString::fromLatin1(hash.result().toHex()) != m_expectedHash)
    { QFile::remove(m_installerPath); fail(tr("Update file failed verification.")); return; }
    delete m_file; m_file = nullptr;
    emit progress(100);
    setState(Ready);
}

void UpdateManager::install()
{
    if (m_state != Ready) return;
    if (!m_launcher || (!canInstallHere() && !m_testHost.size())) { fail(tr("Updating is not supported on this system.")); return; }
    setState(Installing);
    if (!m_launcher(m_installerPath)) { fail(tr("Could not start the installer.")); return; }
    emit quitRequested();
}

void UpdateManager::cancel()
{
    if (m_reply) { m_reply->disconnect(this); m_reply->abort(); }
    if (m_file) { m_file->close(); m_file->remove(); delete m_file; m_file = nullptr; }
    m_error.clear();
    m_state = Idle;
    emit stateChanged(Idle);
}
