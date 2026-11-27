// Radio: de zeven populairste Nederlandse radiozenders (NMO luisteronderzoek, 1e helft 2026).
// Zelfde rollen en functies als AppModel, zodat de rij op het startscherm gewoon werkt.
// Afspelen gebeurt in PP Kiosk zelf (RadioPlayer.qml, Qt Multimedia).
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QAbstractListModel>
#include <QNetworkAccessManager>
#include <QStringList>
#include <QVariantMap>

class RadioModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)            // zichtbare zenders
    Q_PROPERTY(int stationCount READ stationCount NOTIFY countChanged)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
    Q_PROPERTY(bool searching READ searching NOTIFY resultsChanged)
    Q_PROPERTY(int hiddenCount READ hiddenCount NOTIFY countChanged)
    // Programma-info van NPO-zenders (nporadioX.nl/api/broadcasts): nu en straks
    Q_PROPERTY(QVariantMap programme READ programme NOTIFY programmeChanged)
    // Lokale radio: deze regionale zender staat altijd vooraan (beheer: Media > Muziek)
    Q_PROPERTY(QString region READ region WRITE setRegion NOTIFY countChanged)

public:
    enum Roles { NameRole = Qt::UserRole + 1, IconRole, AccentRole, PathRole };

    explicit RadioModel(const QString &dataDir, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return int(m_visible.size()); }
    int stationCount() const { return int(m_visible.size()); }
    QVariantList results() const { return m_results; }
    bool searching() const { return m_searching; }
    int hiddenCount() const;
    int revision() const { return m_revision; }
    QVariantMap programme() const { return m_programme; }
    Q_INVOKABLE void requestProgramme(int row);
    QString region() const { return m_region; }
    void setRegion(const QString &name);
    // Namen van de regionale zenders (voor het beheer)
    static QStringList regionalNames();

    Q_INVOKABLE QString nameAt(int row) const;
    Q_INVOKABLE QString iconAt(int row) const;
    Q_INVOKABLE QString accentAt(int row) const;
    Q_INVOKABLE QString subtitleAt(int row) const;
    Q_INVOKABLE QStringList streamsAt(int row) const;
    Q_INVOKABLE bool hasCustomIcon(int) const { return false; }
    Q_INVOKABLE void move(int from, int to);
    // Vraagt de speler om deze zender te starten (playRequested); laatste tegel: addRequested
    Q_INVOKABLE bool launch(int row);
    Q_INVOKABLE bool isAddTile(int row) const { return row >= 0 && row < count() && m_visible.at(row) < 0; }
    Q_INVOKABLE bool isStation(int row) const { return row >= 0 && row < count() && m_visible.at(row) >= 0; }
    // Volgende/vorige zender (slaat de tegel "Radiozender toevoegen" over)
    Q_INVOKABLE int nextStation(int row, int step) const;
    Q_INVOKABLE QString pathAt(int row) const;
    Q_INVOKABLE int rowOfName(const QString &name) const;
    // Zoeken in Radio Browser (radio-browser.info, ruim 40.000 zenders)
    Q_INVOKABLE void searchStations(const QString &query);
    Q_INVOKABLE bool addResult(int index);
    Q_INVOKABLE bool hasResult(int index) const;
    Q_INVOKABLE void remove(int row);
    Q_INVOKABLE void restoreHidden();
    // Diensten kiezen: alle ingebouwde zenders met aan/uit, en de keuze opslaan
    Q_INVOKABLE QVariantList catalog() const;
    Q_INVOKABLE void setChosen(const QStringList &names);

Q_SIGNALS:
    void revisionChanged();
    void programmeChanged();
    void countChanged();
    void resultsChanged();
    void playRequested(int row);
    void addRequested();

private:
    QString m_region;
    struct Station {
        QString name;
        QString slogan;
        QString site;      // voor het logo
        QString accent;
        QStringList streams;
        bool custom = false;
        bool extra = false;  // uit de lijst met alle Nederlandse zenders: alleen als gekozen
        QString logoUrl;     // eigen zender: logo uit Radio Browser
    };
    const Station &st(int row) const { return m_stations.at(m_visible.at(row)); }
    QString logoFile(int row) const;
    QString logoFileFor(const Station &s) const;
    void fetchLogos();
    void loadCustom();
    void saveCustom() const;
    void saveOrder() const;
    void rebuild();

    QString m_dataDir;
    QList<Station> m_stations;
    QList<int> m_visible;
    QVariantList m_results;
    bool m_searching = false;
    QNetworkAccessManager m_network;
    int m_revision = 0;
    QVariantMap m_programme;
    int m_programmeToken = 0;
};
