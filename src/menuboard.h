// Menubord: maakt uit het weekmenu automatisch dia's voor de presentatie "Menu van de week"
// (groot, kleurrijk, met foto, dag en tijd van het diner). Vandaag eerst, dan de komende dagen.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QImage>
#include <QObject>
#include <QString>

class Agenda;

class MenuBoard : public QObject
{
    Q_OBJECT
public:
    MenuBoard(Agenda *agenda, const QString &presentationsRoot, QObject *parent = nullptr);

    static QString folderName() { return QStringLiteral("Menu van de week"); }
    // Welke ingebouwde foto past bij dit gerecht (bijv. "vis", "pasta"); altijd een geldige naam
    static QString photoFor(const QString &dish, const QString &note);
    // Eén dia tekenen (1920 x 1080)
    static QImage render(const QString &dayLabel, const QString &dateText, const QString &dish, const QString &note,
                         const QString &time, const QImage &photo);
    // Zelfde dia in de stijl van het Glas-thema: donker, gloed en doorschijnende glaskaart
    static QImage renderGlass(const QString &dayLabel, const QString &dateText, const QString &dish, const QString &note,
                              const QString &time, const QImage &photo);

public Q_SLOTS:
    void update(bool force = false);

private:
    Agenda *m_agenda;
    QString m_dir;
    QString m_key;
};
