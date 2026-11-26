// Spraakbediening: tik op de microfoon en zeg een opdracht ("zet de radio aan", "wat doen we nu?").
// Gebruikt de Nederlandse spraakherkenning van Windows via PowerShell (listen.ps1), offline en met
// een vaste lijst zinnen. Werkt alleen als Nederlandse spraak in Windows staat en er een microfoon is.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QStringList>

class QProcess;

class Listener : public QObject
{
    Q_OBJECT
    // "off" (nog niet gestart), "starting", "ready", "listening" of "error"
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString message READ message NOTIFY stateChanged)

public:
    explicit Listener(QObject *parent = nullptr);
    ~Listener() override;

    QString state() const { return m_state; }
    QString message() const { return m_message; }
    static QStringList phrases();

    // Herkenning alvast opstarten (scheelt wachten bij de eerste keer)
    Q_INVOKABLE void prepare();
    // Eén opdracht beluisteren
    Q_INVOKABLE void listen();
    // Rooktest: doen alsof er iets gezegd is
    Q_INVOKABLE void simulate(const QString &text);

Q_SIGNALS:
    void stateChanged();
    void heard(const QString &text);
    void nothingHeard();

private:
    void setState(const QString &state, const QString &message = {});
    void readOutput();

    QProcess *m_proc = nullptr;
    QString m_state = QStringLiteral("off");
    QString m_message;
    bool m_wantListen = false;
};
