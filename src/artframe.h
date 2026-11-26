// Schilderij-modus: het scherm als lijst met kunst of natuurfoto's.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QVariantMap>
#include <functional>

class ArtFrame : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList sources READ sources CONSTANT)
    Q_PROPERTY(int source READ source WRITE setSource NOTIFY settingsChanged)
    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY settingsChanged)   // minuten
    Q_PROPERTY(bool showInfo READ showInfo WRITE setShowInfo NOTIFY settingsChanged)
    Q_PROPERTY(bool fill READ fill WRITE setFill NOTIFY settingsChanged)              // vullen of passe-partout
    Q_PROPERTY(QString pexelsKey READ pexelsKey WRITE setPexelsKey NOTIFY settingsChanged)
    Q_PROPERTY(QVariantMap current READ current NOTIFY currentChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY statusChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit ArtFrame(const QString &dataDir, QObject *parent = nullptr);

    QStringList sources() const;
    int source() const { return m_source; }
    void setSource(int source);
    int interval() const { return m_interval; }
    void setInterval(int minutes);
    bool showInfo() const { return m_showInfo; }
    void setShowInfo(bool show);
    bool fill() const { return m_fill; }
    void setFill(bool fill);
    QString pexelsKey() const { return m_pexelsKey; }
    void setPexelsKey(const QString &key);
    QVariantMap current() const { return m_history.value(m_position); }
    bool busy() const { return m_busy; }
    QString status() const { return m_status; }

    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void start();
    // Scherm niet laten uitgaan zolang het schilderij te zien is
    Q_INVOKABLE void keepAwake(bool on);

Q_SIGNALS:
    void settingsChanged();
    void currentChanged();
    void statusChanged();

private:
    using Callback = std::function<void(const QByteArray &, int)>;
    void get(const QUrl &url, const Callback &callback, const QByteArray &auth = {});
    void fetchBatch();
    void fetchMet();
    void fetchMetObject();
    void fetchCommons(bool paintings);
    void fetchPexels();
    void addCandidates(QList<QVariantMap> items);
    void showNextCandidate();
    void pruneCache();
    void setStatus(const QString &status, bool busy);
    void save();

    QNetworkAccessManager m_network;
    QSettings m_settings;
    int m_source = 0;
    int m_interval = 5;
    bool m_showInfo = true;
    bool m_fill = true;
    QString m_pexelsKey;

    QList<QVariantMap> m_queue;     // nog te tonen
    QList<QVariantMap> m_history;   // getoond (terugbladeren)
    int m_position = -1;
    QList<int> m_metIds;
    int m_generation = 0;           // verandert bij een andere bron: oude antwoorden negeren
    bool m_busy = false;
    bool m_waiting = false;
    bool m_downloading = false;
    int m_failures = 0;
    QString m_cacheDir;
    int m_retries = 0;
    QString m_status;
};
