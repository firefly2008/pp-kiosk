// SPDX-License-Identifier: GPL-2.0-or-later
#include "usagemodel.h"

#include <QColor>
#include <QDateTime>
#include <QMetaMethod>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtMath>

#include <algorithm>

UsageModel::UsageModel(QObject *parent)
    : QAbstractListModel(parent)
{
    if (!m_settings.contains(profileKey(QStringLiteral("usage/since")))) {
        m_settings.setValue(profileKey(QStringLiteral("usage/since")), QDateTime::currentSecsSinceEpoch());
    }
    load();
}

void UsageModel::addSource(const QString &category, QObject *model)
{
    m_sources.insert(category, model);
    // Pictogrammen en namen komen uit de bron: bij wijzigingen opnieuw opbouwen
    auto *im = qobject_cast<QAbstractItemModel *>(model);
    if (!im) { rebuild(); return; }
    connect(im, &QAbstractItemModel::modelReset, this, &UsageModel::rebuild);
    connect(im, &QAbstractItemModel::dataChanged, this, [this] {
        ++m_revision;
        Q_EMIT revisionChanged();
    });
    rebuild();
}

void UsageModel::load()
{
    const QJsonArray list = QJsonDocument::fromJson(m_settings.value(profileKey(QStringLiteral("usage/list"))).toByteArray()).array();
    for (const QJsonValue &v : list) {
        const QJsonObject o = v.toObject();
        m_entries.append({o.value(QStringLiteral("c")).toString(), o.value(QStringLiteral("p")).toString(),
                          o.value(QStringLiteral("n")).toInt(), qint64(o.value(QStringLiteral("t")).toDouble())});
    }
}

void UsageModel::save() const
{
    QJsonArray list;
    for (const Entry &e : m_entries) {
        list.append(QJsonObject{{QStringLiteral("c"), e.category}, {QStringLiteral("p"), e.path},
                                {QStringLiteral("n"), e.count}, {QStringLiteral("t"), double(e.last)}});
    }
    m_settings.setValue(profileKey(QStringLiteral("usage/list")), QJsonDocument(list).toJson(QJsonDocument::Compact));
}

QVariant UsageModel::callSource(const QString &category, const char *method, int row) const
{
    QObject *src = m_sources.value(category);
    if (!src || row < 0) {
        return {};
    }
    QString out;
    QMetaObject::invokeMethod(src, method, Q_RETURN_ARG(QString, out), Q_ARG(int, row));
    return out;
}

int UsageModel::sourceRow(const QString &category, const QString &path) const
{
    QObject *src = m_sources.value(category);
    if (!src) {
        return -1;
    }
    const int n = src->property("count").toInt();
    for (int row = 0; row < n; ++row) {
        QString p;
        QMetaObject::invokeMethod(src, "pathAt", Q_RETURN_ARG(QString, p), Q_ARG(int, row));
        if (p == path) {
            return row;
        }
    }
    return -1;
}

// Score: aantal keer geopend, waarbij oud gebruik langzaam minder telt (halveert per 14 dagen)
void UsageModel::rebuild()
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    QList<QPair<double, Entry>> scored;
    for (const Entry &e : std::as_const(m_entries)) {
        if (e.path == QLatin1String("bigscreen:radio-now") || e.path == QLatin1String("bigscreen:resume")
            || sourceRow(e.category, e.path) < 0) {
            continue;
        }
        const double days = (now - e.last) / 86400.0;
        scored.append({e.count * qPow(0.5, days / 14.0), e});
    }
    std::sort(scored.begin(), scored.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
    beginResetModel();
    m_rows.clear();
    for (const auto &s : std::as_const(scored)) {
        if (m_rows.size() >= 6 || s.first < 0.5) {
            break;
        }
        m_rows.append(s.second);
    }
    endResetModel();
    ++m_revision;
    Q_EMIT revisionChanged();
    Q_EMIT countChanged();
}

