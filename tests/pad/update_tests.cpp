// SPDX-License-Identifier: GPL-3.0-or-later
// Exercises UpdateManager against a local HTTP server shaped like the GitHub release API.
#include "pad/updatemanager.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <cstdio>
#include <functional>
#include <stdexcept>

static void check(bool ok, const char *msg) { if (!ok) throw std::runtime_error(msg); }

struct Mock
{
    QTcpServer server;
    QMap<QString, QPair<int, QByteArray>> routes;
    QMap<QString, QString> redirects;
    int hits = 0;
    Mock()
    {
        check(server.listen(QHostAddress::LocalHost, 0), "listen failed");
        QObject::connect(&server, &QTcpServer::newConnection, [this] {
            while (auto *s = server.nextPendingConnection())
                QObject::connect(s, &QTcpSocket::readyRead, [this, s] {
                    const QByteArray req = s->readAll();
                    const QString path = QString::fromLatin1(req.split(' ').value(1));
                    ++hits;
                    QByteArray out;
                    if (redirects.contains(path))
                        out = "HTTP/1.1 302 Found\r\nLocation: " + redirects[path].toLatin1() + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                    else if (routes.contains(path))
                        out = "HTTP/1.1 " + QByteArray::number(routes[path].first) + " X\r\nContent-Length: " + QByteArray::number(routes[path].second.size()) + "\r\nConnection: close\r\n\r\n" + routes[path].second;
                    else out = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                    s->write(out); s->disconnectFromHost();
                });
        });
    }
    QString base() const { return QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()); }
};

static QByteArray releaseJson(const Mock &m, const QString &tag, const QByteArray &setup, bool withHash = true, bool draft = false, qint64 sizeOverride = -1)
{
    const QString ver = tag.startsWith('v') ? tag.mid(1) : tag;
    const QString name = "Pad-" + ver + "-Windows-x64-Setup.exe";
    QJsonArray assets;
    assets.append(QJsonObject{{"name", name}, {"size", double(sizeOverride >= 0 ? sizeOverride : setup.size())}, {"browser_download_url", m.base() + "/dl/" + name}});
    if (withHash) assets.append(QJsonObject{{"name", name + ".sha256"}, {"size", 100}, {"browser_download_url", m.base() + "/dl/" + name + ".sha256"}});
    return QJsonDocument(QJsonObject{{"tag_name", tag}, {"draft", draft}, {"prerelease", false}, {"assets", assets}}).toJson();
}

static bool waitFor(UpdateManager &u, std::initializer_list<UpdateManager::State> states, int ms = 8000)
{
    QEventLoop loop; QTimer timeout; timeout.setSingleShot(true);
    auto done = [&] { for (auto s : states) if (u.state() == s) return true; return false; };
    if (done()) return true;
    QObject::connect(&u, &UpdateManager::stateChanged, &loop, [&] { if (done()) loop.quit(); });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(ms); loop.exec();
    return done();
}

static void unit()
{
    QVector<int> c; QString pre;
    check(UpdateManager::parseVersion("v1.2.3", c, pre) && c == QVector<int>({1, 2, 3}) && pre.isEmpty(), "parse v1.2.3");
    check(UpdateManager::parseVersion("1.0.0-rc.1", c, pre) && pre == "rc.1", "parse prerelease");
    check(!UpdateManager::parseVersion("1.2", c, pre) && !UpdateManager::parseVersion("Development build", c, pre) && !UpdateManager::parseVersion("", c, pre), "reject junk");
    check(UpdateManager::compareVersions("1.0.0", "1.0.1") > 0, "patch newer");
    check(UpdateManager::compareVersions("1.9.0", "1.10.0") > 0, "numeric not lexical");
    check(UpdateManager::compareVersions("2.0.0", "1.9.9") < 0, "older candidate");
    check(UpdateManager::compareVersions("1.0.0", "v1.0.0") == 0, "equal with v");
    check(UpdateManager::compareVersions("", "0.0.1") > 0, "dev build older than any release");
    check(UpdateManager::compareVersions("Development build", "1.0.0") > 0, "dev label older");
    check(UpdateManager::compareVersions("1.0.0-rc.1", "1.0.0") > 0, "release beats its prerelease");
    check(UpdateManager::compareVersions("1.0.0", "not-a-version") == 0, "unparsable candidate is not an update");
    const QString h(64, 'a');
    check(UpdateManager::parseSha256((h + "  Pad-1.0.0-Windows-x64-Setup.exe\n").toLatin1(), "Pad-1.0.0-Windows-x64-Setup.exe") == h, "sha with name");
    check(UpdateManager::parseSha256(h.toUpper().toLatin1(), "x").isEmpty() == false, "bare uppercase sha accepted");
    check(UpdateManager::parseSha256((h + "  other.exe\n").toLatin1(), "Pad.exe").isEmpty(), "sha for other file rejected");
    check(UpdateManager::parseSha256("zzz", "x").isEmpty(), "junk sha rejected");
    check(UpdateManager::hostAllowed(QUrl("https://github.com/x"), {}) && UpdateManager::hostAllowed(QUrl("https://objects.githubusercontent.com/x"), {}), "github hosts allowed");
    check(!UpdateManager::hostAllowed(QUrl("http://github.com/x"), {}), "http rejected");
    check(!UpdateManager::hostAllowed(QUrl("https://evilgithub.com/x"), {}) && !UpdateManager::hostAllowed(QUrl("https://github.com.evil.com/x"), {}) && !UpdateManager::hostAllowed(QUrl("https://xgithubusercontent.com/x"), {}), "lookalike hosts rejected");
    check(!UpdateManager::hostAllowed(QUrl("http://127.0.0.1/x"), {}), "loopback rejected in production");
}

