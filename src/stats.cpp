// SPDX-License-Identifier: GPL-2.0-or-later
#include "stats.h"

#include <QDate>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>

Stats::Stats(const QString &dataDir, QObject *parent)
    : QObject(parent)
    , m_file(dataDir + QStringLiteral("/stats.json"))
{
    QFile f(m_file);
    if (f.open(QIODevice::ReadOnly)) {
        m_data = QJsonDocument::fromJson(f.readAll()).object();
    }
    if (!m_data.contains(QStringLiteral("since"))) {
        m_data.insert(QStringLiteral("since"), QDate::currentDate().toString(Qt::ISODate));
    }
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(5000);
    connect(&m_saveTimer, &QTimer::timeout, this, &Stats::flush);
}

void Stats::count(const QString &kind, const QString &name)
{
    static const QStringList kinds{QStringLiteral("tile"), QStringLiteral("radio"), QStringLiteral("stream"),
                                   QStringLiteral("game"), QStringLiteral("voice")};
    const QString n = name.trimmed().left(80);
    if (!kinds.contains(kind) || n.isEmpty()) {
        return;
    }
    QJsonObject group = m_data.value(kind).toObject();
    if (!group.contains(n) && group.size() >= 500) {
        return;     // niet onbeperkt groeien
    }
    group.insert(n, group.value(n).toInt() + 1);
    m_data.insert(kind, group);
    // Gebruik per dag (laatste 60 dagen)
    QJsonObject days = m_data.value(QStringLiteral("days")).toObject();
    const QString today = QDate::currentDate().toString(Qt::ISODate);
    days.insert(today, days.value(today).toInt() + 1);
    const QString oldest = QDate::currentDate().addDays(-60).toString(Qt::ISODate);
    for (const QString &k : days.keys()) {
        if (k < oldest) {
            days.remove(k);
        }
    }
    m_data.insert(QStringLiteral("days"), days);
    qInfo().noquote() << "Stats:" << kind << n << group.value(n).toInt();
    m_saveTimer.start();
}

QByteArray Stats::toJson() const
{
    return QJsonDocument(m_data).toJson(QJsonDocument::Compact);
}

void Stats::clear()
{
    m_data = QJsonObject{{QStringLiteral("since"), QDate::currentDate().toString(Qt::ISODate)}};
    flush();
}

void Stats::flush()
{
    m_saveTimer.stop();
    QSaveFile f(m_file);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(m_data).toJson(QJsonDocument::Indented));
        f.commit();
    }
}
