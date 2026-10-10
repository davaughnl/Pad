// SPDX-License-Identifier: GPL-3.0-or-later
// Lemon Squeezy license activation. Presentation and network only: it never touches mapping.
#ifndef PADLICENSE_H
#define PADLICENSE_H
#include <functional>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QHostInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QPushButton>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialog>

#ifndef PAD_LS_STORE_ID
#define PAD_LS_STORE_ID 0
#endif
#ifndef PAD_LS_PRODUCT_ID
#define PAD_LS_PRODUCT_ID 0
#endif
#ifndef PAD_BUY_URL
#define PAD_BUY_URL ""
#endif
#ifndef PAD_TEST_TOKEN_SHA256
#define PAD_TEST_TOKEN_SHA256 ""
#endif

struct PadLicenseResult
{
    enum Kind { Ok, Invalid, Limit, Expired, Disabled, WrongProduct, Network };
    Kind kind = Network;
    QString message;
    QString key;
    QString instanceId;
};

class PadLicense
{
public:
    explicit PadLicense(QNetworkAccessManager *net, qint64 storeId = PAD_LS_STORE_ID, qint64 productId = PAD_LS_PRODUCT_ID,
                        const QString &apiBase = QStringLiteral("https://api.lemonsqueezy.com"), const QString &cachePath = QString())
        : m_net(net), m_store(storeId), m_product(productId), m_api(apiBase),
          m_cache(cachePath.isEmpty() ? defaultCachePath() : cachePath) {}

    static QString defaultCachePath()
    {
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/license.json");
    }

    // True when this PC holds a saved activation. It does not mean the key is still valid.
    bool hasCached() { load(); return !m_savedKey.isEmpty() && !m_savedInstance.isEmpty(); }
    qint64 lastValidated() { load(); return m_savedWhen; }
    void clear() { QFile::remove(m_cache); m_savedKey.clear(); m_savedInstance.clear(); m_savedWhen = 0; }

    // Test-only bypass for release smoke tests: needs a secret token that matches a hash baked in at build time.
    static bool testBypass()
    {
        const QByteArray want = QByteArrayLiteral(PAD_TEST_TOKEN_SHA256);
        const QByteArray token = qgetenv("PAD_LICENSE_TEST_TOKEN");
        if (want.isEmpty() || token.isEmpty()) return false;
        return QCryptographicHash::hash(token, QCryptographicHash::Sha256).toHex() == want;
    }

    void activate(const QString &rawKey, std::function<void(PadLicenseResult)> done)
    {
        const QString key = rawKey.trimmed();
        if (key.isEmpty()) { PadLicenseResult r; r.kind = PadLicenseResult::Invalid; r.message = QObject::tr("Enter your license key."); done(r); return; }
        QString name = QHostInfo::localHostName(); if (name.isEmpty()) name = QStringLiteral("Windows PC");
        post(QStringLiteral("/v1/licenses/activate"), {{"license_key", key}, {"instance_name", name}}, key, QString(),
             [this, key, done](PadLicenseResult r) { if (r.kind == PadLicenseResult::Ok) save(key, r.instanceId); done(r); });
    }

