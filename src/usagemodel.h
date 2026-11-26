// Rij "Vaak gebruikt": de tegels die je het meest (en het laatst) opent, per profiel.
// Elke tegel verwijst naar een tegel in een andere rij (categorie + pad); pictogram, kleur en
// naam komen daar vandaan. Ook: laatst geopende tegel (opstarten waar je was) en hoe lang een rij
// niet gebruikt is (rijen inklappen).
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "paths.h"

#include <QAbstractListModel>
#include <QHash>
#include <QSettings>

class UsageModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(QString lastCategory READ lastCategory NOTIFY countChanged)
    Q_PROPERTY(QString lastPath READ lastPath NOTIFY countChanged)

public:
    enum Roles { NameRole = Qt::UserRole + 1, IconRole, AccentRole, PathRole };

    explicit UsageModel(QObject *parent = nullptr);

    // Bronnen per categorie ("media", "video", "muziek", "podcasts", "nieuws", "radio")
    void addSource(const QString &category, QObject *model);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return int(m_rows.size()); }
    int revision() const { return m_revision; }
    QString lastCategory() const;
    QString lastPath() const;

    Q_INVOKABLE void record(const QString &category, const QString &path);
    Q_INVOKABLE void remove(int row);
    Q_INVOKABLE QString nameAt(int row) const;
    Q_INVOKABLE QString iconAt(int row) const;
    Q_INVOKABLE QString accentAt(int row) const;
    Q_INVOKABLE QString pathAt(int row) const;
    Q_INVOKABLE QString categoryAt(int row) const;
    Q_INVOKABLE bool hasCustomIcon(int) const { return false; }
    Q_INVOKABLE void move(int, int) {}
    // Waar staat deze tegel nu in zijn eigen rij? (-1 = niet meer)
    Q_INVOKABLE int sourceRow(const QString &category, const QString &path) const;
    Q_INVOKABLE QObject *sourceModel(const QString &category) const { return m_sources.value(category); }
    // Dagen sinds iets uit deze rij is geopend (sinds de installatie van 4.3 als nooit)
    Q_INVOKABLE int daysUnused(const QString &category) const;

Q_SIGNALS:
    void countChanged();
    void revisionChanged();

private:
    struct Entry {
        QString category;
        QString path;
        int count = 0;
        qint64 last = 0;
    };
    void load();
    void save() const;
    void rebuild();
    QVariant callSource(const QString &category, const char *method, int row) const;

    mutable QSettings m_settings{appSettingsFile(), QSettings::IniFormat};
    QHash<QString, QObject *> m_sources;
    QList<Entry> m_entries;
    QList<Entry> m_rows;   // top 6
    int m_revision = 0;
};
