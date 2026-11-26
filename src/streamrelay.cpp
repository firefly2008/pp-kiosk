// SPDX-License-Identifier: GPL-2.0-or-later
#include "streamrelay.h"

#include <QDebug>

#include "components.h"

StreamRelay::StreamRelay(QObject *parent)
    : QObject(parent)
{
}

StreamRelay::~StreamRelay()
{
    stop();
}

bool StreamRelay::running() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

QString StreamRelay::start(const QString &url)
{
    stop();
    const QString ffmpeg = Components::ffmpegPath();
    if (ffmpeg.isEmpty() || url.isEmpty()) {
        return {};
    }
    const QString relay = QStringLiteral("udp://127.0.0.1:%1").arg(Components::freeUdpPort());
    const QString target = relay + QStringLiteral("?pkt_size=1316&localport=") + QString::number(Components::freeUdpPort());
    const QStringList args{
        QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("warning"),
        QStringLiteral("-user_agent"), QStringLiteral("VLC/3.0.9 LibVLC/3.0.9"),
        QStringLiteral("-reconnect"), QStringLiteral("1"), QStringLiteral("-reconnect_streamed"), QStringLiteral("1"),
        QStringLiteral("-reconnect_delay_max"), QStringLiteral("10"),
        QStringLiteral("-rw_timeout"), QStringLiteral("20000000"),
        QStringLiteral("-i"), url,
        QStringLiteral("-map"), QStringLiteral("0:v?"), QStringLiteral("-map"), QStringLiteral("0:a?"),
        QStringLiteral("-c"), QStringLiteral("copy"),
        QStringLiteral("-f"), QStringLiteral("fifo"), QStringLiteral("-fifo_format"), QStringLiteral("mpegts"),
        QStringLiteral("-drop_pkts_on_overflow"), QStringLiteral("1"),
        QStringLiteral("-attempt_recovery"), QStringLiteral("1"), QStringLiteral("-recover_any_error"), QStringLiteral("1"),
        QStringLiteral("-recovery_wait_time"), QStringLiteral("1"),
        target,
    };
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_process->start(ffmpeg, args);
    if (!m_process->waitForStarted(5000)) {
        qWarning() << "StreamRelay: ffmpeg did not start";
        stop();
        return {};
    }
    qInfo() << "StreamRelay: started";
    return relay + QStringLiteral("?overrun_nonfatal=1&fifo_size=1000000&timeout=15000000");
}

void StreamRelay::stop()
{
    if (!m_process) {
        return;
    }
    m_process->disconnect(this);
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(2000);
    }
    m_process->deleteLater();
    m_process = nullptr;
}
