// Houdt bij wanneer iemand het scherm voor het laatst aanraakte (muis, toets of touch).
// Voor de screensaver en de nachtstand.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QElapsedTimer>
#include <QObject>

class Activity : public QObject
{
    Q_OBJECT
public:
    explicit Activity(QObject *parent = nullptr);

    // Seconden sinds de laatste aanraking
    Q_INVOKABLE int idleSeconds() const { return int(m_last.elapsed() / 1000); }
    Q_INVOKABLE void reset() { m_last.restart(); }

Q_SIGNALS:
    void touched();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QElapsedTimer m_last;
    qint64 m_lastSignal = -1000;
};
