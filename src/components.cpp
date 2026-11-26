// SPDX-License-Identifier: GPL-2.0-or-later
#include "components.h"

#include <QCryptographicHash>
#include <utility>

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QCoreApplication>
#include <QProcess>
#include <QRandomGenerator>
#include <QUdpSocket>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

namespace {
Components *s_instance = nullptr;

const char *const userAgent = "Kijkluister (+https://github.com/firefly2008/bigscreen-host-windows)";
}

Components::Components(const QString &dataDir, QObject *parent)
    : QObject(parent)
    , m_dataDir(dataDir)
    , m_settings(dataDir + QStringLiteral("/settings.ini"), QSettings::IniFormat)
{
    s_instance = this;
}

Components *Components::instance()
{
    return s_instance;
}

QString Components::name(Kind kind) const
{
    return kind == MpcHc ? QStringLiteral("MPC-HC") : QStringLiteral("ffmpeg");
}

QString Components::dir(Kind kind) const
{
    return m_dataDir + (kind == MpcHc ? QStringLiteral("/mpc-hc") : QStringLiteral("/ffmpeg"));
}

QString Components::executable(Kind kind) const
{
    const QString exe = dir(kind) + (kind == MpcHc ? QStringLiteral("/mpc-hc64.exe") : QStringLiteral("/ffmpeg.exe"));
    return QFileInfo::exists(exe) ? QDir::toNativeSeparators(exe) : QString();
}

QVariantMap Components::installed() const
{
    return {
        {QStringLiteral("mpc"), m_settings.value(QStringLiteral("components/mpc-hc")).toString()},
        {QStringLiteral("ffmpeg"), m_settings.value(QStringLiteral("components/ffmpeg")).toString()},
    };
}

void Components::setStatus(const QString &status, int progress)
{
    m_status = status;
    m_progress = progress;
    Q_EMIT changed();
}

QNetworkReply *Components::get(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", userAgent);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(120000);
    return m_network.get(request);
}

void Components::ensure(Kind kind, std::function<void(bool)> done)
{
    if (!executable(kind).isEmpty()) {
        if (done) {
            done(true);
        }
        return;
    }
    if (done) {
        m_waiting[kind].append(std::move(done));
    }
    enqueue(kind, false);
}

void Components::startBackground()
{
    // Ontbrekende onderdelen ophalen; bestaande eens per week op een nieuwe versie controleren
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    for (Kind kind : {MpcHc, Ffmpeg}) {
        const QString key = QStringLiteral("components/checked-") + QString::number(kind);
        const bool present = !executable(kind).isEmpty();
        if (!present) {
            enqueue(kind, false);
        } else if (now - m_settings.value(key).toLongLong() > 7 * 24 * 3600) {
            enqueue(kind, true);
        }
    }
}

void Components::enqueue(Kind kind, bool updateCheckOnly)
{
    if (m_current == kind) {
        return;
    }
    for (Job &job : m_queue) {
        if (job.kind == kind) {
            job.updateCheckOnly = job.updateCheckOnly && updateCheckOnly;
            return;
        }
    }
    // Een download waar iemand op wacht gaat voor
    if (!updateCheckOnly) {
        m_queue.prepend({kind, false});
    } else {
        m_queue.append({kind, true});
    }
    Q_EMIT changed();
    if (m_current < 0) {
        QTimer::singleShot(0, this, &Components::next);
    }
}

void Components::next()
{
    if (m_current >= 0 || m_queue.isEmpty()) {
        return;
    }
    const Job job = m_queue.takeFirst();
    m_current = job.kind;
    m_currentUpdateOnly = job.updateCheckOnly;
    fetchRelease();
}

