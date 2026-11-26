// Presentaties: elke map in %LOCALAPPDATA%\PPKiosk\presentaties is een presentatie.
// Dia's zijn de afbeeldingen in die map (op naam gesorteerd). Optioneel info.json:
//   { "title": "...", "interval": 10,
//     "captions": { "01.jpg": { "title": "...", "subtitle": "...", "badge": "..." } } }
// Bij de eerste start komt er een voorbeeld ("Diner van de dag").
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <QVariantList>

class Presentations : public QObject
{
    Q_OBJECT
    // Elk item: folder, title, count, cover (file-url van de eerste dia), interval
    Q_PROPERTY(QVariantList list READ list NOTIFY changed)

public:
    explicit Presentations(const QString &dataDir, QObject *parent = nullptr);

    QVariantList list() const { return m_list; }
    QString root() const { return m_root; }

    // Dia's van een presentatie: source (file-url), title, subtitle, badge
    Q_INVOKABLE QVariantList slides(const QString &folder) const;
    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void changed();

private:
    void installDemo();

    QString m_root;
    QVariantList m_list;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};
