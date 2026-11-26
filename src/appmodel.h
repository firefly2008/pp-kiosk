// Lijst van geinstalleerde toepassingen (Startmenu-snelkoppelingen) plus de
// persoonlijke aanpassingen daarop: verborgen tegels, eigen pictogrammen en de
// achtergrond. Vervangt op Windows de KService/Kicker-lijst en de wallpaper-KCM.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "paths.h"

#include <QAbstractListModel>
#include <QColor>
#include <QHash>
#include <QList>
#include <QNetworkAccessManager>
#include <QSettings>
#include <QString>
#include <QUrl>
#include <QVariantMap>

// Pad naar MPC-HC (64-bit), of "mpc-hc64.exe" als het niet gevonden is
QString mpcHcPath();

class AppModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int hiddenCount READ hiddenCount NOTIFY countChanged)
    Q_PROPERTY(QString background READ background NOTIFY backgroundChanged)
    // Telt op zodra een pictogram (en daarmee een kleur) verandert, zodat QML opnieuw opvraagt
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    // Grote naam en logo linksboven op de startpagina
    Q_PROPERTY(bool showHero READ showHero WRITE setShowHero NOTIFY showHeroChanged)
    // Tekst groter (1.0 normaal, 1.25 groter) voor kijken op afstand
    Q_PROPERTY(double fontScale READ fontScale WRITE setFontScale NOTIFY showHeroChanged)
    // Welkomststap al gedaan? En is dit een nieuwe installatie (voor de voorkeuze van diensten)
    Q_PROPERTY(bool welcomeDone READ welcomeDone WRITE setWelcomeDone NOTIFY showHeroChanged)
    Q_PROPERTY(bool freshInstall READ freshInstall CONSTANT)
    // Volgorde van de rijen op de startpagina (titels), zelf te verplaatsen
    Q_PROPERTY(QStringList sectionOrder READ sectionOrder WRITE setSectionOrder NOTIFY sectionOrderChanged)

    // Web-tegels: welk tabblad zichtbaar is
    Q_PROPERTY(QString category READ category WRITE setCategory NOTIFY categoryChanged)

