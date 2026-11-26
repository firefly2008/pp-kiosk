// SPDX-License-Identifier: GPL-2.0-or-later
#include "recorder.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QLocale>
#include <QSet>
#include <QStorageInfo>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>
#include <QUdpSocket>
#include <QRandomGenerator>
#include <QJsonArray>
#include <QJsonDocument>

#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "components.h"
#include "mpcplayer.h"

namespace {
QString sanitize(QString name)
{
    static const QString forbidden = QStringLiteral("<>:\"/\\|?*[]");
    for (QChar &c : name) {
        if (forbidden.contains(c) || c.unicode() < 32) {
            c = QLatin1Char('_');
        }
    }
    return name.simplified().left(80);
}
}

Recorder::Recorder(const QString &dataDir, QObject *parent)
    : QObject(parent)
    , m_settings(dataDir + QStringLiteral("/settings.ini"), QSettings::IniFormat)
    , m_dataDir(dataDir)
{
    // Geplande opnames inlezen en elke 15 seconden kijken of er een moet beginnen
    QFile file(m_dataDir + QStringLiteral("/recordings-schedule.json"));
    if (file.open(QIODevice::ReadOnly)) {
        m_schedule = QJsonDocument::fromJson(file.readAll()).array().toVariantList();
    }
    m_scheduleTimer.setInterval(15000);
    connect(&m_scheduleTimer, &QTimer::timeout, this, &Recorder::checkSchedule);
    m_scheduleTimer.start();
    QTimer::singleShot(3000, this, &Recorder::checkSchedule);
    m_folder = m_settings.value(QStringLiteral("recorder/folder")).toString();

    m_clock.setInterval(1000);
    connect(&m_clock, &QTimer::timeout, this, &Recorder::tick);
    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, &Recorder::launch);
    m_endTimer.setSingleShot(true);
    connect(&m_endTimer, &QTimer::timeout, this, &Recorder::stop);
    // Opruimen: kort na de start en daarna elk uur kijken
    m_cleanupTimer.setInterval(60 * 60 * 1000);
    connect(&m_cleanupTimer, &QTimer::timeout, this, [this] { checkCleanup(false); });
    m_cleanupTimer.start();
    QTimer::singleShot(90 * 1000, this, [this] { checkCleanup(false); });
}

void Recorder::setKeepDays(int days)
{
    m_settings.setValue(QStringLiteral("recorder/keepDays"), qMax(0, days));
    m_settings.remove(QStringLiteral("recorder/cleanupAsked"));
    Q_EMIT cleanupSettingsChanged();
}

void Recorder::setMinFreeGb(int gb)
{
    m_settings.setValue(QStringLiteral("recorder/minFreeGb"), qMax(0, gb));
    m_settings.remove(QStringLiteral("recorder/cleanupAsked"));
    Q_EMIT cleanupSettingsChanged();
}

QString Recorder::diskText() const
{
    const QString dir = m_folder.isEmpty() ? defaultFolder() : m_folder;
    QStorageInfo storage(QDir(dir).exists() ? dir : QDir::homePath());
    if (!storage.isValid()) {
        return {};
    }
    qint64 used = 0;
    for (const QVariant &r : recordings()) {
        used += r.toMap().value(QStringLiteral("size")).toLongLong();
    }
    return tr("Opnames: %1 GB · Vrij op de schijf: %2 GB")
        .arg(QString::number(used / 1e9, 'f', 1), QString::number(storage.bytesAvailable() / 1e9, 'f', 0));
}

