// SPDX-License-Identifier: GPL-2.0-or-later
#include "resumemodel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

#include "components.h"
#include "medialibrary.h"
#include "mpcplayer.h"

namespace {
QString decodeIni(const QByteArray &raw)
{
    // MPC-HC schrijft zijn ini als UTF-16 (met BOM) of als UTF-8
    if (raw.startsWith("\xFF\xFE")) {
        return QString::fromUtf16(reinterpret_cast<const char16_t *>(raw.constData() + 2), (raw.size() - 2) / 2);
    }
    if (raw.startsWith("\xEF\xBB\xBF")) {
        return QString::fromUtf8(raw.mid(3));
    }
    return QString::fromUtf8(raw);
}
}

ResumeModel::ResumeModel(const QString &dataDir, MediaLibrary *library, QObject *parent)
    : QAbstractListModel(parent)
    , m_dataDir(dataDir)
    , m_library(library)
    , m_settings(dataDir + QStringLiteral("/settings.ini"), QSettings::IniFormat)
{
    QFile cache(m_dataDir + QStringLiteral("/resume-durations.json"));
    if (cache.open(QIODevice::ReadOnly)) {
        const QJsonObject o = QJsonDocument::fromJson(cache.readAll()).object();
        for (auto it = o.begin(); it != o.end(); ++it) {
            m_durations.insert(it.key(), qint64(it.value().toDouble()));
        }
    }
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(800);
    connect(&m_refreshTimer, &QTimer::timeout, this, &ResumeModel::refresh);
    connect(library, &MediaLibrary::itemsChanged, &m_refreshTimer, qOverload<>(&QTimer::start));
    if (MpcPlayer *mpc = MpcPlayer::instance()) {
        // MPC-HC schrijft zijn geschiedenis bij het afsluiten
        connect(mpc, &MpcPlayer::finished, this, [this] { QTimer::singleShot(1500, this, &ResumeModel::refresh); });
    }
    m_refreshTimer.start();
}

int ResumeModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_items.size());
}

QHash<int, QByteArray> ResumeModel::roleNames() const
{
    return {{NameRole, "name"}, {IconRole, "iconSource"}, {AccentRole, "accent"}, {PathRole, "path"}};
}

QVariant ResumeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size()) {
        return {};
    }
    const QVariantMap &item = m_items.at(index.row());
    switch (role) {
    case NameRole: return item.value(QStringLiteral("name"));
    case IconRole: return item.value(QStringLiteral("poster")).toString().isEmpty()
            ? QStringLiteral("glyph:") + QChar(0xE768) : item.value(QStringLiteral("poster"));
    case AccentRole: return QStringLiteral("#2a2d34");
    case PathRole: return item.value(QStringLiteral("file"));
    }
    return {};
}

QString ResumeModel::nameAt(int row) const
{
    return row >= 0 && row < m_items.size() ? m_items.at(row).value(QStringLiteral("name")).toString() : QString();
}

QString ResumeModel::backdropAt(int row) const
{
    return row >= 0 && row < m_items.size() ? m_items.at(row).value(QStringLiteral("backdrop")).toString() : QString();
}

QString ResumeModel::subtitleAt(int row) const
{
    return row >= 0 && row < m_items.size() ? m_items.at(row).value(QStringLiteral("subtitle")).toString() : QString();
}

double ResumeModel::progressAt(int row) const
{
    return row >= 0 && row < m_items.size() ? m_items.at(row).value(QStringLiteral("progress"), -1.0).toDouble() : -1.0;
}

