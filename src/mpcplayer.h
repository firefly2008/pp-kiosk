// Films en series afspelen in MPC-HC (clsid2/mpc-hc), meegeleverd met Kijkluister.
// - draagbare installatie in %LOCALAPPDATA%\Kijkluister\mpc-hc (eigen, schrijfbare instellingen:
//   Nederlands, schermvullend, positie onthouden)
// - series: de gekozen aflevering en de afleveringen daarna als afspeellijst
// - gamepad bedient MPC-HC zolang die speelt; daarna terug naar Kijkluister
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>

class MpcPlayer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(bool bundled READ bundled CONSTANT)

public:
    explicit MpcPlayer(const QString &dataDir, QObject *parent = nullptr);
    ~MpcPlayer() override;

    static MpcPlayer *instance();

    bool running() const;
    bool bundled() const;
    QString executable();      // door Kijkluister opgehaalde MPC-HC, anders een geïnstalleerde
    // Speel één bestand, of een reeks (eerste = nu, rest = daarna). closeAtEnd: MPC-HC sluit na afloop.
    bool play(const QStringList &files, bool closeAtEnd);
    // Slaaptimer: MPC-HC netjes sluiten (positie wordt onthouden)
    Q_INVOKABLE void stop();

    // Gamepad naar MPC-HC (alleen zolang die speelt en voorop staat)
    bool handleButton(int sdlButton, bool pressed);

Q_SIGNALS:
    void runningChanged();
    void finished();

private:
    static void configure(const QString &dir);
    void sendVirtualKey(unsigned short vk, bool pressed, bool alt = false);

    QString m_dataDir;
    QString m_portableDir;
    QProcess *m_process = nullptr;
};