QVariantList Recorder::cleanupCandidates() const
{
    QVariantList result;
    if (m_folder.isEmpty()) {
        return result;
    }
    QFileInfoList files = QDir(m_folder).entryInfoList({QStringLiteral("*.mp4"), QStringLiteral("*.ts"), QStringLiteral("*.mkv")},
                                                      QDir::Files, QDir::Time | QDir::Reversed);   // oudste eerst
    QSet<QString> chosen;
    const QDateTime now = QDateTime::currentDateTime();
    auto add = [&](const QFileInfo &f, const QString &reason) {
        const QString path = QDir::toNativeSeparators(f.absoluteFilePath());
        if (chosen.contains(path) || (m_active && m_parts.contains(path))) {
            return;
        }
        chosen.insert(path);
        result.append(QVariantMap{{QStringLiteral("path"), path}, {QStringLiteral("name"), f.completeBaseName()},
                                  {QStringLiteral("reason"), reason}, {QStringLiteral("size"), f.size()},
                                  {QStringLiteral("date"), QLocale().toString(f.lastModified(), QStringLiteral("d MMM yyyy"))}});
    };
    if (keepDays() > 0) {
        for (const QFileInfo &f : files) {
            if (f.lastModified().daysTo(now) > keepDays()) {
                add(f, tr("ouder dan %1 dagen").arg(keepDays()));
            }
        }
    }
    if (minFreeGb() > 0) {
        QStorageInfo storage(m_folder);
        qint64 freeBytes = storage.isValid() ? storage.bytesAvailable() : -1;
        for (const QVariant &v : std::as_const(result)) {
            freeBytes += v.toMap().value(QStringLiteral("size")).toLongLong();
        }
        const qint64 needed = qint64(minFreeGb()) * 1000 * 1000 * 1000;
        for (const QFileInfo &f : files) {
            if (freeBytes < 0 || freeBytes >= needed) {
                break;
            }
            const QString path = QDir::toNativeSeparators(f.absoluteFilePath());
            if (chosen.contains(path)) {
                continue;
            }
            add(f, tr("schijf bijna vol"));
            freeBytes += f.size();
        }
    }
    return result;
}

int Recorder::cleanup(const QStringList &paths)
{
    int removed = 0;
    for (const QString &path : paths) {
        if (m_active && m_parts.contains(path)) {
            continue;
        }
        // Alleen bestanden in de opnamemap
        if (!m_folder.isEmpty() && QDir::cleanPath(QFileInfo(path).absolutePath()).compare(QDir::cleanPath(m_folder), Qt::CaseInsensitive) == 0
            && QFile::remove(path)) {
            ++removed;
        }
    }
    qInfo() << "Recordings cleaned up:" << removed;
    m_settings.remove(QStringLiteral("recorder/cleanupAsked"));
    refresh();
    return removed;
}

void Recorder::checkCleanup(bool force)
{
    const QVariantList list = cleanupCandidates();
    if (list.isEmpty()) {
        return;
    }
    // Hooguit eens per dag vragen (tenzij de lijst veranderd is)
    const QString today = QDate::currentDate().toString(Qt::ISODate) + QLatin1Char('/') + QString::number(list.size());
    if (!force && m_settings.value(QStringLiteral("recorder/cleanupAsked")).toString() == today) {
        return;
    }
    m_settings.setValue(QStringLiteral("recorder/cleanupAsked"), today);
    const QString reason = list.first().toMap().value(QStringLiteral("reason")).toString();
    qInfo() << "Cleanup suggested:" << list.size() << reason;
    Q_EMIT cleanupSuggested(int(list.size()), reason);
}

Recorder::~Recorder()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        // Afsluiten van Kijkluister: ffmpeg netjes laten eindigen
        m_process->write("q\n");
        m_process->closeWriteChannel();
        m_process->waitForFinished(5000);
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
        }
    }
}

quint16 Recorder::freeUdpPort()
{
    return Components::freeUdpPort();
}

QString Recorder::ffmpegPath() const
{
    return Components::ffmpegPath();
}

bool Recorder::available() const
{
    return !ffmpegPath().isEmpty() || Components::instance(); // anders wordt ffmpeg eerst opgehaald
}

QString Recorder::defaultFolder() const
{
    const QString videos = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    return QDir::toNativeSeparators((videos.isEmpty() ? QDir::homePath() : videos) + QStringLiteral("/Kijkluister opnames"));
}

