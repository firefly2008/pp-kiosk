// Eigen sleutels van de gebruiker voor gratis diensten (TMDB voor filmgegevens, Unsplash voor foto's).
// Er zit geen sleutel meer in het programma: iedereen vult zijn eigen sleutel in (Instellingen → Sleutels).
// Opslag versleuteld met Windows DPAPI (zie securestore.h); nooit in het logbestand.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QUrl>

class ApiKeys : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool unsplashSet READ unsplashSet NOTIFY changed)
    Q_PROPERTY(QString unsplashKey READ unsplashKey NOTIFY changed)
public:
    explicit ApiKeys(const QString &dataDir, QObject *parent = nullptr);
    static ApiKeys *instance();

    // Unsplash-sleutel (leeg = geen). Alleen voor tests: BIGSCREEN_TEST_UNSPLASH
    QString unsplashKey() const;
    bool unsplashSet() const { return !unsplashKey().isEmpty(); }
    // TMDB-sleutel alleen voor tests: BIGSCREEN_TEST_TMDB (de echte staat in MediaLibrary)
    static QString testTmdbKey();

    Q_INVOKABLE void setUnsplashKey(const QString &key);
    // kind: "tmdb" of "unsplash". Antwoord via tested()
    Q_INVOKABLE void test(const QString &kind, const QString &key);
    // Adres waar de sleutel wordt aangemaakt
    Q_INVOKABLE QString signupUrl(const QString &kind) const;
    // QR-code (PNG) van dat adres, om het met de telefoon te openen
    Q_INVOKABLE QUrl qrFor(const QString &kind);

Q_SIGNALS:
    void changed();
    void tested(const QString &kind, bool ok, const QString &message);

private:
    QSettings m_settings;
    QNetworkAccessManager m_network;
    QString m_unsplash;
    QString m_dataDir;
};
