// Dagstructuur: de activiteiten van vandaag (met pictogram, tijd en kleur) en mededelingen.
// Opgeslagen in %LOCALAPPDATA%\PPKiosk\agenda.json; aan te passen via de beheerpagina.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QDate>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class Agenda : public QObject
{
    Q_OBJECT
    // Activiteiten van vandaag, gesorteerd op tijd. Elk item: id, start, end, title, icon, color,
    // note en state ("past", "now", "next" of "later")
    Q_PROPERTY(QVariantList today READ today NOTIFY changed)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY changed)   // -1: nu geen activiteit
    Q_PROPERTY(int nextIndex READ nextIndex NOTIFY changed)         // -1: niets meer vandaag
    Q_PROPERTY(QStringList messages READ messages NOTIFY changed)
    // Instellingen van het scherm (voorlezen, tekstgrootte, contrast, screensaver, nachtstand, naam)
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY changed)
    // Eten van vandaag uit het weekmenu: dish, note, image (leeg als er niets is)
    Q_PROPERTY(QVariantMap todayMenu READ todayMenu NOTIFY changed)
    // Verjaardagen en bijzondere dagen van vandaag: title, icon
    Q_PROPERTY(QVariantList celebrations READ celebrations NOTIFY changed)
    // Dagdeel: "ochtend", "middag", "avond" of "nacht"
    Q_PROPERTY(QString daypart READ daypart NOTIFY changed)

public:
    explicit Agenda(const QString &dataDir, QObject *parent = nullptr);

    QVariantList today() const { return m_today; }
    int currentIndex() const { return m_current; }
    int nextIndex() const { return m_next; }
    QStringList messages() const;
    QVariantMap settings() const;
    QVariantMap todayMenu() const { return m_menu; }
    QVariantList celebrations() const { return m_celebrations; }
    QString daypart() const { return m_daypart; }
    QString pictoDir() const { return m_pictoDir; }
    // Weekmenu: 7 objecten (maandag eerst) met dish, note, image
    QJsonArray weekMenu() const { return m_data.value(QStringLiteral("menu")).toArray(); }
    // Activiteiten van een andere dag (weekoverzicht op de beheerpagina)
    QVariantList itemsFor(const QDate &date) const;
    // Weekoverzicht op de kiosk: vandaag en de 6 dagen erna. Per dag: date, items, dish, feest
    Q_INVOKABLE QVariantList week() const;

    // Voor de beheerpagina: hele agenda als JSON lezen en vervangen
    QByteArray toJson() const;
    bool setJson(const QByteArray &json, QString *error);

    // Rooktest en beheer: tijd vastzetten ("HH:mm"), leeg = echte tijd
    Q_INVOKABLE void setTestTime(const QString &time);
    // Alleen voor de rooktest: instelling tijdelijk veranderen (wordt niet opgeslagen)
    Q_INVOKABLE void setTestSetting(const QString &key, const QVariant &value);
    Q_INVOKABLE QString testPhoto();

Q_SIGNALS:
    void changed();
    // Een nieuwe activiteit is net begonnen (om voor te lezen)
    void activityStarted(const QString &title, const QString &note);

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
    QString m_pictoDir;
    QString m_currentId;
    bool m_first = true;
    QVariantMap m_menu;
    QVariantList m_celebrations;
    QString m_daypart;
    QTimer m_timer;
};
