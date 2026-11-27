// SPDX-License-Identifier: GPL-2.0-or-later
#include "backup.h"

#include <QBuffer>
#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QTemporaryFile>

namespace {
const QByteArray magic("PPKIOSK-BACKUP");
constexpr quint32 formatVersion = 1;

// Wat er mee gaat (paden ten opzichte van de datamap)
bool wanted(const QString &rel)
{
    static const QStringList files{QStringLiteral("agenda.json"), QStringLiteral("snelkoppelingen.json"),
                                   QStringLiteral("stats.json"), QStringLiteral("settings.ini")};
    if (files.contains(rel)) {
        return true;
    }
    if (rel.startsWith(QLatin1String("pictos/"))) {
        return true;
    }
    // Het menubord maakt de kiosk zelf opnieuw
    return rel.startsWith(QLatin1String("presentaties/")) && !rel.startsWith(QLatin1String("presentaties/Menu van de week/"));
}

bool safePath(const QString &rel)
{
    return !rel.isEmpty() && !rel.contains(QLatin1String("..")) && !rel.startsWith(QLatin1Char('/')) && !rel.contains(QLatin1Char(':'))
           && !rel.contains(QLatin1Char('\\')) && wanted(rel);
}
}

QByteArray Backup::create(const QString &dataDir, int *files)
{
    QByteArray out;
    QBuffer buffer(&out);
    buffer.open(QIODevice::WriteOnly);
    QDataStream ds(&buffer);
    ds.setVersion(QDataStream::Qt_6_4);
    ds << magic << formatVersion << QDateTime::currentDateTime().toString(Qt::ISODate);
    const QDir root(dataDir);
    int n = 0;
    QDirIterator it(dataDir, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString rel = root.relativeFilePath(path);
        if (!wanted(rel)) {
            continue;
        }
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QByteArray data = f.readAll();
        // Foto's en dia's zijn al gecomprimeerd; tekst wel inpakken
        const bool text = rel.endsWith(QLatin1String(".json")) || rel.endsWith(QLatin1String(".ini"));
        ds << rel << text << (text ? qCompress(data) : data);
        ++n;
    }
    ds << QString();    // einde
    if (files) {
        *files = n;
    }
    qInfo() << "Backup: created" << n << "files" << out.size() << "bytes";
    return out;
}

bool Backup::restore(const QString &dataDir, const QString &archive, QString *error, int *files)
{
    QFile in(archive);
    if (!in.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("Bestand kan niet gelezen worden.");
        return false;
    }
    QDataStream ds(&in);
    ds.setVersion(QDataStream::Qt_6_4);
    QByteArray head;
    quint32 version = 0;
    QString created;
    ds >> head >> version >> created;
    if (head != magic || version != formatVersion) {
        *error = QStringLiteral("Dit is geen back-up van PP Kiosk.");
        return false;
    }
    // Eerst alles lezen en controleren, pas dan schrijven
    QList<QPair<QString, QByteArray>> entries;
    for (;;) {
        QString rel;
        bool text = false;
        QByteArray data;
        ds >> rel;
        if (ds.status() != QDataStream::Ok) {
            *error = QStringLiteral("Back-up is beschadigd.");
            return false;
        }
        if (rel.isEmpty()) {
            break;
        }
        ds >> text >> data;
        if (ds.status() != QDataStream::Ok || !safePath(rel)) {
            *error = QStringLiteral("Back-up is beschadigd.");
            return false;
        }
        entries.append({rel, text ? qUncompress(data) : data});
    }
    int n = 0;
    for (const auto &e : entries) {
        const QString target = dataDir + QLatin1Char('/') + e.first;
        if (e.first == QLatin1String("settings.ini")) {
            // Instellingen samenvoegen: beheercode en versleutelde (pc-gebonden) waarden van deze pc houden
            QTemporaryFile tmp;
            if (!tmp.open()) {
                continue;
            }
            tmp.write(e.second);
            tmp.flush();
            QSettings from(tmp.fileName(), QSettings::IniFormat);
            QSettings to(target, QSettings::IniFormat);
            for (const QString &key : from.allKeys()) {
                if (key.startsWith(QLatin1String("admin/")) || key.endsWith(QLatin1String("_dpapi"))) {
                    continue;
                }
                to.setValue(key, from.value(key));
            }
            to.sync();
            ++n;
            continue;
        }
        QDir().mkpath(QFileInfo(target).absolutePath());
        QSaveFile f(target);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(e.second);
            if (f.commit()) {
                ++n;
            }
        }
    }
    if (files) {
        *files = n;
    }
    qInfo() << "Backup: restored" << n << "files from" << created;
    return true;
}
