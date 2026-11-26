// Map met instellingen, log, bibliotheek en cache: %LOCALAPPDATA%\\Kijkluister
// (tot en met 3.8.1 heette die map BigscreenHost; hij wordt bij de eerste start hernoemd).
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QString>

QString appDataDir();

#include <QSettings>
// Alle instellingen in één bestand: %LOCALAPPDATA%\Kijkluister\settings.ini
inline QString appSettingsFile() { return appDataDir() + QStringLiteral("/settings.ini"); }

// Profielen (4.3): instellingen per profiel onder "profiles/<id>/". Het hoofdprofiel (id leeg)
// gebruikt de oude sleutels, zodat bestaande instellingen gewoon blijven werken.
inline QString currentProfile()
{
    return QSettings(appSettingsFile(), QSettings::IniFormat).value(QStringLiteral("profile/current")).toString();
}
inline QString profileKey(const QString &key)
{
    const QString id = currentProfile();
    return id.isEmpty() ? key : QStringLiteral("profiles/%1/%2").arg(id, key);
}