void UsageModel::record(const QString &category, const QString &path)
{
    if (category.isEmpty() || path.isEmpty()) {
        return;
    }
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    bool found = false;
    for (Entry &e : m_entries) {
        if (e.category == category && e.path == path) {
            ++e.count;
            e.last = now;
            found = true;
        }
    }
    if (!found) {
        m_entries.append({category, path, 1, now});
    }
    m_settings.setValue(profileKey(QStringLiteral("usage/last")), QStringList{category, path});
    m_settings.setValue(profileKey(QStringLiteral("usage/lastUse/") + category), now);
    save();
    rebuild();
}

void UsageModel::remove(int row)
{
    if (row < 0 || row >= m_rows.size()) {
        return;
    }
    const Entry gone = m_rows.at(row);
    m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(), [&gone](const Entry &e) {
        return e.category == gone.category && e.path == gone.path;
    }), m_entries.end());
    save();
    rebuild();
}

QString UsageModel::lastCategory() const
{
    return m_settings.value(profileKey(QStringLiteral("usage/last"))).toStringList().value(0);
}

QString UsageModel::lastPath() const
{
    return m_settings.value(profileKey(QStringLiteral("usage/last"))).toStringList().value(1);
}

int UsageModel::daysUnused(const QString &category) const
{
    const qint64 since = m_settings.value(profileKey(QStringLiteral("usage/since"))).toLongLong();
    const qint64 last = qMax(since, m_settings.value(profileKey(QStringLiteral("usage/lastUse/") + category)).toLongLong());
    return int((QDateTime::currentSecsSinceEpoch() - last) / 86400);
}

int UsageModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant UsageModel::data(const QModelIndex &index, int role) const
{
    const int row = index.row();
    if (!index.isValid() || row < 0 || row >= count()) {
        return {};
    }
    switch (role) {
    case NameRole: return nameAt(row);
    case IconRole: return iconAt(row);
    case AccentRole: return accentAt(row);
    case PathRole: return pathAt(row);
    }
    return {};
}

QHash<int, QByteArray> UsageModel::roleNames() const
{
    return {{NameRole, "name"}, {IconRole, "iconSource"}, {AccentRole, "accent"}, {PathRole, "path"}};
}

QString UsageModel::nameAt(int row) const
{
    if (row < 0 || row >= count()) return {};
    const Entry &e = m_rows.at(row);
    return callSource(e.category, "nameAt", sourceRow(e.category, e.path)).toString();
}

QString UsageModel::iconAt(int row) const
{
    if (row < 0 || row >= count()) return {};
    const Entry &e = m_rows.at(row);
    return callSource(e.category, "iconAt", sourceRow(e.category, e.path)).toString();
}

QString UsageModel::accentAt(int row) const
{
    if (row < 0 || row >= count()) return QStringLiteral("#22252b");
    const Entry &e = m_rows.at(row);
    QObject *src = m_sources.value(e.category);
    const int r = sourceRow(e.category, e.path);
    if (!src || r < 0) return QStringLiteral("#22252b");
    // AppModel geeft een QColor terug, RadioModel een tekst
    const int index = src->metaObject()->indexOfMethod("accentAt(int)");
    if (index >= 0 && src->metaObject()->method(index).returnMetaType().id() == QMetaType::QColor) {
        QColor color;
        QMetaObject::invokeMethod(src, "accentAt", Q_RETURN_ARG(QColor, color), Q_ARG(int, r));
        return color.name();
    }
    QString text;
    QMetaObject::invokeMethod(src, "accentAt", Q_RETURN_ARG(QString, text), Q_ARG(int, r));
    return text;
}

QString UsageModel::pathAt(int row) const
{
    return row >= 0 && row < count() ? m_rows.at(row).path : QString();
}

QString UsageModel::categoryAt(int row) const
{
    return row >= 0 && row < count() ? m_rows.at(row).category : QString();
}
