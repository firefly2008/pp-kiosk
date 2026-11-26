// SPDX-License-Identifier: GPL-2.0-or-later
#include "updater.h"

#include <QCryptographicHash>
#include "paths.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QVersionNumber>

#ifndef PPKIOSK_VERSION
#define PPKIOSK_VERSION "0.0.0"
#endif

namespace {
const QString taskName = QStringLiteral("PP Kiosk bijwerken");
QString dataFile(const QString &name)
{
    return appDataDir() + QLatin1Char('/') + name;
}
}

Updater::Updater(QObject *parent)
    : QObject(parent)
{
    // Geplande taak aanwezig (installer 4.0 of nieuwer)? Dan kan bijwerken stil
    QProcess query;
    query.start(QStringLiteral("schtasks.exe"), {QStringLiteral("/Query"), QStringLiteral("/TN"), taskName});
    m_silent = query.waitForFinished(5000) && query.exitStatus() == QProcess::NormalExit && query.exitCode() == 0;
    qInfo() << "Update: silent task" << (m_silent ? "present" : "missing");
    m_statusTimer = new QTimer(this);
    m_statusTimer->setInterval(5000);
    connect(m_statusTimer, &QTimer::timeout, this, &Updater::readTaskStatus);
}

void Updater::startAutomatic()
{
    m_checkTimer = new QTimer(this);
    m_checkTimer->setInterval(6 * 60 * 60 * 1000);
    connect(m_checkTimer, &QTimer::timeout, this, [this] { check(false); });
    m_checkTimer->start();
    QTimer::singleShot(60 * 1000, this, [this] { check(false); });
    // Was er al een download klaar (bv. PP Kiosk was even dicht)? Dan die oppakken
    readTaskStatus();
}

void Updater::setPhase(const QString &phase)
{
    if (phase != m_phase) {
        m_phase = phase;
        qInfo() << "Update phase" << phase;
        Q_EMIT changed();
    }
}

void Updater::startSilentDownload()
{
    if (!m_phase.isEmpty()) {
        return;
    }
    QFile::remove(dataFile(QStringLiteral("update-now")));
    QFile::remove(dataFile(QStringLiteral("update-status.txt")));
    if (!QProcess::startDetached(QStringLiteral("schtasks.exe"), {QStringLiteral("/Run"), QStringLiteral("/TN"), taskName})) {
        qInfo() << "Update: could not start task";
        return;
    }
    setPhase(QStringLiteral("downloading"));
    m_statusTimer->start();
}

// Wat de geplande taak meldt (update-status.txt): downloading/ready/installing/current/error
void Updater::readTaskStatus()
{
    QFile file(dataFile(QStringLiteral("update-status.txt")));
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QString text = QString::fromUtf8(file.readAll()).trimmed();
    const QString word = text.section(QLatin1Char(' '), 0, 0).remove(QChar(0xFEFF));
    const QString version = text.section(QLatin1Char(' '), 1, 1);
    if (!version.isEmpty() && word != QLatin1String("error")) {
        m_latest = version;
    }
    if (word == QLatin1String("downloading")) {
        setPhase(QStringLiteral("downloading"));
        m_statusTimer->start();
    } else if (word == QLatin1String("ready") && compare(version, current()) > 0) {
        setPhase(QStringLiteral("ready"));
        m_statusTimer->start();
    } else if (word == QLatin1String("error")) {
        qInfo().noquote() << "Update task:" << text;
        m_statusTimer->stop();
        setPhase(QString());
    } else if (word == QLatin1String("current") || word == QLatin1String("installed") || word == QLatin1String("ready")) {
        m_statusTimer->stop();
        setPhase(QString());
    }
}

void Updater::installNow()
{
    if (m_phase != QLatin1String("ready")) {
        return;
    }
    QFile flag(dataFile(QStringLiteral("update-now")));
    if (flag.open(QIODevice::WriteOnly)) {
        flag.write(m_latest.toUtf8());
        flag.close();
    }
    setPhase(QStringLiteral("installing"));
    setStatus(tr("PP Kiosk %1 wordt geïnstalleerd en start zo opnieuw…").arg(m_latest), true);
    qInfo() << "Update: install now" << m_latest;
    QTimer::singleShot(2500, qApp, &QCoreApplication::quit);
}

