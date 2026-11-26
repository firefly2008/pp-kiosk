// SPDX-License-Identifier: GPL-2.0-or-later
#include "presentations.h"
#include "paths.h"

#include <QCollator>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QUrl>

namespace {
const QStringList imageFilters = {QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.png"), QStringLiteral("*.webp"), QStringLiteral("*.bmp")};

QStringList sortedImages(const QDir &dir)
{
    QStringList files = dir.entryList(imageFilters, QDir::Files);
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(files.begin(), files.end(), collator);
    return files;
}

QJsonObject readInfo(const QDir &dir)
{
    QFile f(dir.filePath(QStringLiteral("info.json")));
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(f.readAll()).object();
}
} // namespace

Presentations::Presentations(const QString &dataDir, QObject *parent)
    : QObject(parent)
    , m_root(dataDir + QStringLiteral("/presentaties"))
{
    QDir().mkpath(m_root);
    installDemo();
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(800);
    connect(&m_debounce, &QTimer::timeout, this, &Presentations::refresh);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_debounce, qOverload<>(&QTimer::start));
    refresh();
}

void Presentations::installDemo()
{
    QSettings settings(appSettingsFile(), QSettings::IniFormat);
    if (settings.value(QStringLiteral("presentations/demoInstalled")).toBool()) {
        return;
    }
    const QString dir = m_root + QStringLiteral("/Diner van de dag");
    QDir().mkpath(dir);
    QFile::copy(QStringLiteral(":/demo/diner-van-de-dag.jpg"), dir + QStringLiteral("/01.jpg"));
    QFile::setPermissions(dir + QStringLiteral("/01.jpg"), QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);
    QFile info(dir + QStringLiteral("/info.json"));
    if (info.open(QIODevice::WriteOnly)) {
        const QJsonObject caption{
            {QStringLiteral("title"), QStringLiteral("Diner van de dag")},
            {QStringLiteral("subtitle"), QStringLiteral("Zuurkoolstamppot met rookworst")},
            {QStringLiteral("badge"), QStringLiteral("17:30")},
        };
        info.write(QJsonDocument(QJsonObject{
                                     {QStringLiteral("title"), QStringLiteral("Diner van de dag")},
                                     {QStringLiteral("interval"), 12},
                                     {QStringLiteral("captions"), QJsonObject{{QStringLiteral("01.jpg"), caption}}},
                                 })
                       .toJson());
    }
    settings.setValue(QStringLiteral("presentations/demoInstalled"), true);
    qInfo() << "Presentations: demo installed";
}

void Presentations::refresh()
{
    if (!m_watcher.directories().isEmpty()) {
        m_watcher.removePaths(m_watcher.directories());
    }
    m_watcher.addPath(m_root);
    m_list.clear();
    QDir root(m_root);
    QStringList folders = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(folders.begin(), folders.end(), collator);
    for (const QString &folder : folders) {
        const QDir dir(root.filePath(folder));
        m_watcher.addPath(dir.absolutePath());
        const QStringList images = sortedImages(dir);
        if (images.isEmpty()) {
            continue;
        }
        const QJsonObject info = readInfo(dir);
        QVariantMap item;
        item.insert(QStringLiteral("folder"), folder);
        item.insert(QStringLiteral("title"), info.value(QStringLiteral("title")).toString(folder));
        item.insert(QStringLiteral("count"), images.size());
        item.insert(QStringLiteral("cover"), QUrl::fromLocalFile(dir.filePath(images.first())).toString());
        item.insert(QStringLiteral("interval"), qBound(3, info.value(QStringLiteral("interval")).toInt(10), 300));
        m_list.append(item);
    }
    qInfo() << "Presentations:" << m_list.size();
    Q_EMIT changed();
}

QVariantList Presentations::slides(const QString &folder) const
{
    QVariantList out;
    if (folder.contains(QLatin1Char('/')) || folder.contains(QLatin1Char('\\')) || folder.startsWith(QLatin1Char('.'))) {
        return out;
    }
    const QDir dir(m_root + QLatin1Char('/') + folder);
    const QJsonObject captions = readInfo(dir).value(QStringLiteral("captions")).toObject();
    for (const QString &file : sortedImages(dir)) {
        const QJsonObject c = captions.value(file).toObject();
        out.append(QVariantMap{
            {QStringLiteral("source"), QUrl::fromLocalFile(dir.filePath(file)).toString()},
            {QStringLiteral("title"), c.value(QStringLiteral("title")).toString()},
            {QStringLiteral("subtitle"), c.value(QStringLiteral("subtitle")).toString()},
            {QStringLiteral("badge"), c.value(QStringLiteral("badge")).toString()},
        });
    }
    return out;
}
