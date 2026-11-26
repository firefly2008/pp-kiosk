// SPDX-License-Identifier: GPL-2.0-or-later
#include "activity.h"

#include <QEvent>

Activity::Activity(QObject *parent)
    : QObject(parent)
{
    m_last.start();
}

bool Activity::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:
    case QEvent::TouchBegin:
    case QEvent::KeyPress:
    case QEvent::Wheel:
        m_last.restart();
        // Niet bij elke muisbeweging een signaal, hooguit eens per halve seconde
        if (m_lastSignal < 0 || m_last.msecsSinceReference() - m_lastSignal > 500) {
            m_lastSignal = m_last.msecsSinceReference();
            Q_EMIT touched();
        }
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}