    void validate(std::function<void(PadLicenseResult)> done)
    {
        load();
        const QString key = m_savedKey, instance = m_savedInstance;
        post(QStringLiteral("/v1/licenses/validate"), {{"license_key", key}, {"instance_id", instance}}, key, instance,
             [this, key, instance, done](PadLicenseResult r) { if (r.kind == PadLicenseResult::Ok) save(key, instance); done(r); });
    }

private:
    void load()
    {
        QFile f(m_cache); if (!f.open(QIODevice::ReadOnly)) return;
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        m_savedKey = o.value("key").toString(); m_savedInstance = o.value("instance_id").toString();
        m_savedWhen = static_cast<qint64>(o.value("validated_at").toDouble());
    }
    void save(const QString &key, const QString &instance)
    {
        QDir().mkpath(QFileInfo(m_cache).absolutePath());
        QSaveFile f(m_cache); if (!f.open(QIODevice::WriteOnly)) return;
        QJsonObject o; o["key"] = key; o["instance_id"] = instance; o["validated_at"] = static_cast<double>(QDateTime::currentSecsSinceEpoch());
        f.write(QJsonDocument(o).toJson()); f.commit();
        m_savedKey = key; m_savedInstance = instance; m_savedWhen = QDateTime::currentSecsSinceEpoch();
    }
    void post(const QString &path, const QList<QPair<QString, QString>> &fields, const QString &key, const QString &instance,
              std::function<void(PadLicenseResult)> done)
    {
        QNetworkRequest req(QUrl(m_api + path));
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
        req.setRawHeader("Accept", "application/json");
        req.setTransferTimeout(15000);
        QUrlQuery body; for (const auto &f : fields) body.addQueryItem(f.first, QString::fromLatin1(QUrl::toPercentEncoding(f.second)));
        QNetworkReply *reply = m_net->post(req, body.toString(QUrl::FullyEncoded).toUtf8());
        const qint64 store = m_store, product = m_product; const bool validating = path.endsWith("validate");
        QObject::connect(reply, &QNetworkReply::finished, reply, [=]() {
            PadLicenseResult r; r.key = key;
            const QByteArray data = reply->readAll(); reply->deleteLater();
            const QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isObject())
            {
                r.kind = PadLicenseResult::Network;
                r.message = QObject::tr("Could not reach the license server. Check your internet connection and try again.");
                done(r); return;
            }
            const QJsonObject o = doc.object();
            const bool ok = validating ? o.value("valid").toBool() : o.value("activated").toBool();
            const QString status = o.value("license_key").toObject().value("status").toString();
            const QJsonObject meta = o.value("meta").toObject();
            r.instanceId = o.value("instance").toObject().value("id").toString();
            if (validating) r.instanceId = instance;
            if (!ok)
            {
                const QString err = o.value("error").toString().toLower();
                if (status == QLatin1String("expired")) { r.kind = PadLicenseResult::Expired; r.message = QObject::tr("This license key has expired."); }
                else if (status == QLatin1String("disabled")) { r.kind = PadLicenseResult::Disabled; r.message = QObject::tr("This license key has been disabled."); }
                else if (err.contains("limit")) { r.kind = PadLicenseResult::Limit; r.message = QObject::tr("This key is already active on its maximum number of PCs. Deactivate one, or contact support."); }
                else { r.kind = PadLicenseResult::Invalid; r.message = QObject::tr("That license key was not recognised. Check it and try again."); }
                done(r); return;
            }
            if (store <= 0 || product <= 0 || meta.value("store_id").toVariant().toLongLong() != store
                || meta.value("product_id").toVariant().toLongLong() != product)
            {
                r.kind = PadLicenseResult::WrongProduct; r.message = QObject::tr("That key is for a different product.");
                done(r); return;
            }
            r.kind = PadLicenseResult::Ok; done(r);
        });
    }
    QNetworkAccessManager *m_net; qint64 m_store, m_product; QString m_api, m_cache;
    QString m_savedKey, m_savedInstance; qint64 m_savedWhen = 0;
};

