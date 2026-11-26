// SPDX-License-Identifier: GPL-2.0-or-later
#include "speaker.h"

#include <QDebug>
#include <QLocale>
#include <QTextToSpeech>
#include <QTimer>
#include <QUrl>

Speaker::Speaker(QObject *parent)
    : QObject(parent)
{
    const QStringList engines = QTextToSpeech::availableEngines();
    if (!engines.isEmpty()) {
        m_tts = new QTextToSpeech(this);
        const QLocale nl(QLocale::Dutch, QLocale::Netherlands);
        bool dutch = false;
        for (const QLocale &l : m_tts->availableLocales()) {
            if (l.language() == QLocale::Dutch) {
                m_tts->setLocale(l.territory() == QLocale::Netherlands ? nl : l);
                dutch = true;
                if (l.territory() == QLocale::Netherlands) {
                    break;
                }
            }
        }
        m_tts->setRate(-0.1);   // iets rustiger
        qInfo() << "Speaker: engines" << engines << "dutch voice" << dutch;
    } else {
        qInfo() << "Speaker: no text-to-speech engine";
    }
    m_chime.setSource(QUrl(QStringLiteral("qrc:/assets/chime.wav")));
    m_chime.setVolume(0.8);
}

void Speaker::announce(const QString &text, bool chime)
{
    qInfo().noquote() << "Speaker: announce" << text;
    if (chime) {
        m_chime.play();
        QTimer::singleShot(1300, this, [this, text] { say(text); });
    } else {
        say(text);
    }
}

void Speaker::say(const QString &text)
{
    if (m_tts && !text.isEmpty()) {
        m_tts->stop();
        m_tts->say(text);
    }
}

void Speaker::stop()
{
    if (m_tts) {
        m_tts->stop();
    }
}