void Recorder::setFolder(const QString &folder)
{
    const QString native = QDir::toNativeSeparators(folder.trimmed());
    if (native == m_folder) {
        return;
    }
    m_folder = native;
    m_settings.setValue(QStringLiteral("recorder/folder"), m_folder);
    m_settings.sync();
    Q_EMIT folderChanged();
    refresh();
}

QString Recorder::localPath(const QUrl &url) const
{
    return QDir::toNativeSeparators(url.isLocalFile() ? url.toLocalFile() : url.toString());
}

void Recorder::setStatus(const QString &status)
{
    m_status = status;
    Q_EMIT stateChanged();
}

bool Recorder::start(const QString &channel, const QString &url, int minutes, const QString &title)
{
    if (m_active) {
        setStatus(tr("Er loopt al een opname"));
        return false;
    }
    if (ffmpegPath().isEmpty()) {
        Components *c = Components::instance();
        if (!c) {
            setStatus(tr("ffmpeg niet gevonden"));
            return false;
        }
        // Eerst ffmpeg ophalen (eenmalig), dan de opname starten
        setStatus(tr("ffmpeg wordt opgehaald, de opname begint zo…"));
        c->ensure(Components::Ffmpeg, [this, channel, url, minutes, title](bool ok) {
            if (ok) {
                start(channel, url, minutes, title);
            } else {
                setStatus(tr("ffmpeg kon niet worden opgehaald"));
            }
        });
        return true;
    }
    if (m_folder.isEmpty()) {
        setFolder(defaultFolder());
    }
    if (!QDir().mkpath(m_folder)) {
        setStatus(tr("Map %1 kan niet worden gemaakt").arg(m_folder));
        return false;
    }
    m_channel = channel;
    m_url = url;
    m_started = QDateTime::currentDateTime();
    m_endAt = minutes > 0 ? m_started.addSecs(minutes * 60) : QDateTime();
    m_retries = 0;
    m_part = 0;
    m_parts.clear();
    m_stopping = false;
    m_active = true;
    m_title = title;
    m_file = QDir::toNativeSeparators(m_folder + QLatin1Char('/') + (title.isEmpty() ? QString() : sanitize(title) + QStringLiteral(" - "))
                                      + sanitize(channel) + QStringLiteral(" - ")
                                      + m_started.toString(QStringLiteral("yyyy-MM-dd HH.mm.ss")) + QStringLiteral(".mp4"));
    if (minutes > 0) {
        m_endTimer.start(minutes * 60 * 1000);
    }
    m_clock.start();
    keepAwake(true);
    qInfo().noquote() << "Recorder: start" << channel << "->" << m_file << "minutes" << minutes;
    launch();
    return true;
}

