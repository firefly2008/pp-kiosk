// SPDX-License-Identifier: GPL-2.0-or-later
#include "shortcuts.h"

#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUrl>

namespace {
QJsonObject sc(const char *group, const char *name, const char *url, const char *icon, const char *color)
{
    return QJsonObject{
        {QStringLiteral("group"), QString::fromUtf8(group)},
        {QStringLiteral("name"), QString::fromUtf8(name)},
        {QStringLiteral("url"), QString::fromLatin1(url)},
        {QStringLiteral("icon"), QString::fromUtf8(icon)},
        {QStringLiteral("color"), QString::fromLatin1(color)},
    };
}
} // namespace

QJsonArray Shortcuts::defaults()
{
    return QJsonArray{
        sc("Tv", "NPO Start", "https://npo.nl/start", "📺", "#FF6D00"),
        sc("Tv", "NOS", "https://nos.nl/live", "📰", "#E4002B"),
        sc("Tv", "Videoland (RTL)", "https://www.videoland.com", "🎬", "#E50914"),
        sc("Tv", "KIJK (SBS)", "https://www.kijk.nl", "📡", "#1E88E5"),
        sc("Tv", "NLZIET", "https://app.nlziet.nl", "📺", "#2E3A8C"),
        sc("Tv", "Omroep MAX", "https://www.omroepmax.nl", "👵", "#E91E63"),
        sc("Tv", "Zapp (kinderen)", "https://www.zapp.nl", "🧒", "#00A8E1"),
        sc("Tv", "Schooltv", "https://schooltv.nl", "🎒", "#43A047"),
        sc("Radio", "NPO Luister", "https://www.nporadio1.nl/live", "📻", "#5E35B1"),
        sc("Radio", "NPO Radio 2", "https://www.nporadio2.nl/live", "🎶", "#C62828"),
        sc("Radio", "Qmusic", "https://qmusic.nl", "🎵", "#D81B60"),
        sc("Radio", "Radio 538", "https://www.538.nl", "🎧", "#283593"),
        sc("Online", "YouTube", "https://www.youtube.com", "▶️", "#FF0000"),
        sc("Online", "Spotify", "https://open.spotify.com", "🎵", "#1DB954"),
        sc("Online", "Buienradar", "https://www.buienradar.nl", "🌦️", "#0277BD"),
        sc("Online", "NU.nl", "https://www.nu.nl", "🗞️", "#C2185B"),
    };
}

Shortcuts::Shortcuts(const QString &dataDir, QObject *parent)
    : QObject(parent)
    , m_file(dataDir + QStringLiteral("/snelkoppelingen.json"))
{
    QFile f(m_file);
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isObject()) {
            m_items = doc.object().value(QStringLiteral("items")).toArray();
            return;
        }
    }
    m_items = defaults();
    save();
}

QVariantList Shortcuts::list() const
{
    return m_items.toVariantList();
}

QByteArray Shortcuts::toJson() const
{
    return QJsonDocument(QJsonObject{{QStringLiteral("items"), m_items}}).toJson(QJsonDocument::Compact);
}

void Shortcuts::save() const
{
    QSaveFile f(m_file);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(QJsonObject{{QStringLiteral("items"), m_items}}).toJson());
        f.commit();
    }
}

bool Shortcuts::setJson(const QByteArray &json, QString *error)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        *error = QStringLiteral("Geen geldige gegevens");
        return false;
    }
    QJsonArray items;
    for (const QJsonValue &v : doc.object().value(QStringLiteral("items")).toArray()) {
        const QJsonObject o = v.toObject();
        const QString name = o.value(QStringLiteral("name")).toString().trimmed().left(40);
        QString url = o.value(QStringLiteral("url")).toString().trimmed();
        if (!url.isEmpty() && !url.contains(QStringLiteral("://"))) {
            url.prepend(QStringLiteral("https://"));
        }
        const QUrl u(url);
        if (name.isEmpty() || !u.isValid() || (u.scheme() != QLatin1String("https") && u.scheme() != QLatin1String("http"))) {
            *error = QStringLiteral("Snelkoppeling \"%1\": vul een naam en een webadres in.").arg(name);
            return false;
        }
        QString color = o.value(QStringLiteral("color")).toString();
        if (!QRegularExpression(QStringLiteral("^#[0-9A-Fa-f]{6}$")).match(color).hasMatch()) {
            color = QStringLiteral("#3A4A6B");
        }
        items.append(QJsonObject{
            {QStringLiteral("group"), o.value(QStringLiteral("group")).toString().trimmed().left(30)},
            {QStringLiteral("name"), name},
            {QStringLiteral("url"), u.toString()},
            {QStringLiteral("icon"), o.value(QStringLiteral("icon")).toString().left(16)},
            {QStringLiteral("color"), color},
        });
        if (items.size() >= 60) {
            break;
        }
    }
    m_items = items;
    save();
    qInfo() << "Shortcuts: saved" << items.size();
    Q_EMIT changed();
    return true;
}
