// SPDX-License-Identifier: GPL-2.0-or-later
#include "messageboard.h"
#include "paths.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSaveFile>

namespace {
QString fileName() { return appDataDir() + QStringLiteral("/berichten.json"); }
QString newId() { return QString::number(QDateTime::currentMSecsSinceEpoch(), 36) + QString::number(QRandomGenerator::global()->bounded(1296), 36); }
}

MessageBoard::MessageBoard(QObject *parent)
    : QObject(parent)
{
    QFile f(fileName());
    if (f.open(QIODevice::ReadOnly)) {
        m_items = QJsonDocument::fromJson(f.readAll()).array();
    }
}

QVariantMap MessageBoard::current() const
{
    for (const QJsonValue &v : m_items) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("kind")).toString() == QLatin1String("screen") && !o.value(QStringLiteral("read")).toBool()) {
            return o.toVariantMap();
        }
    }
    return {};
}

QJsonObject MessageBoard::post(const QString &text, const QString &from)
{
    const QJsonObject o{
        {QStringLiteral("id"), newId()},
        {QStringLiteral("kind"), QStringLiteral("screen")},
        {QStringLiteral("text"), text.trimmed().left(300)},
        {QStringLiteral("from"), from.trimmed().left(40)},
        {QStringLiteral("time"), QDateTime::currentDateTime().toString(Qt::ISODate)},
        {QStringLiteral("read"), false},
    };
    m_items.append(o);
    while (m_items.size() > 60) {
        m_items.removeFirst();
    }
    save();
    qInfo() << "Board: message for the screen";
    Q_EMIT changed();
    Q_EMIT arrived();
    return o;
}

void MessageBoard::clear()
{
    m_items = {};
    save();
    Q_EMIT changed();
}

void MessageBoard::markRead(const QString &id)
{
    for (int i = 0; i < m_items.size(); ++i) {
        QJsonObject o = m_items.at(i).toObject();
        if (o.value(QStringLiteral("id")).toString() == id && !o.value(QStringLiteral("read")).toBool()) {
            o.insert(QStringLiteral("read"), true);
            o.insert(QStringLiteral("readAt"), QDateTime::currentDateTime().toString(Qt::ISODate));
            m_items.replace(i, o);
            save();
            qInfo() << "Board: read";
            Q_EMIT changed();
            return;
        }
    }
}

void MessageBoard::alert(const QString &text)
{
    m_items.append(QJsonObject{
        {QStringLiteral("id"), newId()},
        {QStringLiteral("kind"), QStringLiteral("alert")},
        {QStringLiteral("text"), text.trimmed().left(120)},
        {QStringLiteral("time"), QDateTime::currentDateTime().toString(Qt::ISODate)},
    });
    while (m_items.size() > 60) {
        m_items.removeFirst();
    }
    save();
    qInfo().noquote() << "Board: alert from the screen:" << text;
    Q_EMIT changed();
}

void MessageBoard::save() const
{
    QSaveFile f(fileName());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(m_items).toJson(QJsonDocument::Compact));
        f.commit();
    }
}