void Recorder::launch()
{
    if (!m_active || m_stopping) {
        return;
    }
    delete m_process;
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, &QProcess::finished, this, &Recorder::onFinished);
    connect(m_process, &QProcess::readyReadStandardError, this, [this] {
        m_lastStderr += m_process->readAllStandardError();
        if (m_lastStderr.size() > 4000) {
            m_lastStderr = m_lastStderr.right(2000);
        }
    });
    // Na een onderbreking een nieuw deelbestand, zodat het eerste deel bruikbaar blijft
    QString target = m_file;
    if (m_part > 0) {
        target = m_file.left(m_file.size() - 4) + QStringLiteral(" (deel %1).mp4").arg(m_part + 1);
    }
    if (!m_parts.contains(target)) {
        m_parts << target;
    }
    // Zelfde opzet als IPTV-Recorder: niets hercoderen, alleen kopiëren naar MP4.
    // -movflags: ook een afspeelbaar bestand als de stroom onverwacht stopt.
    QStringList args{
        QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("warning"), QStringLiteral("-y"),
        QStringLiteral("-user_agent"), QStringLiteral("VLC/3.0.9 LibVLC/3.0.9"),
        QStringLiteral("-reconnect"), QStringLiteral("1"), QStringLiteral("-reconnect_streamed"), QStringLiteral("1"),
        QStringLiteral("-reconnect_delay_max"), QStringLiteral("10"),
        QStringLiteral("-rw_timeout"), QStringLiteral("20000000"),
        QStringLiteral("-i"), m_url,
        // Eén keer ophalen, twee kanten op. 1: het bestand (MP4, niets hercoderen).
        QStringLiteral("-map"), QStringLiteral("0:v?"), QStringLiteral("-map"), QStringLiteral("0:a?"),
        QStringLiteral("-c"), QStringLiteral("copy"),
        QStringLiteral("-bsf:a"), QStringLiteral("aac_adtstoasc"),
        QStringLiteral("-movflags"), QStringLiteral("+frag_keyframe+empty_moov+default_base_moof"),
        QStringLiteral("-f"), QStringLiteral("mp4"),
        target,
        // 2: een lokale doorgifte (MPEG-TS over UDP) waar de speler op overschakelt, zodat er
        // één verbinding met de aanbieder blijft (veel abonnementen staan er maar één toe).
        // Via de fifo-muxer, zodat een haperende doorgifte de opname nooit stopt.
        QStringLiteral("-map"), QStringLiteral("0:v?"), QStringLiteral("-map"), QStringLiteral("0:a?"),
        QStringLiteral("-c"), QStringLiteral("copy"),
        QStringLiteral("-f"), QStringLiteral("fifo"), QStringLiteral("-fifo_format"), QStringLiteral("mpegts"),
        QStringLiteral("-drop_pkts_on_overflow"), QStringLiteral("1"),
        QStringLiteral("-attempt_recovery"), QStringLiteral("1"), QStringLiteral("-recover_any_error"), QStringLiteral("1"),
        QStringLiteral("-recovery_wait_time"), QStringLiteral("1"),
        m_relayUrl + QStringLiteral("?pkt_size=1316"),
    };
    // Doorgifte naar de speler op een vrije poort (een vaste poort kan nog bezet zijn);
    // ffmpeg verstuurt vanaf een andere vrije poort, zodat de speler de zijne kan openen
    if (m_part == 0 || m_relayUrl.isEmpty()) {
        const quint16 relay = freeUdpPort();
        m_relayUrl = QStringLiteral("udp://127.0.0.1:%1").arg(relay);
    }
    {
        const QString target = m_relayUrl + QStringLiteral("?pkt_size=1316&localport=") + QString::number(freeUdpPort());
        args[args.size() - 1] = target;
    }
    m_lastStderr.clear();
    setStatus(m_part > 0 ? tr("Opname hervat (deel %1)").arg(m_part + 1) : tr("Opname loopt"));
    m_process->start(ffmpegPath(), args);
    if (!m_process->waitForStarted(5000)) {
        qWarning() << "Recorder: ffmpeg did not start" << m_process->errorString();
        m_active = false;
        m_clock.stop();
        m_endTimer.stop();
        setStatus(tr("ffmpeg kon niet starten"));
        Q_EMIT finished(m_file, false);
    }
}

void Recorder::onFinished(int code, QProcess::ExitStatus status)
{
    Q_UNUSED(status)
    const QString err = QString::fromUtf8(m_lastStderr).simplified().right(300);
    qInfo().noquote() << "Recorder: ffmpeg exited" << code << (err.isEmpty() ? QString() : QStringLiteral("stderr: ") + err);
    refresh();
    if (!m_active) {
        return;
    }
    if (m_stopping || (m_endAt.isValid() && QDateTime::currentDateTime() >= m_endAt)) {
        m_active = false;
        m_stopping = false;
        m_clock.stop();
        m_endTimer.stop();
        setStatus(tr("Opname opgeslagen"));
        keepAwake(false);
        m_scheduledId.clear();
        Q_EMIT finished(m_file, true);
        return;
    }
    // Stream viel weg: net als IPTV-Recorder opnieuw proberen, met korte tussenpozen
    ++m_retries;
    ++m_part;
    if (m_retries > 60) {
        m_active = false;
        m_clock.stop();
        m_endTimer.stop();
        setStatus(tr("Opname gestopt: de zender blijft wegvallen"));
        Q_EMIT finished(m_file, !m_parts.isEmpty());
        return;
    }
    setStatus(tr("Zender weggevallen, opnieuw proberen (%1)…").arg(m_retries));
    m_retryTimer.start(5000);
}