public:
    enum Roles { NameRole = Qt::UserRole + 1, PathRole, IconRole, CustomIconRole, AccentRole };
    // Apps: snelkoppelingen uit het Startmenu. Web: streamingdiensten en andere websites.
    enum Kind { Apps, Web };

    // category: bij web-tegels de rij (video, muziek, podcasts)
    explicit AppModel(Kind kind, const QString &category = QString(), QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return m_apps.size(); }
    int hiddenCount() const { return m_hidden.size(); }
    QString background() const { return m_background; }
    int revision() const { return m_revision; }
    QString category() const { return m_category; }
    void setCategory(const QString &category)
    {
        if (category != m_category) {
            m_category = category;
            refresh();
            Q_EMIT categoryChanged();
        }
    }
    bool showHero() const { return m_settings.value(QStringLiteral("showHero"), true).toBool(); }
    double fontScale() const { return m_settings.value(QStringLiteral("fontScale"), 1.0).toDouble(); }
    QStringList sectionOrder() const { return m_settings.value(key("sectionOrder")).toStringList(); }
    void setSectionOrder(const QStringList &order)
    {
        m_settings.setValue(key("sectionOrder"), order);
        Q_EMIT sectionOrderChanged();
    }
    // Media-rij: tegel "Nu op de radio" zolang de radio op de achtergrond speelt (leeg = weg)
    // Algemene instellingen per profiel (rijen inklappen, nachtmodus, radio hervatten, ...)
    Q_INVOKABLE QVariant setting(const QString &name, const QVariant &fallback = {}) const
    {
        return m_settings.value(profileKey(QStringLiteral("ui/") + name), fallback);
    }
    Q_INVOKABLE void setSetting(const QString &name, const QVariant &value)
    {
        m_settings.setValue(profileKey(QStringLiteral("ui/") + name), value);
        Q_EMIT settingsChanged();
    }
    // Media-rij: tegel "Verder kijken" naast Schilderij (alleen als er iets verder te kijken is)
    Q_INVOKABLE void setResumeCount(int count);
    Q_INVOKABLE void setNowPlaying(const QString &name, const QString &icon, const QString &accent);
    void setFontScale(double scale)
    {
        m_settings.setValue(QStringLiteral("fontScale"), scale);
        Q_EMIT showHeroChanged();
    }
    // Bestaande gebruikers krijgen de welkomststap niet (die is via Aanpassen terug te halen)
    bool welcomeDone() const { return m_settings.value(QStringLiteral("welcomeDone"), !m_freshInstall).toBool(); }
    void setWelcomeDone(bool done)
    {
        m_settings.setValue(QStringLiteral("welcomeDone"), done);
        Q_EMIT showHeroChanged();
    }
    bool freshInstall() const { return m_freshInstall; }
    Q_INVOKABLE bool tipShown(const QString &tip) const { return m_settings.value(QStringLiteral("tips/") + tip).toBool(); }
    Q_INVOKABLE void markTipShown(const QString &tip) { m_settings.setValue(QStringLiteral("tips/") + tip, true); }
    void setShowHero(bool show)
    {
        if (show != showHero()) {
            m_settings.setValue(QStringLiteral("showHero"), show);
            Q_EMIT showHeroChanged();
        }
    }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool launch(int row);
    Q_INVOKABLE bool openUri(const QString &uri);
    // Website schermvullend openen in Edge/Chrome (eigen profiel, Home-knop terug naar PP Kiosk)
    Q_INVOKABLE bool launchWeb(const QString &url);
    Q_INVOKABLE QString nameAt(int row) const;
    Q_INVOKABLE bool hasCustomIcon(int row) const;
    Q_INVOKABLE QString iconAt(int row) const;
    Q_INVOKABLE QString pathAt(int row) const { return data(index(row, 0), PathRole).toString(); }
    Q_INVOKABLE QColor accentAt(int row) const;

    // Tegels aanpassen
    Q_INVOKABLE void hide(int row);
    Q_INVOKABLE void move(int from, int to);
    Q_INVOKABLE bool addTile(const QString &name, const QString &target);
    Q_INVOKABLE void restoreHidden();
    // Beheerpagina: alle diensten van dit tabblad (name, url, on) en kiezen welke zichtbaar zijn
    QVariantList webCatalog() const;
    void setWebShown(const QStringList &urls);
    Q_INVOKABLE bool setCustomIcon(int row, const QUrl &imageFile);
    Q_INVOKABLE void clearCustomIcon(int row);

    // Achtergrond
    Q_INVOKABLE bool setBackground(const QUrl &imageFile);
    Q_INVOKABLE void clearBackground();

Q_SIGNALS:
    void sectionOrderChanged();
    void settingsChanged();
    void countChanged();
    void backgroundChanged();
    // Tegel die een scherm binnen Bigscreen opent (bigscreen:movies, bigscreen:shows)
    void internalRequested(const QString &target);
    void revisionChanged();
    void showHeroChanged();
    void categoryChanged();

private:
    struct App {
        QString name;
        QString path; // pad van snelkoppeling/programma, of URL bij een web-tegel
        bool custom = false; // door de gebruiker zelf toegevoegd
        QString category;    // bij web-tegels: video, muziek of podcasts
    };
    QString key(const char *name) const;
    QList<App> customTiles() const;
    void saveCustomTiles(const QList<App> &tiles);
    QString faviconFile(const QString &url) const;
    void fetchFavicons();

    QColor accentFor(const App &app) const;
    void applyOrder(QList<App> &tiles) const;
    void saveOrder();

    Kind m_kind;
    int m_resumeCount = 0;
    QString m_nowPlaying;
    QString m_nowPlayingIcon;
    QString m_nowPlayingAccent;
    mutable QHash<QString, QColor> m_accentCache; // sleutel: pictogrambestand
    QNetworkAccessManager m_network;
    QString importImage(const QUrl &source, const QString &baseName);

    QList<App> m_apps;
    QStringList m_hidden;
    QVariantMap m_customIcons; // pad van snelkoppeling -> file-URL van eigen pictogram
    QString m_background;
    int m_revision = 0;
    QString m_category = QStringLiteral("video");
    QSettings m_settings;
    bool m_freshInstall = false;
};
