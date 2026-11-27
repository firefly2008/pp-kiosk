// Presentaties: elke map in %LOCALAPPDATA%\PPKiosk\presentaties is een presentatie.
// Dia's zijn de afbeeldingen in die map (op naam gesorteerd). Optioneel info.json:
//   { "title": "...", "interval": 10,
//     "captions": { "01.jpg": { "title": "...", "subtitle": "...", "badge": "..." } } }
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QFileSystemWatcher>
#include <QHash>
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

    // Beheer (via de beheerpagina)
    // Geüpload bestand (tijdelijk pad) toevoegen aan presentatie `name`. PowerPoint/PDF: origineel wordt
    // bewaard in de map "origineel" en omgezet naar dia's; afbeeldingen worden een extra dia.
    bool addUpload(const QString &name, const QString &fileName, const QString &tempPath, QString *error);
    bool remove(const QString &name);
    QString originalFile(const QString &name) const;   // leeg als er geen origineel is
    QByteArray toJson() const;                          // lijst voor de beheerpagina
    static QString cleanName(const QString &name);

Q_SIGNALS:
    void conversionFinished(const QString &name, bool ok, const QString &message);

    void changed();

private:
    void removeDemo();
    void convert(const QString &name);
    void convertWithLibreOffice(const QString &name, const QString &source);
    bool pdfToImages(const QString &pdf, const QString &outDir, QString *error);
    void finishConversion(const QString &name, const QString &outDir, const QString &message);
    QHash<QString, QString> m_status;   // map → "bezig" of foutmelding

    QString m_root;
    QVariantList m_list;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};
