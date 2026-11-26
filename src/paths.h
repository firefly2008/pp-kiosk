// Map met instellingen, log en cache: %LOCALAPPDATA%\\PPKiosk
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QString>

QString appDataDir();

#include <QSettings>
// Alle instellingen in één bestand: %LOCALAPPDATA%\PPKiosk\settings.ini
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
