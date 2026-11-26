// Dagstructuur: de activiteiten van vandaag (met pictogram, tijd en kleur) en mededelingen.
// Opgeslagen in %LOCALAPPDATA%\PPKiosk\agenda.json; aan te passen via de beheerpagina.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QVariantList>

class Agenda : public QObject
{
    Q_OBJECT
    // Activiteiten van vandaag, gesorteerd op tijd. Elk item: id, start, end, title, icon, color,
    // note en state ("past", "now", "next" of "later")
    Q_PROPERTY(QVariantList today READ today NOTIFY changed)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY changed)   // -1: nu geen activiteit
    Q_PROPERTY(int nextIndex READ nextIndex NOTIFY changed)         // -1: niets meer vandaag
    Q_PROPERTY(QStringList messages READ messages NOTIFY changed)

public:
    explicit Agenda(const QString &dataDir, QObject *parent = nullptr);

    QVariantList today() const { return m_today; }
    int currentIndex() const { return m_current; }
    int nextIndex() const { return m_next; }
    QStringList messages() const;

    // Voor de beheerpagina: hele agenda als JSON lezen en vervangen
    QByteArray toJson() const;
    bool setJson(const QByteArray &json, QString *error);

    // Rooktest en beheer: tijd vastzetten ("HH:mm"), leeg = echte tijd
    Q_INVOKABLE void setTestTime(const QString &time);

Q_SIGNALS:
    void changed();

private:
    void load();
    void save() const;
    void recompute();
    static QJsonObject defaultAgenda();

    QString m_file;
    QJsonObject m_data;
    QVariantList m_today;
    int m_current = -1;
    int m_next = -1;
    QString m_stamp;     // datum + minuut van de laatste berekening
    QString m_testTime;
    QTimer m_timer;
};
