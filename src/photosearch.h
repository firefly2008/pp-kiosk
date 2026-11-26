// Achtergrond wijzigen → Gratis foto zoeken. Unsplash (met ingebouwde sleutel) of, zonder sleutel,
// Openverse (vrij gelicenseerde foto's). De gekozen foto wordt in %LOCALAPPDATA%\Kijkluister\backgrounds bewaard.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QVariantList>

class PhotoSearch : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY resultsChanged)
    Q_PROPERTY(QString source READ source CONSTANT)   // "Unsplash" of "Openverse"

public:
    explicit PhotoSearch(const QString &dataDir, QObject *parent = nullptr);

    QVariantList results() const { return m_results; }
    bool busy() const { return m_busy; }
    QString source() const;

    Q_INVOKABLE void search(const QString &query);
    // Groot formaat ophalen; daarna downloaded(bestand) of failed()
    Q_INVOKABLE void download(int index);

Q_SIGNALS:
    void resultsChanged();
    void downloaded(const QUrl &file, const QString &credit);
    void failed(const QString &message);

private:
    void searchWith(const QString &query, bool useUnsplash);
    QString m_dataDir;
    QNetworkAccessManager m_network;
    QVariantList m_results;
    bool m_busy = false;
    int m_generation = 0;
};