void Recorder::stop()
{
    if (!m_active) {
        return;
    }
    m_stopping = true;
    m_endTimer.stop();
    m_retryTimer.stop();
    if (m_process && m_process->state() != QProcess::NotRunning) {
        setStatus(tr("Opname afronden…"));
        m_process->write("q\n");   // netjes afsluiten, zoals IPTV-Recorder doet
        QTimer::singleShot(15000, this, [this] {
            if (m_process && m_process->state() != QProcess::NotRunning) {
                m_process->kill();
            }
        });
    } else {
        onFinished(0, QProcess::NormalExit);
    }
}

int Recorder::elapsedSeconds() const
{
    return m_active ? int(m_started.secsTo(QDateTime::currentDateTime())) : 0;
}

int Recorder::remainingSeconds() const
{
    return m_active && m_endAt.isValid() ? int(qMax<qint64>(0, QDateTime::currentDateTime().secsTo(m_endAt))) : -1;
}

qint64 Recorder::bytes() const
{
    qint64 total = 0;
    for (const QString &part : m_parts) {
        total += QFileInfo(part).size();
    }
    return total;
}

QVariantList Recorder::recordings() const
{
    QVariantList list;
    if (m_folder.isEmpty()) {
        return list;
    }
    const QFileInfoList files = QDir(m_folder).entryInfoList({QStringLiteral("*.mp4"), QStringLiteral("*.ts"), QStringLiteral("*.mkv")},
                                                            QDir::Files, QDir::Time);
    for (const QFileInfo &f : files) {
        list.append(QVariantMap{
            {QStringLiteral("name"), f.completeBaseName()},
            {QStringLiteral("path"), QDir::toNativeSeparators(f.absoluteFilePath())},
            {QStringLiteral("size"), f.size()},
            {QStringLiteral("date"), QLocale().toString(f.lastModified(), QStringLiteral("d MMM yyyy HH:mm"))},
            {QStringLiteral("busy"), m_active && m_parts.contains(QDir::toNativeSeparators(f.absoluteFilePath()))},
        });
    }
    return list;
}

void Recorder::refresh()
{
    Q_EMIT recordingsChanged();
}

void Recorder::openFolder()
{
    if (!m_folder.isEmpty()) {
        QDir().mkpath(m_folder);
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_folder));
    }
}

bool Recorder::play(const QString &path)
{
    if (MpcPlayer *mpc = MpcPlayer::instance(); mpc && mpc->play({path}, true)) {
        return true;
    }
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

bool Recorder::remove(const QString &path)
{
    if (m_active && m_parts.contains(path)) {
        return false;
    }
    const bool ok = QFile::remove(path);
    refresh();
    return ok;
}

// ---------------------------------------------------------------------------
// Geplande opnames (vanuit de tv-gids)
// ---------------------------------------------------------------------------

void Recorder::keepAwake(bool on)
{
#ifdef Q_OS_WIN
    // Pc niet in slaap laten vallen zolang er opgenomen wordt
    SetThreadExecutionState(on ? (ES_CONTINUOUS | ES_SYSTEM_REQUIRED) : ES_CONTINUOUS);
#else
    Q_UNUSED(on)
#endif
}

void Recorder::saveSchedule()
{
    QFile file(m_dataDir + QStringLiteral("/recordings-schedule.json"));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(QJsonArray::fromVariantList(m_schedule)).toJson(QJsonDocument::Indented));
    }
    Q_EMIT scheduleChanged();
}

bool Recorder::isScheduled(const QString &url, qint64 start) const
{
    for (const QVariant &v : m_schedule) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("url")).toString() == url && m.value(QStringLiteral("start")).toLongLong() == start) {
            return true;
        }
    }
    return false;
}

