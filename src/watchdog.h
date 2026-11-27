// Waakhond: als het scherm langer dan 90 seconden vastzit, start PP Kiosk vanzelf opnieuw.
// Een aparte thread kijkt of de hoofdthread nog "tikt"; zo niet, dan een nieuwe PP Kiosk starten en deze stoppen.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QThread>
#include <QTimer>
#include <atomic>

class Watchdog : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

public:
    explicit Watchdog(QObject *parent = nullptr);
    ~Watchdog() override;

    bool enabled() const { return m_enabled.load(); }
    void setEnabled(bool on);
    // Voor de rooktest: hoe lang geleden de hoofdthread voor het laatst tikte (ms)
    Q_INVOKABLE int lastBeatAge() const;

Q_SIGNALS:
    void enabledChanged();

private:
    void check();

    QTimer m_beat;
    QThread m_thread;
    std::atomic<bool> m_enabled{true};
    std::atomic<qint64> m_lastBeat{0};
    int m_limitMs = 90000;
};