static void scenario(const char *name, const std::function<void()> &fn) { fn(); std::printf("PASS %s\n", name); std::fflush(stdout); }

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    try {
        scenario("versions-and-hosts", unit);
        const QByteArray setup(5000, 'S');
        const QString good = QCryptographicHash::hash(setup, QCryptographicHash::Sha256).toHex();
        const QString api = "/repos/davaughnl/Pad/releases/latest";
        const QString nm = "Pad-1.2.0-Windows-x64-Setup.exe";
        auto mk = [&](Mock &m, const QString &tag = "v1.2.0") {
            m.routes[api] = {200, releaseJson(m, tag, setup)};
            m.routes["/dl/" + nm] = {200, setup};
            m.routes["/dl/" + nm + ".sha256"] = {200, (good + "  " + nm + "\n").toLatin1()};
        };
        scenario("up-to-date", [&] {
            Mock m; mk(m); UpdateManager u("1.2.0", nullptr, QUrl(m.base() + api), "127.0.0.1");
            u.check(); check(waitFor(u, {UpdateManager::UpToDate, UpdateManager::Failed}) && u.state() == UpdateManager::UpToDate, "not up to date");
        });
        scenario("available-download-verify-install", [&] {
            Mock m; mk(m); UpdateManager u("1.0.0", nullptr, QUrl(m.base() + api), "127.0.0.1");
            QString launched; bool quit = false; int last = -1;
            u.setInstallerLauncher([&](const QString &p) { launched = p; return true; });
            QObject::connect(&u, &UpdateManager::quitRequested, [&] { quit = true; });
            QObject::connect(&u, &UpdateManager::progress, [&](int p) { last = p; });
            u.check(); check(waitFor(u, {UpdateManager::Available, UpdateManager::Failed}) && u.state() == UpdateManager::Available, "not available");
            check(u.release().version == "1.2.0", "wrong version");
            u.download(); check(waitFor(u, {UpdateManager::Ready, UpdateManager::Failed}) && u.state() == UpdateManager::Ready, "download not ready");
            check(last == 100, "progress did not finish");
            QFile f(u.installerPath()); check(f.open(QIODevice::ReadOnly) && f.readAll() == setup, "installer bytes differ");
            check(launched.isEmpty(), "launched before install()");
            u.install(); check(launched == u.installerPath() && quit && u.state() == UpdateManager::Installing, "install did not launch and quit");
        });
        scenario("dev-build-gets-latest", [&] {
            Mock m; mk(m); UpdateManager u("", nullptr, QUrl(m.base() + api), "127.0.0.1");
            u.check(); check(waitFor(u, {UpdateManager::Available, UpdateManager::Failed}) && u.state() == UpdateManager::Available, "dev build not offered release");
        });
        scenario("hash-mismatch-never-launches", [&] {
            Mock m; mk(m); m.routes["/dl/" + nm + ".sha256"] = {200, (QString(64, '0') + "  " + nm + "\n").toLatin1()};
            UpdateManager u("1.0.0", nullptr, QUrl(m.base() + api), "127.0.0.1"); bool launched = false;
            u.setInstallerLauncher([&](const QString &) { launched = true; return true; });
            u.check(); waitFor(u, {UpdateManager::Available}); u.download();
            check(waitFor(u, {UpdateManager::Failed, UpdateManager::Ready}) && u.state() == UpdateManager::Failed, "mismatch not rejected");
            check(!QFile::exists(u.installerPath()), "bad file kept");
            u.install(); check(!launched, "launched unverified file");
        });
        scenario("tampered-bytes-rejected", [&] {
            Mock m; mk(m); QByteArray bad = setup; bad[10] = 'X'; m.routes["/dl/" + nm] = {200, bad};
            UpdateManager u("1.0.0", nullptr, QUrl(m.base() + api), "127.0.0.1");
            u.check(); waitFor(u, {UpdateManager::Available}); u.download();
            check(waitFor(u, {UpdateManager::Failed, UpdateManager::Ready}) && u.state() == UpdateManager::Failed, "tampered file accepted");
        });
        scenario("oversize-body-rejected", [&] {
            Mock m; mk(m); m.routes["/dl/" + nm] = {200, QByteArray(9000, 'S')};
            UpdateManager u("1.0.0", nullptr, QUrl(m.base() + api), "127.0.0.1");
            u.check(); waitFor(u, {UpdateManager::Available}); u.download();
            check(waitFor(u, {UpdateManager::Failed, UpdateManager::Ready}) && u.state() == UpdateManager::Failed, "oversize accepted");
        });
        scenario("missing-hash-asset", [&] {
            Mock m; mk(m); m.routes[api] = {200, releaseJson(m, "v1.2.0", setup, false)};
            UpdateManager u("1.0.0", nullptr, QUrl(m.base() + api), "127.0.0.1");
            u.check(); check(waitFor(u, {UpdateManager::Failed, UpdateManager::Available}) && u.state() == UpdateManager::Failed, "release without checksum accepted");
        });
        scenario("draft-and-garbage-and-http-errors", [&] {
            Mock m; mk(m); m.routes[api] = {200, releaseJson(m, "v1.2.0", setup, true, true)};
            UpdateManager a("1.0.0", nullptr, QUrl(m.base() + api), "127.0.0.1"); a.check();
            check(waitFor(a, {UpdateManager::Failed, UpdateManager::Available}) && a.state() == UpdateManager::Failed, "draft accepted");
            m.routes[api] = {200, "not json"}; UpdateManager b("1.0.0", nullptr, QUrl(m.base() + api), "127.0.0.1"); b.check();
            check(waitFor(b, {UpdateManager::Failed, UpdateManager::Available}) && b.state() == UpdateManager::Failed, "garbage accepted");
            m.routes[api] = {500, "x"}; UpdateManager c("1.0.0", nullptr, QUrl(m.base() + api), "127.0.0.1"); c.check();
            check(waitFor(c, {UpdateManager::Failed, UpdateManager::Available}) && c.state() == UpdateManager::Failed && !c.errorText().isEmpty(), "500 accepted");
        });
        scenario("redirect-to-forbidden-host-blocked", [&] {
            Mock m; mk(m); m.redirects["/dl/" + nm] = "http://evil.invalid/x";
            UpdateManager u("1.0.0", nullptr, QUrl(m.base() + api), "127.0.0.1");
            u.check(); waitFor(u, {UpdateManager::Available}); u.download();
            check(waitFor(u, {UpdateManager::Failed, UpdateManager::Ready}) && u.state() == UpdateManager::Failed, "forbidden redirect followed");
        });
        scenario("redirect-to-allowed-host-followed", [&] {
            Mock m; mk(m); m.routes["/real"] = {200, setup}; m.redirects["/dl/" + nm] = m.base() + "/real";
            UpdateManager u("1.0.0", nullptr, QUrl(m.base() + api), "127.0.0.1");
            u.check(); waitFor(u, {UpdateManager::Available}); u.download();
            check(waitFor(u, {UpdateManager::Failed, UpdateManager::Ready}) && u.state() == UpdateManager::Ready, "allowed redirect not followed");
        });
        scenario("production-rejects-plain-http-api", [&] {
            UpdateManager u("1.0.0", nullptr, QUrl("http://127.0.0.1:1/x"), QString());
            u.check(); check(u.state() == UpdateManager::Failed, "plain http api accepted");
        });
        scenario("cancel-returns-idle", [&] {
            Mock m; mk(m); UpdateManager u("1.0.0", nullptr, QUrl(m.base() + api), "127.0.0.1");
            u.check(); u.cancel(); check(u.state() == UpdateManager::Idle, "cancel did not reset");
        });
        std::printf("PASS update-manager\n");
        return 0;
    } catch (const std::exception &e) { std::fprintf(stderr, "FAIL update-manager: %s\n", e.what()); return 1; }
}
