// SPDX-License-Identifier: GPL-2.0-or-later
#include "photosearch.h"
#include "apikeys.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>

PhotoSearch::PhotoSearch(const QString &dataDir, QObject *parent)
    : QObject(parent)
    , m_dataDir(dataDir)
{
}

QString PhotoSearch::source() const
{
    return (ApiKeys::instance() ? ApiKeys::instance()->unsplashKey() : QString()).isEmpty() ? QStringLiteral("Openverse") : QStringLiteral("Unsplash");
}

void PhotoSearch::search(const QString &query)
{
    searchWith(query, !(ApiKeys::instance() ? ApiKeys::instance()->unsplashKey() : QString()).isEmpty());
}

void PhotoSearch::searchWith(const QString &query, bool useUnsplash)
{
    const QString q = query.trimmed();
    const int generation = ++m_generation;
    if (q.size() < 2) {
        m_results.clear();
        m_busy = false;
        Q_EMIT resultsChanged();
        return;
    }
    const QString key = useUnsplash ? (ApiKeys::instance() ? ApiKeys::instance()->unsplashKey() : QString()) : QString();
    QNetworkRequest request;
    if (!key.isEmpty()) {
        QUrl url(QStringLiteral("https://api.unsplash.com/search/photos"));
        QUrlQuery params;
        params.addQueryItem(QStringLiteral("query"), q);
        params.addQueryItem(QStringLiteral("orientation"), QStringLiteral("landscape"));
        params.addQueryItem(QStringLiteral("per_page"), QStringLiteral("30"));
        params.addQueryItem(QStringLiteral("lang"), QStringLiteral("nl"));
        url.setQuery(params);
        request.setUrl(url);
        request.setRawHeader("Authorization", "Client-ID " + key.toLatin1());
        request.setRawHeader("Accept-Version", "v1");
    } else {
        QUrl url(QStringLiteral("https://api.openverse.org/v1/images/"));
        QUrlQuery params;
        params.addQueryItem(QStringLiteral("q"), q);
        params.addQueryItem(QStringLiteral("aspect_ratio"), QStringLiteral("wide"));
        params.addQueryItem(QStringLiteral("size"), QStringLiteral("large"));
        params.addQueryItem(QStringLiteral("category"), QStringLiteral("photograph"));
        params.addQueryItem(QStringLiteral("page_size"), QStringLiteral("20"));
        url.setQuery(params);
        request.setUrl(url);
    }
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Kijkluister"));
    m_busy = true;
    Q_EMIT resultsChanged();
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, q, unsplash = !key.isEmpty()] {
        reply->deleteLater();
        if (generation != m_generation) {
            return;   // er is intussen verder getypt
        }
        m_busy = false;
        m_results.clear();
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        if (unsplash) {
            for (const QJsonValue &v : o.value(QStringLiteral("results")).toArray()) {
                const QJsonObject p = v.toObject();
                const QJsonObject urls = p.value(QStringLiteral("urls")).toObject();
                const QString alt = p.value(QStringLiteral("alt_description")).toString();
                const QString photographer = p.value(QStringLiteral("user")).toObject().value(QStringLiteral("name")).toString();
                m_results.append(QVariantMap{
                    {QStringLiteral("name"), alt.isEmpty() ? q : alt},
                    {QStringLiteral("detail"), tr("Foto: %1 (Unsplash)").arg(photographer)},
                    {QStringLiteral("thumb"), urls.value(QStringLiteral("small")).toString()},
                    {QStringLiteral("full"), urls.value(QStringLiteral("raw")).toString() + QStringLiteral("&w=3840&fm=jpg&q=85")},
                    {QStringLiteral("credit"), tr("Foto: %1 op Unsplash").arg(photographer)},
                    // Unsplash vraagt een melding bij elke download
                    {QStringLiteral("track"), p.value(QStringLiteral("links")).toObject().value(QStringLiteral("download_location")).toString()},
                });
            }
        } else {
            for (const QJsonValue &v : o.value(QStringLiteral("results")).toArray()) {
                const QJsonObject p = v.toObject();
                const QString creator = p.value(QStringLiteral("creator")).toString();
                const QString license = p.value(QStringLiteral("license")).toString().toUpper();
                m_results.append(QVariantMap{
                    {QStringLiteral("name"), p.value(QStringLiteral("title")).toString()},
                    {QStringLiteral("detail"), tr("Foto: %1 · CC %2").arg(creator, license)},
                    {QStringLiteral("thumb"), p.value(QStringLiteral("thumbnail")).toString()},
                    {QStringLiteral("full"), p.value(QStringLiteral("url")).toString()},
                    {QStringLiteral("credit"), tr("Foto: %1 (CC %2, via Openverse)").arg(creator, license)},
                });
            }
        }
        qInfo() << "Photo search" << q << m_results.size() << (unsplash ? "Unsplash" : "Openverse");
        if (unsplash && m_results.isEmpty()) {
            // Unsplash gaf niets (bv. ongeldige sleutel): dan Openverse
            searchWith(q, false);
            return;
        }
        Q_EMIT resultsChanged();
    });
}

void PhotoSearch::download(int index)
{
    if (index < 0 || index >= m_results.size()) {
        return;
    }
    const QVariantMap r = m_results.at(index).toMap();
    QNetworkRequest request(QUrl(r.value(QStringLiteral("full")).toString()));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Kijkluister"));
    const QString credit = r.value(QStringLiteral("credit")).toString();
    const QString track = r.value(QStringLiteral("track")).toString();
    if (!track.isEmpty() && !(ApiKeys::instance() ? ApiKeys::instance()->unsplashKey() : QString()).isEmpty()) {
        QNetworkRequest t{QUrl(track)};
        t.setRawHeader("Authorization", "Client-ID " + (ApiKeys::instance() ? ApiKeys::instance()->unsplashKey() : QString()).toLatin1());
        QNetworkReply *ping = m_network.get(t);
        connect(ping, &QNetworkReply::finished, ping, &QObject::deleteLater);
    }
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, credit] {
        reply->deleteLater();
        QImage image;
        if (reply->error() != QNetworkReply::NoError || !image.loadFromData(reply->readAll()) || image.width() < 800) {
            qInfo() << "Photo download failed" << reply->errorString();
            Q_EMIT failed(tr("De foto kon niet worden opgehaald."));
            return;
        }
        if (image.width() > 3840) {
            image = image.scaledToWidth(3840, Qt::SmoothTransformation);
        }
        QDir().mkpath(m_dataDir + QStringLiteral("/backgrounds"));
        const QString name = QString::fromLatin1(QCryptographicHash::hash(reply->url().toString().toUtf8(), QCryptographicHash::Md5).toHex().left(12));
        const QString file = m_dataDir + QStringLiteral("/backgrounds/foto-") + name + QStringLiteral(".jpg");
        image.save(file, "JPG", 90);
        qInfo() << "Photo saved" << file << image.size();
        Q_EMIT downloaded(QUrl::fromLocalFile(file), credit);
    });
}
