// Rij "Nu op tv" op het startscherm (4.4): je favoriete zenders met het programma van nu.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QAbstractListModel>
#include <QTimer>
#include <QVariantList>

class IptvModel;

class NowTvModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
    enum Roles { NameRole = Qt::UserRole + 1, IconRole, AccentRole, PathRole };
    explicit NowTvModel(IptvModel *iptv, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_items.size(); }
    int revision() const { return m_revision; }
    Q_INVOKABLE QString nameAt(int row) const;
    Q_INVOKABLE QString iconAt(int row) const;
    Q_INVOKABLE QString accentAt(int) const { return QStringLiteral("#1f3a5c"); }
    Q_INVOKABLE QString pathAt(int row) const;      // "tv:<zenderadres>"
    Q_INVOKABLE QString urlAt(int row) const;
    Q_INVOKABLE QString subtitleAt(int row) const;
    Q_INVOKABLE double progressAt(int row) const;
    Q_INVOKABLE bool hasCustomIcon(int) const { return false; }
    Q_INVOKABLE void move(int, int) {}
    Q_INVOKABLE bool launch(int row);
    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void countChanged();
    void revisionChanged();
    void watchRequested(const QString &url);

private:
    IptvModel *m_iptv;
    QVariantList m_items;
    QTimer m_timer;
    int m_revision = 0;
};
