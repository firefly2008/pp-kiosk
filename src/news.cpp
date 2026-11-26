// SPDX-License-Identifier: GPL-2.0-or-later
#include "news.h"

#include <QDateTime>
#include <QDebug>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QXmlStreamReader>

namespace {
const char *feedUrl = "https://feeds.nos.nl/nosnieuwsalgemeen";
}

News::News(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(10 * 60 * 1000);
    connect(&m_timer, &QTimer::timeout, this, &News::refresh);
    m_timer.start();
}

QVariantList News::parse(const QByteArray &xml, int max)
{
    QVariantList out;
    QXmlStreamReader r(xml);
    QString title;
    QString date;
    bool inItem = false;
    while (!r.atEnd() && out.size() < max) {
        r.readNext();
        if (r.isStartElement()) {
            if (r.name() == QLatin1String("item") || r.name() == QLatin1String("entry")) {
                inItem = true;
                title.clear();
                date.clear();
            } else if (inItem && r.name() == QLatin1String("title")) {
                title = r.readElementText().simplified();
            } else if (inItem && (r.name() == QLatin1String("pubDate") || r.name() == QLatin1String("updated"))) {
                date = r.readElementText().trimmed();
            }
        } else if (r.isEndElement() && (r.name() == QLatin1String("item") || r.name() == QLatin1String("entry"))) {
            inItem = false;
            if (title.isEmpty()) {
                continue;
            }
            QDateTime when = QDateTime::fromString(date, Qt::RFC2822Date);
            if (!when.isValid()) {
                when = QDateTime::fromString(date, Qt::ISODate);
            }
            out.append(QVariantMap{
                {QStringLiteral("title"), title.left(200)},
                {QStringLiteral("time"), when.isValid() ? when.toLocalTime().toString(QStringLiteral("HH:mm")) : QString()},
            });
        }
    }
    return out;
}

void News::refresh()
{
    QNetworkRequest request{QUrl(QString::fromLatin1(feedUrl))};
    request.setTransferTimeout(20000);
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qInfo() << "News: not available" << reply->errorString();
            return;
        }
        const QVariantList list = parse(reply->readAll());
        if (!list.isEmpty()) {
            m_headlines = list;
            qInfo() << "News:" << list.size() << "headlines";
            Q_EMIT changed();
        }
    });
}

void News::loadSample()
{
    m_headlines = parse(QByteArrayLiteral(
        "<?xml version=\"1.0\"?><rss version=\"2.0\"><channel><title>NOS</title>"
        "<item><title>Eerste kop van vandaag</title><pubDate>Thu, 26 Nov 2026 19:30:00 +0100</pubDate></item>"
        "<item><title>Tweede kop</title><pubDate>Thu, 26 Nov 2026 18:05:00 +0100</pubDate></item>"
        "<item><title>Derde kop</title></item></channel></rss>"));
    qInfo() << "News:" << m_headlines.size() << "headlines";
    Q_EMIT changed();
}
