// Bibliotheek met films en series uit de eigen mappen van de gebruiker.
//
// Werkt zoals de Kodi-scrapers metadata.themoviedb.org.python (films) en
// metadata.tvshows.themoviedb.org.python (series): bestandsnaam opschonen met de
// standaardregels van Kodi, zoeken op titel en jaar bij TMDB, dan de details met
// cast in een aanroep (append_to_response), in het Nederlands met Engelse
// terugval. Die add-ons zijn Python voor Kodi's eigen runtime en bevatten Kodi's
// eigen API-sleutel; daarom is de werkwijze hier in C++ nagebouwd en gebruikt de
// gebruiker een eigen (gratis) TMDB-sleutel. Zonder sleutel komen series van
// TVmaze (geen sleutel nodig) en tonen films alleen hun titel.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

#include <functional>

class QNetworkRequest;

class MediaLibrary : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString moviesFolder READ moviesFolder NOTIFY settingsChanged)
    Q_PROPERTY(QString showsFolder READ showsFolder NOTIFY settingsChanged)
    // Meerdere mappen/locaties per soort (bijvoorbeeld een interne schijf en een NAS)
    Q_PROPERTY(QStringList moviesFolders READ moviesFolders NOTIFY settingsChanged)
    Q_PROPERTY(QStringList showsFolders READ showsFolders NOTIFY settingsChanged)
    Q_PROPERTY(QString apiKey READ apiKey NOTIFY settingsChanged)          // eigen sleutel (leeg = ingebouwde)
    Q_PROPERTY(bool configured READ configured NOTIFY settingsChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY statusChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QVariantList movies READ movies NOTIFY itemsChanged)
    Q_PROPERTY(QVariantList shows READ shows NOTIFY itemsChanged)

public:
    explicit MediaLibrary(const QString &dataDir, QObject *parent = nullptr);

    QString moviesFolder() const;
    QString showsFolder() const;
    QStringList moviesFolders() const;
    QStringList showsFolders() const;
    QString apiKey() const { return m_userKey; }
    // Ontgrendelen met de pincode; daarna is de sleutel te zien en te wijzigen
    Q_INVOKABLE void setApiKey(const QString &key);   // eigen TMDB-sleutel (leeg = geen)
    bool configured() const { return !m_moviesFolders.isEmpty() || !m_showsFolders.isEmpty(); }
    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    QVariantList movies() const { return m_movies; }
    QVariantList shows() const { return m_shows; }

    Q_INVOKABLE void configure(const QString &moviesFolder, const QString &showsFolder, const QString &apiKey);
    Q_INVOKABLE void configureFolders(const QStringList &moviesFolders, const QStringList &showsFolders, const QString &apiKey);
    Q_INVOKABLE void scan(bool refreshAll = false);
    Q_INVOKABLE bool play(const QString &path);
    Q_INVOKABLE bool playEpisodes(const QStringList &paths);
    Q_INVOKABLE QString localPath(const QUrl &url) const;

Q_SIGNALS:
    void settingsChanged();
    void statusChanged();
    void itemsChanged();

private:
    using JsonCallback = std::function<void(const QJsonDocument &, int httpStatus)>;
    void getJson(QNetworkRequest request, const JsonCallback &callback);
    void tmdb(const QString &path, const QList<QPair<QString, QString>> &query, const JsonCallback &callback);

    void next();
    void finish(const QString &key, QJsonObject entry, const QString &source);
    void scrapeMovie(const QString &key, bool withYear);
    void scrapeShowTmdb(const QString &key, bool withYear);
    void scrapeShowTvmaze(const QString &key);
    void keyRejected();
    void publish();
    void save();
    void setStatus(const QString &status, bool busy);
    QString summary() const;

    QString m_dataDir;
    QSettings m_settings;
    QNetworkAccessManager m_network;
    QStringList m_moviesFolders;
    QStringList m_showsFolders;
    QString m_apiKey;       // de sleutel die gebruikt wordt
    QString m_userKey;      // zelf ingevulde sleutel (leeg: de ingebouwde)
    QJsonObject m_cache; // sleutel: pad van film of seriemap
    QStringList m_queue;
    int m_total = 0;
    bool m_busy = false;
    bool m_rescanPending = false;
    bool m_keyRejected = false;
    QString m_status;
    QVariantList m_movies;
    QVariantList m_shows;
    QTimer m_publishTimer;
};