void Components::finish(bool ok)
{
    const Kind kind = Kind(m_current);
    m_current = -1;
    if (ok || m_currentUpdateOnly) {
        m_settings.setValue(QStringLiteral("components/checked-") + QString::number(kind), QDateTime::currentSecsSinceEpoch());
    }
    setStatus(ok || m_currentUpdateOnly ? QString() : tr("%1 kon niet worden opgehaald").arg(name(kind)), -1);
    const bool present = !executable(kind).isEmpty();
    const auto callbacks = m_waiting.take(kind);
    for (const auto &cb : callbacks) {
        cb(present);
    }
    Q_EMIT ready(kind, present);
    if (!ok && !m_currentUpdateOnly) {
        // Melding even laten staan
        QTimer::singleShot(8000, this, [this] {
            if (m_current < 0) {
                setStatus(QString(), -1);
            }
        });
    }
    QTimer::singleShot(0, this, &Components::next);
}

// Nieuwste uitgave op GitHub opzoeken en het juiste pakket kiezen
void Components::fetchRelease()
{
    const Kind kind = Kind(m_current);
    const QString repo = kind == MpcHc ? QStringLiteral("clsid2/mpc-hc") : QStringLiteral("BtbN/FFmpeg-Builds");
    const QString api = QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(repo);
    if (!m_currentUpdateOnly) {
        setStatus(tr("%1 wordt opgehaald…").arg(name(kind)), -1);
    }
    QNetworkReply *reply = get(QUrl(api));
    connect(reply, &QNetworkReply::finished, this, [this, reply, kind] {
        reply->deleteLater();
        const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
        const QString tag = release.value(QStringLiteral("tag_name")).toString();
        QString url;
        QString chosen;
        QHash<QString, QString> digests;   // naam → "sha256:…" (door GitHub berekend)
        for (const QJsonValue &v : release.value(QStringLiteral("assets")).toArray()) {
            digests.insert(v.toObject().value(QStringLiteral("name")).toString(), v.toObject().value(QStringLiteral("digest")).toString());
            const QString n = v.toObject().value(QStringLiteral("name")).toString();
            const QString u = v.toObject().value(QStringLiteral("browser_download_url")).toString();
            if (kind == MpcHc) {
                // MPC-HC.2.8.5.x64.zip (geen pdb-/installer-pakketten)
                if (QRegularExpression(QStringLiteral(R"(x64\.zip$)")).match(n).hasMatch()
                    && !n.contains(QLatin1String("pdb"), Qt::CaseInsensitive)) {
                    url = u;
                    chosen = n;
                    break;
                }
            } else {
                // Stabiele uitgave, bv. ffmpeg-n8.0-latest-win64-gpl-8.0.zip (hoogste versie); anders master
                const auto m = QRegularExpression(QStringLiteral(R"(^ffmpeg-n(\d+)\.(\d+).*-win64-gpl-[\d.]+\.zip$)")).match(n);
                if (m.hasMatch()) {
                    if (chosen.isEmpty() || !chosen.startsWith(QLatin1String("ffmpeg-n")) || n > chosen) {
                        url = u;
                        chosen = n;
                    }
                } else if (n == QLatin1String("ffmpeg-master-latest-win64-gpl.zip") && chosen.isEmpty()) {
                    url = u;
                    chosen = n;
                }
            }
        }
        m_expectedDigest = digests.value(chosen);
        if (url.isEmpty() && kind != MpcHc) {
            // GitHub-API geweigerd (bv. limiet bereikt): vaste downloadlink van de nieuwste master-bouw
            chosen = QStringLiteral("ffmpeg-master-latest-win64-gpl.zip");
            url = QStringLiteral("https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/") + chosen;
            qInfo() << "Components: API unavailable, using direct ffmpeg link";
        }
        const QString key = kind == MpcHc ? QStringLiteral("components/mpc-hc") : QStringLiteral("components/ffmpeg");
        const QString version = kind == MpcHc ? tag : chosen;
        qInfo().noquote() << "Components:" << name(kind) << "latest" << tag << chosen
                          << "(have" << m_settings.value(key).toString() << ")";
        if (url.isEmpty() && kind == MpcHc) {
            // GitHub-API geweigerd: de gewone pagina "nieuwste uitgave" stuurt door naar .../tag/<versie>;
            // daarmee de downloadlink MPC-HC.<versie>.x64.zip maken
            qInfo() << "Components: API unavailable, finding MPC-HC version via release page";
            QNetworkReply *page = get(QUrl(QStringLiteral("https://github.com/clsid2/mpc-hc/releases/latest")));
            connect(page, &QNetworkReply::finished, this, [this, page, key] {
                page->deleteLater();
                const QString latest = page->url().path().section(QLatin1Char('/'), -1);
                if (page->error() != QNetworkReply::NoError || latest.isEmpty() || latest == QLatin1String("latest")) {
                    qWarning() << "Components: no package found" << page->errorString();
                    finish(false);
                    return;
                }
                const QString version = latest.startsWith(QLatin1Char('v')) ? latest.mid(1) : latest;
                if (!executable(MpcHc).isEmpty() && m_settings.value(key).toString() == latest) {
                    finish(true);
                    return;
                }
                setStatus(tr("%1 wordt opgehaald…").arg(name(MpcHc)), 0);
                download(QStringLiteral("https://github.com/clsid2/mpc-hc/releases/download/%1/MPC-HC.%2.x64.zip").arg(latest, version), latest);
            });
            return;
        }
        if (url.isEmpty()) {
            qWarning() << "Components: no package found" << reply->errorString();
            finish(false);
            return;
        }
        if (!executable(kind).isEmpty() && m_settings.value(key).toString() == version) {
            finish(true); // al de nieuwste
            return;
        }
        if (m_currentUpdateOnly && !m_status.isEmpty()) {
            // ok
        }
        setStatus(tr("%1 wordt opgehaald…").arg(name(kind)), 0);
        download(url, version);
    });
}

