// SPDX-License-Identifier: GPL-2.0-or-later
#include "watchdog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QProcess>
#include <cstdlib>

Watchdog::Watchdog(QObject *parent)
    : QObject(parent)
{
    const QByteArray limit = qgetenv("PPKIOSK_WATCHDOG_MS");
    if (!limit.isEmpty()) {
        m_limitMs = qMax(2000, limit.toInt());
    }
    m_lastBeat = QDateTime::currentMSecsSinceEpoch();
    m_beat.setInterval(5000);
    connect(&m_beat, &QTimer::timeout, this, [this] { m_lastBeat = QDateTime::currentMSecsSinceEpoch(); });
    m_beat.start();

    // De controle draait in een eigen thread, zodat die ook werkt als de hoofdthread vastzit
    auto *checker = new QTimer;
    checker->setInterval(10000);
    checker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::started, checker, qOverload<>(&QTimer::start));
    connect(checker, &QTimer::timeout, checker, [this] { check(); }, Qt::DirectConnection);
    connect(&m_thread, &QThread::finished, checker, &QObject::deleteLater);
    m_thread.start();
}

Watchdog::~Watchdog()
{
    m_thread.quit();
    m_thread.wait();
}

void Watchdog::setEnabled(bool on)
{
    if (m_enabled.exchange(on) != on) {
        m_lastBeat = QDateTime::currentMSecsSinceEpoch();
        qInfo() << "Watchdog:" << (on ? "on" : "off");
        Q_EMIT enabledChanged();
    }
}

int Watchdog::lastBeatAge() const
{
    return int(QDateTime::currentMSecsSinceEpoch() - m_lastBeat.load());
}

void Watchdog::check()
{
    if (!m_enabled.load()) {
        return;
    }
    const qint64 age = QDateTime::currentMSecsSinceEpoch() - m_lastBeat.load();
    if (age < m_limitMs) {
        return;
    }
    qWarning() << "Watchdog: screen stuck for" << age << "ms; restarting PP Kiosk";
    QProcess::startDetached(QCoreApplication::applicationFilePath(), QCoreApplication::arguments().mid(1));
    std::_Exit(3);
}
