// SPDX-License-Identifier: GPL-2.0-or-later
// Kijken via een lokale doorgifte: ffmpeg haalt de zender op met een VLC-User-Agent
// (sommige aanbieders weigeren de standaard Lavf-agent met 403) en stuurt hem als
// MPEG-TS over UDP naar de eigen speler.
#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

class StreamRelay : public QObject
{
    Q_OBJECT
public:
    explicit StreamRelay(QObject *parent = nullptr);
    ~StreamRelay() override;

    // Geeft het udp-adres voor de speler, of "" als ffmpeg niet beschikbaar is
    Q_INVOKABLE QString start(const QString &url);
    Q_INVOKABLE void stop();
    Q_INVOKABLE bool running() const;

private:
    QProcess *m_process = nullptr;
};