void Components::download(const QString &url, const QString &tag)
{
    const Kind kind = Kind(m_current);
    QDir().mkpath(m_dataDir + QStringLiteral("/downloads"));
    const QString file = m_dataDir + QStringLiteral("/downloads/") + url.section(QLatin1Char('/'), -1);
    auto *out = new QFile(file, this);
    if (!out->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        delete out;
        finish(false);
        return;
    }
    QNetworkReply *reply = get(QUrl(url));
    connect(reply, &QNetworkReply::readyRead, this, [reply, out] { out->write(reply->readAll()); });
    connect(reply, &QNetworkReply::downloadProgress, this, [this, kind](qint64 got, qint64 total) {
        if (total > 0) {
            const int pct = int(got * 100 / total);
            if (pct != m_progress) {
                setStatus(tr("%1 wordt opgehaald… %2%").arg(name(kind)).arg(pct), pct);
            }
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, out, file, tag] {
        reply->deleteLater();
        out->write(reply->readAll());
        out->close();
        out->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Components: download failed" << reply->url() << reply->errorString();
            QFile::remove(file);
            finish(false);
            return;
        }
        // Controlegetal van GitHub (als bekend): een beschadigd of vervalst pakket niet uitpakken
        const QString expected = std::exchange(m_expectedDigest, QString());
        if (expected.startsWith(QLatin1String("sha256:"))) {
            QFile f(file);
            QCryptographicHash sha(QCryptographicHash::Sha256);
            if (!f.open(QIODevice::ReadOnly) || !sha.addData(&f) || QString::fromLatin1(sha.result().toHex()) != expected.mid(7).toLower()) {
                qWarning() << "Components: checksum mismatch" << file;
                f.close();
                QFile::remove(file);
                finish(false);
                return;
            }
            qInfo() << "Components: checksum ok";
        }
        extract(file, tag);
    });
}