QList<ResumeModel::Entry> ResumeModel::readHistory() const
{
    // Alle ini-bestanden van onze MPC-HC (en de terugvalplek in %APPDATA%\MPC-HC)
    QStringList files;
    if (Components *c = Components::instance()) {
        for (const QFileInfo &fi : QDir(c->dir(Components::MpcHc)).entryInfoList({QStringLiteral("*.ini")}, QDir::Files)) {
            files << fi.absoluteFilePath();
        }
    }
    const QString appData = qEnvironmentVariable("APPDATA");
    if (!appData.isEmpty()) {
        for (const QFileInfo &fi : QDir(appData + QStringLiteral("/MPC-HC")).entryInfoList({QStringLiteral("*.ini")}, QDir::Files)) {
            files << fi.absoluteFilePath();
        }
    }
    QHash<QString, Entry> byFile;
    static const QRegularExpression section(QStringLiteral(R"(^\[MediaHistory\\([^\]]+)\]\s*$)"), QRegularExpression::CaseInsensitiveOption);
    for (const QString &path : std::as_const(files)) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            continue;
        }
        Entry current;
        bool inHistory = false;
        auto flush = [&] {
            if (inHistory && !current.file.isEmpty()) {
                const QString key = QDir::toNativeSeparators(current.file).toLower();
                if (!byFile.contains(key) || byFile.value(key).lastOpened < current.lastOpened) {
                    byFile.insert(key, current);
                }
            }
            current = {};
        };
        for (const QString &raw : decodeIni(f.readAll()).split(QLatin1Char('\n'))) {
            const QString line = raw.trimmed();
            if (line.startsWith(QLatin1Char('['))) {
                flush();
                inHistory = section.match(line).hasMatch();
                continue;
            }
            if (!inHistory) {
                continue;
            }
            const int eq = line.indexOf(QLatin1Char('='));
            if (eq < 0) {
                continue;
            }
            const QString key = line.left(eq).trimmed();
            const QString value = line.mid(eq + 1).trimmed();
            if (key.compare(QLatin1String("Filename"), Qt::CaseInsensitive) == 0) {
                current.file = value;
            } else if (key.compare(QLatin1String("FilePosition"), Qt::CaseInsensitive) == 0) {
                current.positionMs = value.toLongLong();
            } else if (key.compare(QLatin1String("LastOpened"), Qt::CaseInsensitive) == 0) {
                current.lastOpened = value;
            }
        }
        flush();
    }
    QList<Entry> list = byFile.values();
    std::sort(list.begin(), list.end(), [](const Entry &a, const Entry &b) { return a.lastOpened > b.lastOpened; });
    return list;
}

// Titel, plaatje en serie-informatie uit de bibliotheek
QVariantMap ResumeModel::lookup(const QString &file) const
{
    const QString key = QDir::toNativeSeparators(file).toLower();
    for (const QVariant &v : m_library->movies()) {
        const QVariantMap m = v.toMap();
        if (QDir::toNativeSeparators(m.value(QStringLiteral("path")).toString()).toLower() == key) {
            QVariantMap r{{QStringLiteral("name"), m.value(QStringLiteral("title"))},
                          {QStringLiteral("subtitle"), m.value(QStringLiteral("year"))},
                          {QStringLiteral("poster"), m.value(QStringLiteral("poster"))},
                          {QStringLiteral("backdrop"), m.value(QStringLiteral("backdrop"))},
                          {QStringLiteral("kind"), QStringLiteral("movie")}};
            if (m.value(QStringLiteral("runtime")).toInt() > 0) {
                r[QStringLiteral("durationMs")] = qint64(m.value(QStringLiteral("runtime")).toInt()) * 60000;
            }
            return r;
        }
    }
    for (const QVariant &v : m_library->shows()) {
        const QVariantMap s = v.toMap();
        const QVariantList episodes = s.value(QStringLiteral("episodes")).toList();
        for (int i = 0; i < episodes.size(); ++i) {
            const QVariantMap e = episodes.at(i).toMap();
            if (QDir::toNativeSeparators(e.value(QStringLiteral("path")).toString()).toLower() != key) {
                continue;
            }
            QStringList rest;
            for (int j = i; j < episodes.size(); ++j) {
                rest << episodes.at(j).toMap().value(QStringLiteral("path")).toString();
            }
            const QString epTitle = e.value(QStringLiteral("title")).toString();
            return {{QStringLiteral("name"), s.value(QStringLiteral("title"))},
                    {QStringLiteral("subtitle"), e.value(QStringLiteral("code")).toString() + (epTitle.isEmpty() ? QString() : QStringLiteral(" · ") + epTitle)},
                    {QStringLiteral("poster"), s.value(QStringLiteral("poster"))},
                    {QStringLiteral("backdrop"), s.value(QStringLiteral("backdrop"))},
                    {QStringLiteral("kind"), QStringLiteral("episode")},
                    {QStringLiteral("playlist"), rest}};
        }
    }
    return {};
}

