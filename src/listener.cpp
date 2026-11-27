// SPDX-License-Identifier: GPL-2.0-or-later
#include "listener.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QProcess>

Listener::Listener(QObject *parent)
    : QObject(parent)
{
}

Listener::~Listener()
{
    if (m_proc) {
        m_proc->disconnect(this);
        m_proc->kill();
        m_proc->waitForFinished(1000);
    }
}

QStringList Listener::phrases()
{
    return {
        QStringLiteral("zet de radio aan"), QStringLiteral("radio aan"), QStringLiteral("muziek aan"), QStringLiteral("zet de muziek aan"),
        QStringLiteral("zet de radio uit"), QStringLiteral("radio uit"), QStringLiteral("muziek uit"), QStringLiteral("zet de muziek uit"),
        QStringLiteral("stop de muziek"), QStringLiteral("stop"), QStringLiteral("volgende zender"),
        QStringLiteral("wat doen we nu"), QStringLiteral("wat gaan we doen"), QStringLiteral("wat is er nu"),
        QStringLiteral("wat doen we straks"), QStringLiteral("wat is het volgende"),
        QStringLiteral("wat eten we vandaag"), QStringLiteral("wat eten we"), QStringLiteral("wat is het eten"),
        QStringLiteral("hoe laat is het"), QStringLiteral("welke dag is het"), QStringLiteral("wat voor dag is het"), QStringLiteral("welke datum is het"),
        QStringLiteral("mijn week"), QStringLiteral("laat de week zien"), QStringLiteral("wat doen we deze week"),
        QStringLiteral("spelletjes"), QStringLiteral("ik wil een spelletje"), QStringLiteral("speel memory"), QStringLiteral("speel puzzel"),
        QStringLiteral("presentaties"), QStringLiteral("kijk en luister"),
        QStringLiteral("terug"), QStringLiteral("naar het begin"), QStringLiteral("startscherm"),
    };
}

void Listener::setState(const QString &state, const QString &message)
{
    if (state == m_state && message == m_message) {
        return;
    }
    m_state = state;
    m_message = message;
    qInfo().noquote() << "Voice: state" << state << message;
    Q_EMIT stateChanged();
}

void Listener::prepare()
{
#ifdef Q_OS_WIN
    if (m_proc) {
        return;
    }
    const QString script = QDir::tempPath() + QStringLiteral("/ppkiosk-listen.ps1");
    QFile::remove(script);
    QFile::copy(QStringLiteral(":/scripts/listen.ps1"), script);
    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_proc, &QProcess::readyRead, this, &Listener::readOutput);
    connect(m_proc, &QProcess::finished, this, [this](int code) {
        readOutput();
        m_proc->deleteLater();
        m_proc = nullptr;
        if (m_state != QLatin1String("error")) {
            setState(QStringLiteral("error"), QStringLiteral("Spraakherkenning is gestopt (code %1).").arg(code));
        }
    });
    setState(QStringLiteral("starting"));
    m_proc->start(QStringLiteral("powershell.exe"),
                  {QStringLiteral("-NoProfile"), QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"), QStringLiteral("-File"),
                   QDir::toNativeSeparators(script), QStringLiteral("-Phrases"), phrases().join(QLatin1Char('|'))});
#else
    setState(QStringLiteral("error"), QStringLiteral("Spraakbediening werkt alleen op Windows."));
#endif
}

void Listener::listen()
{
    if (m_state == QLatin1String("listening")) {
        return;
    }
    if (m_state == QLatin1String("error") && m_proc == nullptr) {
        // Opnieuw proberen (bijvoorbeeld net Nederlandse spraak geïnstalleerd)
        m_state = QStringLiteral("off");
    }
    if (m_state != QLatin1String("ready")) {
        m_wantListen = true;
        prepare();
        return;
    }
    m_proc->write("listen\n");
    setState(QStringLiteral("listening"));
}

void Listener::readOutput()
{
    if (!m_proc) {
        return;
    }
    while (m_proc->canReadLine()) {
        const QString line = QString::fromUtf8(m_proc->readLine()).trimmed();
        const QStringList parts = line.split(QLatin1Char('\t'));
        const QString kind = parts.value(0);
        if (kind == QLatin1String("READY")) {
            setState(QStringLiteral("ready"), parts.value(1));
            if (m_wantListen) {
                m_wantListen = false;
                listen();
            }
        } else if (kind == QLatin1String("HEARD")) {
            setState(QStringLiteral("ready"));
            qInfo().noquote() << "Voice: heard" << parts.value(1) << parts.value(2);
            Q_EMIT heard(parts.value(1));
        } else if (kind == QLatin1String("NOTHING")) {
            setState(QStringLiteral("ready"));
            Q_EMIT nothingHeard();
        } else if (kind == QLatin1String("ERROR")) {
            m_wantListen = false;
            setState(QStringLiteral("error"), parts.value(1));
        } else if (!line.isEmpty()) {
            qInfo().noquote() << "Voice:" << line.left(200);
        }
    }
}

void Listener::simulate(const QString &text)
{
    qInfo().noquote() << "Voice: simulated" << text;
    Q_EMIT heard(text);
}
