// Gebruiksstatistiek: hoe vaak tegels, zenders, diensten en spelletjes gebruikt worden.
// Alleen lokaal opgeslagen (stats.json in de datamap), te zien en te wissen op de beheerpagina.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QTimer>

class Stats : public QObject
{
    Q_OBJECT

public:
    explicit Stats(const QString &dataDir, QObject *parent = nullptr);

    // kind: "tile", "radio", "stream", "game", "voice"; name: wat er gebruikt is
    Q_INVOKABLE void count(const QString &kind, const QString &name);
    Q_INVOKABLE int countOf(const QString &kind, const QString &name) const { return m_data.value(kind).toObject().value(name).toInt(); }

    QByteArray toJson() const;
    void clear();
    void flush();

private:
    QString m_file;
    QJsonObject m_data;
    QTimer m_saveTimer;
};
