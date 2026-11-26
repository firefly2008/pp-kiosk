// SPDX-License-Identifier: GPL-2.0-or-later
#include "nowtvmodel.h"
#include "iptvmodel.h"

NowTvModel::NowTvModel(IptvModel *iptv, QObject *parent)
    : QAbstractListModel(parent)
    , m_iptv(iptv)
{
    connect(iptv, &IptvModel::epgChanged, this, &NowTvModel::refresh);
    connect(iptv, &IptvModel::channelsChanged, this, &NowTvModel::refresh);
    connect(iptv, &IptvModel::filterChanged, this, &NowTvModel::refresh);
    connect(iptv, &IptvModel::dataChanged, this, &NowTvModel::refresh);
    // Elke minuut: voortgang en het volgende programma
    m_timer.setInterval(60000);
    connect(&m_timer, &QTimer::timeout, this, &NowTvModel::refresh);
    m_timer.start();
    refresh();
}

void NowTvModel::refresh()
{
    const QVariantList items = m_iptv->favoritesNow(12);
    if (items.size() != m_items.size()) {
        beginResetModel();
        m_items = items;
        endResetModel();
        Q_EMIT countChanged();
    } else {
        m_items = items;
        if (!m_items.isEmpty()) {
            Q_EMIT dataChanged(index(0), index(m_items.size() - 1));
        }
    }
    ++m_revision;
    Q_EMIT revisionChanged();
}

int NowTvModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant NowTvModel::data(const QModelIndex &index, int role) const
{
    const int row = index.row();
    switch (role) {
    case NameRole: return nameAt(row);
    case IconRole: return iconAt(row);
    case AccentRole: return accentAt(row);
    case PathRole: return pathAt(row);
    }
    return {};
}

QHash<int, QByteArray> NowTvModel::roleNames() const
{
    return {{NameRole, "name"}, {IconRole, "iconSource"}, {AccentRole, "accent"}, {PathRole, "path"}};
}

static QVariantMap itemAt(const QVariantList &items, int row)
{
    return row >= 0 && row < items.size() ? items.at(row).toMap() : QVariantMap();
}

QString NowTvModel::nameAt(int row) const { return itemAt(m_items, row).value(QStringLiteral("name")).toString(); }
QString NowTvModel::iconAt(int row) const { return itemAt(m_items, row).value(QStringLiteral("logo")).toString(); }
QString NowTvModel::urlAt(int row) const { return itemAt(m_items, row).value(QStringLiteral("url")).toString(); }
QString NowTvModel::pathAt(int row) const { return QStringLiteral("tv:") + urlAt(row); }

QString NowTvModel::subtitleAt(int row) const
{
    const QVariantMap m = itemAt(m_items, row);
    const QString title = m.value(QStringLiteral("title")).toString();
    return title.isEmpty() ? tr("Geen gidsinformatie") : title + QStringLiteral("  ·  ") + m.value(QStringLiteral("time")).toString();
}

double NowTvModel::progressAt(int row) const
{
    // Zonder gids toch een balkje van 0, zodat alle tegels even groot zijn
    return qMax(0.0, itemAt(m_items, row).value(QStringLiteral("progress")).toDouble());
}

bool NowTvModel::launch(int row)
{
    const QString url = urlAt(row);
    if (url.isEmpty()) {
        return false;
    }
    Q_EMIT watchRequested(url);
    return true;
}