QString Updater::releaseNotesJson() const
{
    QFile file(QStringLiteral(":/releasenotes.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "releasenotes.json niet gevonden in de bronnen";
        return QStringLiteral("[]");
    }
    return QString::fromUtf8(file.readAll());
}

QString Updater::current() const
{
    return QStringLiteral(PPKIOSK_VERSION);
}

QString Updater::repo() const
{
    return m_settings.value(QStringLiteral("update/repo"), QStringLiteral("firefly2008/pp-kiosk-releases")).toString();
}

int Updater::compare(const QString &a, const QString &b)
{
    return QVersionNumber::compare(QVersionNumber::fromString(a), QVersionNumber::fromString(b));
}

bool Updater::updateAvailable() const
{
    return !m_latest.isEmpty() && m_asset.isValid() && compare(m_latest, current()) > 0;
}

bool Updater::shouldOffer() const
{
    return updateAvailable() && m_settings.value(QStringLiteral("update/dismissed")).toString() != m_latest;
}

void Updater::dismiss()
{
    m_settings.setValue(QStringLiteral("update/dismissed"), m_latest);
}

void Updater::setStatus(const QString &status, bool busy)
{
    m_status = status;
    m_busy = busy;
    Q_EMIT changed();
}

void Updater::check(bool manual)
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (m_busy) {
        return;
    }
    setStatus(tr("Zoeken naar een nieuwe versie…"), true);
    QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(repo())));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("PPKiosk/") + current());
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, manual, now] {
        reply->deleteLater();
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        if (reply->error() != QNetworkReply::NoError || o.isEmpty()) {
            qInfo() << "Update check failed" << http << reply->errorString();
            setStatus(http == 404 ? tr("Nog geen versie gevonden op de downloadplek (github.com/firefly2008/pp-kiosk-releases).")
                                  : tr("Zoeken naar een nieuwe versie lukte niet. Is er internet?"), false);
            Q_EMIT checked(manual);
            return;
        }
        m_settings.setValue(QStringLiteral("update/lastCheck"), now);
        m_latest = o.value(QStringLiteral("tag_name")).toString();
        if (m_latest.startsWith(QLatin1Char('v'))) {
            m_latest.remove(0, 1);
        }
        m_notes = o.value(QStringLiteral("body")).toString().left(600);
        m_asset = QUrl();
        m_digest.clear();
        for (const QJsonValue &v : o.value(QStringLiteral("assets")).toArray()) {
            const QString name = v.toObject().value(QStringLiteral("name")).toString();
            if (name.endsWith(QLatin1String("-x64-setup.exe"), Qt::CaseInsensitive)) {
                m_asset = QUrl(v.toObject().value(QStringLiteral("browser_download_url")).toString());
                m_digest = v.toObject().value(QStringLiteral("digest")).toString();
            }
        }
        m_settings.setValue(QStringLiteral("update/latest"), m_latest);
        m_settings.setValue(QStringLiteral("update/asset"), m_asset);
        m_settings.setValue(QStringLiteral("update/digest"), m_digest);
        m_settings.setValue(QStringLiteral("update/notes"), m_notes);
        qInfo() << "Update check: current" << current() << "latest" << m_latest;
        setStatus(updateAvailable() ? tr("PP Kiosk %1 is er.").arg(m_latest)
                                    : tr("Je hebt de nieuwste versie (%1).").arg(current()), false);
        if (updateAvailable() && m_silent) {
            startSilentDownload();   // geen vraag: op de achtergrond ophalen en installeren
        }
        Q_EMIT checked(manual);
    });
}

void Updater::install()
{
    if (!updateAvailable() || m_busy) {
        return;
    }
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString file = QDir(dir).filePath(QStringLiteral("PPKiosk-%1-x64-setup.exe").arg(m_latest));
    m_progress = 0;
    setStatus(tr("PP Kiosk %1 ophalen…").arg(m_latest), true);
    QNetworkRequest request(m_asset);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = m_network.get(request);
    auto *out = new QFile(file, reply);
    if (!out->open(QIODevice::WriteOnly)) {
        reply->abort();
        setStatus(tr("Kon de nieuwe versie niet opslaan."), false);
        return;
    }
    connect(reply, &QNetworkReply::readyRead, this, [reply, out] { out->write(reply->readAll()); });
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 got, qint64 total) {
        m_progress = total > 0 ? double(got) / total : 0;
        Q_EMIT changed();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, out, file] {
        reply->deleteLater();
        out->write(reply->readAll());
        out->close();
        if (reply->error() != QNetworkReply::NoError || out->size() < 1000000) {
            qInfo() << "Update download failed" << reply->errorString();
            QFile::remove(file);
            setStatus(tr("Ophalen lukte niet. Probeer het later nog eens."), false);
            return;
        }
        // Controlegetal van GitHub verplicht: anders deze installer niet starten
        {
            QFile f(file);
            QCryptographicHash sha(QCryptographicHash::Sha256);
            const bool ok = m_digest.startsWith(QLatin1String("sha256:")) && f.open(QIODevice::ReadOnly) && sha.addData(&f)
                            && QString::fromLatin1(sha.result().toHex()) == m_digest.mid(7).toLower();
            f.close();
            if (!ok) {
                qWarning() << "Update: checksum missing or wrong";
                QFile::remove(file);
                setStatus(tr("Ophalen lukte niet. Probeer het later nog eens."), false);
                return;
            }
        }
        qInfo() << "Update downloaded" << file;
        setStatus(tr("Bijwerken… PP Kiosk start zo opnieuw."), true);
        // Installer stil laten lopen; /relaunch=1 start PP Kiosk daarna weer (zie bigscreen-host.iss)
        if (QProcess::startDetached(file, {QStringLiteral("/SILENT"), QStringLiteral("/SUPPRESSMSGBOXES"),
                                           QStringLiteral("/NORESTART"), QStringLiteral("/relaunch=1")})) {
            QTimer::singleShot(800, qApp, &QCoreApplication::quit);
        } else {
            setStatus(tr("De installer kon niet starten."), false);
        }
    });
}