bool Recorder::addToSchedule(const QString &channel, const QString &url, const QString &title, qint64 start, qint64 stop)
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (url.isEmpty() || stop <= now || stop <= start || isScheduled(url, start)) {
        return false;
    }
    const QString id = QString::number(start) + QLatin1Char('-') + QString::number(qHash(url), 16);
    m_schedule.append(QVariantMap{
        {QStringLiteral("id"), id},
        {QStringLiteral("channel"), channel},
        {QStringLiteral("url"), url},
        {QStringLiteral("title"), title},
        {QStringLiteral("start"), start},
        {QStringLiteral("stop"), stop},
        {QStringLiteral("state"), QStringLiteral("gepland")},
    });
    std::sort(m_schedule.begin(), m_schedule.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("start")).toLongLong() < b.toMap().value(QStringLiteral("start")).toLongLong();
    });
    qInfo().noquote() << "Recorder: scheduled" << title << channel
                      << QDateTime::fromSecsSinceEpoch(start).toString(Qt::ISODate) << "-" << QDateTime::fromSecsSinceEpoch(stop).toString(Qt::ISODate);
    saveSchedule();
    checkSchedule(); // loopt het programma al: meteen beginnen
    return true;
}

void Recorder::removeFromSchedule(const QString &id)
{
    for (int i = 0; i < m_schedule.size(); ++i) {
        if (m_schedule.at(i).toMap().value(QStringLiteral("id")).toString() == id) {
            m_schedule.removeAt(i);
            if (id == m_scheduledId && m_active) {
                stop(); // de lopende opname van dit programma ook stoppen
            }
            saveSchedule();
            return;
        }
    }
}

void Recorder::toggleSchedule(const QString &channel, const QString &url, const QString &title, qint64 start, qint64 stop)
{
    for (const QVariant &v : std::as_const(m_schedule)) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("url")).toString() == url && m.value(QStringLiteral("start")).toLongLong() == start) {
            removeFromSchedule(m.value(QStringLiteral("id")).toString());
            return;
        }
    }
    addToSchedule(channel, url, title, start, stop);
}

void Recorder::checkSchedule()
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    bool changed = false;
    for (int i = m_schedule.size() - 1; i >= 0; --i) {
        QVariantMap m = m_schedule.at(i).toMap();
        const qint64 begin = m.value(QStringLiteral("start")).toLongLong() - 120;   // 2 min eerder
        const qint64 end = m.value(QStringLiteral("stop")).toLongLong() + 300;      // 5 min later
        const QString state = m.value(QStringLiteral("state")).toString();
        if (now >= end) {
            // Afgelopen (opgenomen of gemist): uit de planning
            qInfo().noquote() << "Recorder: schedule done" << m.value(QStringLiteral("title")).toString() << state;
            m_schedule.removeAt(i);
            changed = true;
            continue;
        }
        if (now < begin || state == QLatin1String("bezig")) {
            continue;
        }
        if (m_active) {
            if (state != QLatin1String("overlap")) {
                m[QStringLiteral("state")] = QStringLiteral("overlap");
                m_schedule[i] = m;
                changed = true;
                qWarning().noquote() << "Recorder: scheduled" << m.value(QStringLiteral("title")).toString()
                                     << "skipped, already recording" << m_channel;
            }
            continue;
        }
        const int minutes = int((end - now + 59) / 60);
        m_scheduledId = m.value(QStringLiteral("id")).toString();
        m[QStringLiteral("state")] = QStringLiteral("bezig");
        m_schedule[i] = m;
        changed = true;
        qInfo().noquote() << "Recorder: scheduled start" << m.value(QStringLiteral("title")).toString() << minutes << "min";
        if (!start(m.value(QStringLiteral("channel")).toString(), m.value(QStringLiteral("url")).toString(), minutes,
                   m.value(QStringLiteral("title")).toString())) {
            m[QStringLiteral("state")] = QStringLiteral("mislukt");
            m_schedule[i] = m;
            m_scheduledId.clear();
        }
    }
    if (changed) {
        saveSchedule();
    }
}
