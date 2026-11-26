// Voorlezen: een geluidje en daarna de activiteit hardop ("Het is tijd voor: Lunch").
// Gebruikt de stemmen van Windows (Nederlandse stem als die er is).
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QSoundEffect>

class QTextToSpeech;

class Speaker : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)

public:
    explicit Speaker(QObject *parent = nullptr);

    bool available() const { return m_tts != nullptr; }

    // Geluidje (als chime) en daarna de tekst uitspreken
    Q_INVOKABLE void announce(const QString &text, bool chime);
    Q_INVOKABLE void say(const QString &text);
    Q_INVOKABLE void stop();

private:
    QTextToSpeech *m_tts = nullptr;
    QSoundEffect m_chime;
};
