// "Verder kijken": films en afleveringen die je in MPC-HC halverwege hebt gestopt.
// MPC-HC onthoudt per bestand waar je was (MediaHistory\<hash>: Filename, FilePosition in ms,
// LastOpened). Dit model leest dat uit en koppelt het aan de bibliotheek voor titel en plaatje.
// Zelfde rollen en functies als AppModel, zodat de rij op het startscherm gewoon werkt.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QSettings>
#include <QTimer>
#include <QVariantMap>

class MediaLibrary;

class ResumeModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
    enum Roles { NameRole = Qt::UserRole + 1, IconRole, AccentRole, PathRole };

    ResumeModel(const QString &dataDir, MediaLibrary *library, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return int(m_items.size()); }

    // Zelfde aanroepen als AppModel (startscherm)
    Q_INVOKABLE QString nameAt(int row) const;
    Q_INVOKABLE bool launch(int row);
    Q_INVOKABLE bool hasCustomIcon(int) const { return false; }
    // Voor het grote tekstvlak bovenaan het startscherm
    int revision() const { return m_revision; }
    Q_INVOKABLE QString iconAt(int row) const { return row >= 0 && row < m_items.size() ? m_items.at(row).value(QStringLiteral("poster")).toString() : QString(); }
    Q_INVOKABLE QString accentAt(int) const { return QStringLiteral("#3a2a30"); }
    Q_INVOKABLE void move(int, int) {}
    // Extra voor de tegel
    Q_INVOKABLE QString backdropAt(int row) const;
    Q_INVOKABLE QString subtitleAt(int row) const;
    Q_INVOKABLE double progressAt(int row) const;
    Q_INVOKABLE void remove(int row);      // uit de rij halen (tot je weer verder kijkt)
    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void countChanged();
    void revisionChanged();

private:
    struct Entry {
        QString file;
        qint64 positionMs = 0;
        QString lastOpened;
    };
    QList<Entry> readHistory() const;
    void probeDuration(const QString &file);
    QVariantMap lookup(const QString &file) const;

    QString m_dataDir;
    MediaLibrary *m_library;
    QSettings m_settings;
    QList<QVariantMap> m_items;
    QHash<QString, qint64> m_durations;   // bestand -> lengte in ms (uit de bibliotheek of ffmpeg)
    QSet<QString> m_probing;
    QTimer m_refreshTimer;
    int m_revision = 0;
};
