// Berichtenbord: de begeleiding stuurt via de beheerpagina een kort bericht naar het scherm,
// en de praatkaart op het scherm kan een melding terugsturen ("Ik heb pijn").
// Bewaard in %LOCALAPPDATA%\PPKiosk\berichten.json (de laatste 60).
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QVariantMap>

class MessageBoard : public QObject
{
    Q_OBJECT
    // Het oudste nog niet gelezen bericht voor het scherm (leeg: niets)
    Q_PROPERTY(QVariantMap current READ current NOTIFY changed)

public:
    explicit MessageBoard(QObject *parent = nullptr);

    QVariantMap current() const;
    QJsonArray list() const { return m_items; }
    // Van de beheerpagina naar het scherm
    Q_INVOKABLE QJsonObject post(const QString &text, const QString &from);
    void clear();

    Q_INVOKABLE void markRead(const QString &id);
    // Van het scherm naar de begeleiding (praatkaart)
    Q_INVOKABLE void alert(const QString &text);

Q_SIGNALS:
    void changed();
    void arrived();   // nieuw bericht voor het scherm

private:
    void save() const;
    QJsonArray m_items;
};
