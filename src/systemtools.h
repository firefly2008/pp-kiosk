// Systeemzaken voor Instellingen (4.4):
// - starten met Windows (HKCU\...\Run, en de snelkoppeling die de installer kan maken)
// - geluid kiezen: standaard-geluidsuitgang van Windows wisselen (tv, soundbar, koptelefoon)
// - logbestand delen (op het bureaublad) en back-up maken/terugzetten
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>

class SystemTools : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool autostart READ autostart WRITE setAutostart NOTIFY autostartChanged)

public:
    explicit SystemTools(QObject *parent = nullptr);

    bool autostart() const;
    void setAutostart(bool on);

    // Geluidsuitgangen: [{ id, name, isDefault }]
    Q_INVOKABLE QVariantList audioOutputs() const;
    Q_INVOKABLE bool setAudioOutput(const QString &id);
    Q_INVOKABLE QString audioOutputName() const;

    // Instellingen voor alle profielen samen (bv. "Wie kijkt er?")
    Q_INVOKABLE QVariant globalSetting(const QString &name, const QVariant &fallback) const;
    Q_INVOKABLE void setGlobalSetting(const QString &name, const QVariant &value);

    // Kijkluister opnieuw starten (na het wisselen van taal)
    Q_INVOKABLE void restart();

    // Seconden sinds de laatste toets, muisbeweging of controllerknop (voor de sfeerstand)
    Q_INVOKABLE int idleSeconds() const;
    // Kijkluister is gestart door Windows (starten met Windows) of na het wisselen van profiel
    Q_INVOKABLE bool hasArgument(const QString &arg) const;

    // Logbestand (+ vorig log en systeeminfo) als tekstbestand op het bureaublad; geeft de bestandsnaam
    Q_INVOKABLE QString shareLog() const;

    // Back-up van alle instellingen (zenders, profielen, tegels…) op het bureaublad; geeft de bestandsnaam
    Q_INVOKABLE QString makeBackup() const;
    // Back-ups op het bureaublad, nieuwste eerst (alleen bestandsnamen)
    Q_INVOKABLE QStringList backups() const;
    // Terugzetten en Kijkluister opnieuw starten
    Q_INVOKABLE bool restoreBackup(const QString &fileName);

Q_SIGNALS:
    void autostartChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    qint64 m_lastInput = 0;
};