// Activation screen in Pad's style. Accepts when the key activates; rejects on Quit.
inline QDialog *padCreateActivationDialog(QWidget *parent, PadLicense *license, const QString &buyUrl = QStringLiteral(PAD_BUY_URL))
{
    auto *dialog = new QDialog(parent);
    dialog->setObjectName("padActivation");
    dialog->setWindowTitle(QObject::tr("Activate Pad"));
    dialog->setFixedSize(480, 300);
    auto *root = new QVBoxLayout(dialog); root->setContentsMargins(24, 22, 24, 20); root->setSpacing(10);
    auto *title = new QLabel(QObject::tr("Activate Pad"), dialog); title->setStyleSheet("font-size: 20px; color: #f4f4f5;");
    auto *sub = new QLabel(QObject::tr("Enter the license key from your purchase email. Pad needs an internet connection once to activate. After that it works offline."), dialog);
    sub->setObjectName("padSubtitle"); sub->setWordWrap(true);
    auto *key = new QLineEdit(dialog); key->setObjectName("padLicenseKey"); key->setPlaceholderText(QStringLiteral("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX"));
    key->setClearButtonEnabled(false);
    auto *error = new QLabel(dialog); error->setObjectName("padLicenseError"); error->setWordWrap(true);
    error->setStyleSheet("color: #e08a8d; font-size: 12px;"); error->setMinimumHeight(34); error->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    root->addWidget(title); root->addWidget(sub); root->addSpacing(6); root->addWidget(key); root->addWidget(error); root->addStretch(1);
    auto *row = new QHBoxLayout(); row->setSpacing(8);
    auto *quit = new QPushButton(QObject::tr("Quit"), dialog); quit->setAutoDefault(false);
    quit->setIcon(QIcon(QStringLiteral(":/pad/icons/close.svg"))); quit->setIconSize(QSize(14, 14));
    auto *buy = new QPushButton(QObject::tr("Buy a license"), dialog); buy->setAutoDefault(false);
    buy->setIcon(QIcon(QStringLiteral(":/pad/icons/download.svg"))); buy->setIconSize(QSize(14, 14));
    buy->setVisible(!buyUrl.isEmpty());
    auto *go = new QPushButton(QObject::tr("Activate"), dialog); go->setDefault(true); go->setProperty("padPrimary", true);
    row->addWidget(quit); row->addWidget(buy); row->addStretch(1); row->addWidget(go);
    root->addLayout(row);
    QObject::connect(quit, &QPushButton::clicked, dialog, &QDialog::reject);
    QObject::connect(buy, &QPushButton::clicked, dialog, [buyUrl]() { QDesktopServices::openUrl(QUrl(buyUrl)); });
    QPointer<QDialog> guard(dialog);
    auto submit = [=]() {
        error->clear(); go->setEnabled(false); key->setEnabled(false); go->setText(QObject::tr("Activating"));
        license->activate(key->text(), [=](PadLicenseResult r) {
            if (!guard) return;
            go->setEnabled(true); key->setEnabled(true); go->setText(QObject::tr("Activate"));
            if (r.kind == PadLicenseResult::Ok) dialog->accept(); else { error->setText(r.message); key->setFocus(); key->selectAll(); }
        });
    };
    QObject::connect(go, &QPushButton::clicked, dialog, submit);
    QObject::connect(key, &QLineEdit::returnPressed, dialog, submit);
    key->setFocus();
    return dialog;
}

// Returns true when Pad may open. Shows the activation screen when this PC has no saved activation.
inline bool padLicenseGate(QNetworkAccessManager *net)
{
    if (PadLicense::testBypass()) return true;
    PadLicense license(net);
    if (license.hasCached()) return true;
    QDialog *dialog = padCreateActivationDialog(nullptr, &license);
    const bool ok = dialog->exec() == QDialog::Accepted;
    delete dialog;
    return ok;
}

// Quiet re-check at most once a week. A definite refusal clears the saved activation and closes Pad;
// being offline never locks anyone out.
inline void padLicenseRecheck(QNetworkAccessManager *net, QObject *owner)
{
    if (PadLicense::testBypass()) return;
    auto *license = new PadLicense(net);
    if (!license->hasCached() || QDateTime::currentSecsSinceEpoch() - license->lastValidated() < 7 * 24 * 3600) { delete license; return; }
    license->validate([license](PadLicenseResult r) {
        const bool refused = r.kind == PadLicenseResult::Invalid || r.kind == PadLicenseResult::Expired
                             || r.kind == PadLicenseResult::Disabled || r.kind == PadLicenseResult::WrongProduct;
        if (refused)
        {
            license->clear();
            QMetaObject::invokeMethod(qApp, []() { qApp->quit(); }, Qt::QueuedConnection);
        }
        delete license;
    });
    Q_UNUSED(owner);
}
#endif