void ResumeModel::refresh()
{
    const QStringList hidden = m_settings.value(QStringLiteral("resume/hidden")).toStringList();
    QList<QVariantMap> items;
    for (const Entry &e : readHistory()) {
        if (e.positionMs < 60000 || !QFileInfo::exists(e.file)) {
            continue; // net begonnen, of het bestand is weg
        }
        if (hidden.contains(e.file + QLatin1Char('|') + QString::number(e.positionMs / 60000))) {
            continue;
        }
        QVariantMap item = lookup(e.file);
        if (item.isEmpty()) {
            continue; // alleen films en series uit de bibliotheek
        }
        qint64 duration = item.value(QStringLiteral("durationMs")).toLongLong();
        if (m_durations.contains(e.file)) {
            duration = m_durations.value(e.file);
        } else {
            probeDuration(e.file); // exacte lengte op de achtergrond
        }
        double progress = duration > 0 ? double(e.positionMs) / double(duration) : -1.0;
        if (progress > 0.95) {
            continue; // zo goed als uitgekeken
        }
        const qint64 minutes = e.positionMs / 60000;
        item[QStringLiteral("file")] = e.file;
        item[QStringLiteral("progress")] = progress;
        item[QStringLiteral("positionMs")] = e.positionMs;
        const QString where = duration > 0 ? tr("nog %1 min").arg(qMax<qint64>(1, (duration - e.positionMs) / 60000))
                                           : tr("gestopt na %1 min").arg(minutes);
        const QString sub = item.value(QStringLiteral("subtitle")).toString();
        item[QStringLiteral("subtitle")] = sub.isEmpty() ? where : sub + QStringLiteral("  ·  ") + where;
        items.append(item);
        if (items.size() >= 12) {
            break;
        }
    }
    const int before = int(m_items.size());
    beginResetModel();
    m_items = items;
    endResetModel();
    ++m_revision;
    Q_EMIT revisionChanged();
    if (before != m_items.size()) {
        Q_EMIT countChanged();
    }
    qInfo() << "Resume:" << m_items.size() << "item(s)";
}

// Lengte van een bestand met ffmpeg (staat "Duration: 01:02:03.45" in de uitvoer)
void ResumeModel::probeDuration(const QString &file)
{
    Components *c = Components::instance();
    const QString ffmpeg = c ? c->executable(Components::Ffmpeg) : QString();
    if (ffmpeg.isEmpty() || m_probing.contains(file)) {
        return;
    }
    m_probing.insert(file);
    auto *p = new QProcess(this);
    p->setProcessChannelMode(QProcess::MergedChannels);
    connect(p, &QProcess::finished, this, [this, p, file] {
        p->deleteLater();
        m_probing.remove(file);
        const auto m = QRegularExpression(QStringLiteral(R"(Duration:\s*(\d+):(\d+):(\d+)\.(\d+))")).match(QString::fromUtf8(p->readAll()));
        if (!m.hasMatch()) {
            return;
        }
        const qint64 ms = ((m.captured(1).toLongLong() * 60 + m.captured(2).toLongLong()) * 60 + m.captured(3).toLongLong()) * 1000
                          + m.captured(4).left(3).leftJustified(3, QLatin1Char('0')).toLongLong();
        m_durations.insert(file, ms);
        QJsonObject o;
        for (auto it = m_durations.constBegin(); it != m_durations.constEnd(); ++it) {
            o.insert(it.key(), double(it.value()));
        }
        QFile cache(m_dataDir + QStringLiteral("/resume-durations.json"));
        if (cache.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            cache.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
        }
        m_refreshTimer.start();
    });
    p->start(ffmpeg, {QStringLiteral("-hide_banner"), QStringLiteral("-i"), file});
}

bool ResumeModel::launch(int row)
{
    if (row < 0 || row >= m_items.size()) {
        return false;
    }
    MpcPlayer *mpc = MpcPlayer::instance();
    if (!mpc) {
        return false;
    }
    const QVariantMap item = m_items.at(row);
    // MPC-HC gaat zelf verder op de onthouden plek; bij een serie volgen de afleveringen daarna
    if (item.value(QStringLiteral("kind")) == QLatin1String("episode")) {
        return mpc->play(item.value(QStringLiteral("playlist")).toStringList(), false);
    }
    return mpc->play({item.value(QStringLiteral("file")).toString()}, true);
}

void ResumeModel::remove(int row)
{
    if (row < 0 || row >= m_items.size()) {
        return;
    }
    const QVariantMap item = m_items.at(row);
    QStringList hidden = m_settings.value(QStringLiteral("resume/hidden")).toStringList();
    hidden << item.value(QStringLiteral("file")).toString() + QLatin1Char('|') + QString::number(item.value(QStringLiteral("positionMs")).toLongLong() / 60000);
    while (hidden.size() > 200) {
        hidden.removeFirst();
    }
    m_settings.setValue(QStringLiteral("resume/hidden"), hidden);
    refresh();
}