// Uitpakken met tar.exe (zit in Windows 10/11) en op de vaste plek zetten
void Components::extract(const QString &archive, const QString &tag)
{
    const Kind kind = Kind(m_current);
    setStatus(tr("%1 wordt geïnstalleerd…").arg(name(kind)), -1);
    const QString tmp = m_dataDir + QStringLiteral("/downloads/unz-") + QString::number(kind);
    QDir(tmp).removeRecursively();
    QDir().mkpath(tmp);
    auto *tar = new QProcess(this);
    QString program = QStandardPaths::findExecutable(QStringLiteral("tar"));
    if (program.isEmpty()) {
        program = QStringLiteral("C:/Windows/System32/tar.exe");
    }
    tar->setProgram(program);
    tar->setArguments({QStringLiteral("-xf"), QDir::toNativeSeparators(archive), QStringLiteral("-C"), QDir::toNativeSeparators(tmp)});
    connect(tar, &QProcess::finished, this, [this, tar, archive, tmp, tag, kind](int code) {
        tar->deleteLater();
        QFile::remove(archive);
        // Map met het programma zoeken
        const QString wanted = kind == MpcHc ? QStringLiteral("mpc-hc64.exe") : QStringLiteral("ffmpeg.exe");
        QString found;
        QDirIterator it(tmp, {wanted}, QDir::Files, QDirIterator::Subdirectories);
        if (it.hasNext()) {
            found = QFileInfo(it.next()).absolutePath();
        }
        if (code != 0 || found.isEmpty()) {
            qWarning() << "Components: extract failed" << code << tar->readAllStandardError().left(300);
            QDir(tmp).removeRecursively();
            finish(false);
            return;
        }
        const QString target = dir(kind);
        // Instellingen van MPC-HC (waar je gebleven was) bewaren
        QByteArray ini;
        if (QFile f(target + QStringLiteral("/mpc-hc64.ini")); kind == MpcHc && f.open(QIODevice::ReadOnly)) {
            ini = f.readAll();
        }
        if (kind == MpcHc) {
            QDir(target).removeRecursively();
            if (!QDir().rename(found, target)) {
                qWarning() << "Components: could not move" << found << "to" << target;
                finish(false);
                return;
            }
            if (!ini.isEmpty()) {
                if (QFile f(target + QStringLiteral("/mpc-hc64.ini")); f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    f.write(ini);
                }
            }
        } else {
            // Van ffmpeg alleen ffmpeg.exe en de licentie
            QDir().mkpath(target);
            QFile::remove(target + QStringLiteral("/ffmpeg.exe"));
            QFile::rename(found + QStringLiteral("/ffmpeg.exe"), target + QStringLiteral("/ffmpeg.exe"));
            QDirIterator lic(tmp, {QStringLiteral("LICENSE*")}, QDir::Files, QDirIterator::Subdirectories);
            if (lic.hasNext()) {
                QFile::remove(target + QStringLiteral("/LICENSE-ffmpeg.txt"));
                QFile::copy(lic.next(), target + QStringLiteral("/LICENSE-ffmpeg.txt"));
            }
        }
        QDir(tmp).removeRecursively();
        m_settings.setValue(kind == MpcHc ? QStringLiteral("components/mpc-hc") : QStringLiteral("components/ffmpeg"), tag);
        m_settings.sync();
        qInfo().noquote() << "Components:" << name(kind) << tag << "installed in" << target;
        finish(!executable(kind).isEmpty());
    });
    tar->start();
}

QString Components::ffmpegPath()
{
    if (Components *c = Components::instance()) {
        const QString fetched = c->executable(Components::Ffmpeg);
        if (!fetched.isEmpty()) {
            return fetched;
        }
    }
    const QString local = QCoreApplication::applicationDirPath() + QStringLiteral("/ffmpeg/ffmpeg.exe"); // tot en met 3.8.8
    if (QFileInfo::exists(local)) {
        return local;
    }
    return QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
}

quint16 Components::freeUdpPort()
{
    QUdpSocket socket;
    if (socket.bind(QHostAddress::LocalHost, 0)) {
        return socket.localPort();
    }
    return quint16(47000 + QRandomGenerator::global()->bounded(2000));
}
