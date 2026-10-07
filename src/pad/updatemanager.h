// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PAD_UPDATEMANAGER_H
#define PAD_UPDATEMANAGER_H

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>
#include <QVector>
#include <functional>

class QFile;
class QNetworkAccessManager;
class QNetworkReply;

// Checks the latest published GitHub release, downloads the Windows installer,
// verifies its SHA-256 and hands it to a launcher. Nothing unverified is run.
class UpdateManager : public QObject
{
    Q_OBJECT
  public:
    enum State { Idle, Checking, UpToDate, Available, Downloading, Verifying, Ready, Installing, Failed };
    Q_ENUM(State)

    struct Release
    {
        QString version;   // tag without a leading "v"
        QUrl setupUrl;
        QUrl hashUrl;
        QString setupName;
        qint64 size = 0;
    };

    // currentVersion is empty for a development build. testHost permits plain
    // HTTP to that one host (tests only; production passes an empty string).
    explicit UpdateManager(const QString &currentVersion, QObject *parent = nullptr,
                           const QUrl &apiUrl = defaultApiUrl(), const QString &testHost = QString());
    ~UpdateManager() override;

    static QUrl defaultApiUrl();
    static bool parseVersion(const QString &text, QVector<int> &core, QString &prerelease);
    // <0, 0, >0. An empty or unparsable "current" is older than any release.
    static int compareVersions(const QString &current, const QString &candidate);
    static bool parseRelease(const QByteArray &json, Release *out, QString *error);
    static QString parseSha256(const QByteArray &text, const QString &fileName);
    static bool hostAllowed(const QUrl &url, const QString &testHost);
    static constexpr qint64 maxInstallerBytes = 300LL * 1024 * 1024;

    State state() const { return m_state; }
    const Release &release() const { return m_release; }
    QString errorText() const { return m_error; }
    QString installerPath() const { return m_installerPath; }
    bool canInstallHere() const;
    void setInstallerLauncher(std::function<bool(const QString &)> launcher) { m_launcher = std::move(launcher); }

  public slots:
    void check();
    void download();
    void install();
    void cancel();

  signals:
    void stateChanged(UpdateManager::State state);
    void progress(int percent);
    void quitRequested();

  private:
    void setState(State state);
    void fail(const QString &message);
    QNetworkReply *get(const QUrl &url, bool binary);
    void onCheckFinished();
    void onHashFinished();
    void onDownloadFinished();
    void startSetupDownload();
    QString downloadDir() const;

    QString m_current;
    QUrl m_apiUrl;
    QString m_testHost;
    QNetworkAccessManager *m_net;
    QPointer<QNetworkReply> m_reply;
    QFile *m_file = nullptr;
    QByteArray m_hashBytes;
    State m_state = Idle;
    Release m_release;
    QString m_error;
    QString m_expectedHash;
    QString m_installerPath;
    std::function<bool(const QString &)> m_launcher;
};

#endif
