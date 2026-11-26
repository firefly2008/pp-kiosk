// SPDX-License-Identifier: GPL-2.0-or-later
#include "apikeys.h"

#include <QFile>
#include <QImage>
#include <QNetworkReply>
#include <QPainter>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

#include "securestore.h"
#include "thirdparty/qrcodegen.hpp"

namespace {
ApiKeys *g_instance = nullptr;
}

ApiKeys::ApiKeys(const QString &dataDir, QObject *parent)
    : QObject(parent)
    , m_settings(dataDir + QStringLiteral("/settings.ini"), QSettings::IniFormat)
    , m_dataDir(dataDir)
{
    g_instance = this;
    m_unsplash = SecureStore::value(m_settings, QStringLiteral("keys/unsplash")).toString().trimmed();
}

ApiKeys *ApiKeys::instance()
{
    return g_instance;
}

QString ApiKeys::unsplashKey() const
{
    if (m_unsplash.isEmpty()) {
        return qEnvironmentVariable("BIGSCREEN_TEST_UNSPLASH").trimmed();
    }
    return m_unsplash;
}

QString ApiKeys::testTmdbKey()
{
    return qEnvironmentVariable("BIGSCREEN_TEST_TMDB").trimmed();
}

void ApiKeys::setUnsplashKey(const QString &key)
{
    m_unsplash = key.trimmed();
    SecureStore::setValue(m_settings, QStringLiteral("keys/unsplash"), m_unsplash);
    m_settings.sync();
    Q_EMIT changed();
}

QString ApiKeys::signupUrl(const QString &kind) const
{
    return kind == QLatin1String("tmdb") ? QStringLiteral("https://www.themoviedb.org/settings/api")
                                         : QStringLiteral("https://unsplash.com/oauth/applications");
}

void ApiKeys::test(const QString &kind, const QString &rawKey)
{
    const QString key = rawKey.trimmed();
    if (key.isEmpty()) {
        Q_EMIT tested(kind, false, tr("Vul eerst een sleutel in"));
        return;
    }
    QNetworkRequest request;
    if (kind == QLatin1String("tmdb")) {
        QUrl url(QStringLiteral("https://api.themoviedb.org/3/configuration"));
        if (key.size() > 40) {
            request.setRawHeader("Authorization", "Bearer " + key.toUtf8());
        } else {
            QUrlQuery q;
            q.addQueryItem(QStringLiteral("api_key"), key);
            url.setQuery(q);
        }
        request.setUrl(url);
    } else {
        QUrl url(QStringLiteral("https://api.unsplash.com/search/photos"));
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("query"), QStringLiteral("nature"));
        q.addQueryItem(QStringLiteral("per_page"), QStringLiteral("1"));
        url.setQuery(q);
        request.setUrl(url);
        request.setRawHeader("Authorization", "Client-ID " + key.toLatin1());
        request.setRawHeader("Accept-Version", "v1");
    }
    request.setTransferTimeout(15000);
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, kind] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 200 && status < 300) {
            Q_EMIT tested(kind, true, tr("Sleutel werkt"));
        } else if (status == 401 || status == 403) {
            Q_EMIT tested(kind, false, tr("Sleutel geweigerd: controleer of je hem goed hebt overgenomen"));
        } else if (status == 0) {
            Q_EMIT tested(kind, false, tr("Geen verbinding om te testen"));
        } else {
            Q_EMIT tested(kind, false, tr("De dienst gaf een fout (%1)").arg(status));
        }
    });
}

QUrl ApiKeys::qrFor(const QString &kind)
{
    const QString file = m_dataDir + QStringLiteral("/keys-qr-%1.png").arg(kind == QLatin1String("tmdb") ? QStringLiteral("tmdb") : QStringLiteral("unsplash"));
    if (!QFile::exists(file)) {
        const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(signupUrl(kind).toUtf8().constData(), qrcodegen::QrCode::Ecc::MEDIUM);
        const int border = 2;
        const int scale = 8;
        const int size = (qr.getSize() + border * 2) * scale;
        QImage image(size, size, QImage::Format_RGB32);
        image.fill(Qt::white);
        QPainter p(&image);
        for (int y = 0; y < qr.getSize(); ++y) {
            for (int x = 0; x < qr.getSize(); ++x) {
                if (qr.getModule(x, y)) {
                    p.fillRect((x + border) * scale, (y + border) * scale, scale, scale, Qt::black);
                }
            }
        }
        p.end();
        image.save(file);
    }
    return QUrl::fromLocalFile(file);
}
