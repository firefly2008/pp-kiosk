// SPDX-License-Identifier: GPL-2.0-or-later
#include "news.h"

#include <QDateTime>
#include <QDebug>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTextDocumentFragment>
#include <QXmlStreamReader>

namespace {
const char *feedUrl = "https://feeds.nos.nl/nosnieuwsalgemeen";

// Tekst van het bericht: HTML eruit, alinea's houden, hooguit ~900 tekens (past op één scherm)
QString plainText(const QString &html)
{
    QString text = QTextDocumentFragment::fromHtml(html).toPlainText();
    text.replace(QChar(0x2028), QLatin1Char('\n')).replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
    QStringList paras;
    int size = 0;
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        const QString p = line.simplified();
        if (p.isEmpty()) {
            continue;
        }
        if (size > 0 && size + p.size() > 900) {
            break;
        }
        paras.append(p);
        size += p.size();
    }
    return paras.join(QStringLiteral("\n\n"));
}
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
    QString description;
    QString image;
    bool inItem = false;
    while (!r.atEnd() && out.size() < max) {
        r.readNext();
        if (r.isStartElement()) {
            if (r.name() == QLatin1String("item") || r.name() == QLatin1String("entry")) {
                inItem = true;
                title.clear();
                date.clear();
                description.clear();
                image.clear();
            } else if (inItem && r.name() == QLatin1String("title")) {
                title = r.readElementText().simplified();
            } else if (inItem && (r.name() == QLatin1String("pubDate") || r.name() == QLatin1String("updated"))) {
                date = r.readElementText().trimmed();
            } else if (inItem && (r.name() == QLatin1String("description") || r.name() == QLatin1String("summary"))) {
                description = r.readElementText();
            } else if (inItem && r.name() == QLatin1String("enclosure") && image.isEmpty()) {
                const QString url = r.attributes().value(QLatin1String("url")).toString();
                if (url.startsWith(QLatin1String("https://"))) {
                    image = url;
                }
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
                {QStringLiteral("text"), plainText(description)},
                {QStringLiteral("image"), image},
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
        "<item><title>Eerste kop van vandaag</title><pubDate>Thu, 26 Nov 2026 19:30:00 +0100</pubDate>"
        "<description>&lt;p&gt;Dit is de eerste alinea van het bericht. Hier staat in een paar zinnen wat er is gebeurd, zodat je het rustig kunt lezen.&lt;/p&gt;"
        "&lt;p&gt;En dit is de tweede alinea, met &lt;b&gt;wat meer&lt;/b&gt; uitleg.&lt;/p&gt;</description></item>"
        "<item><title>Tweede kop</title><pubDate>Thu, 26 Nov 2026 18:05:00 +0100</pubDate></item>"
        "<item><title>Derde kop</title></item></channel></rss>"));
    qInfo() << "News:" << m_headlines.size() << "headlines";
    Q_EMIT changed();
}
