// Back-up en herstel: agenda, instellingen, snelkoppelingen, eigen picto's/foto's, presentaties en
// gebruiksstatistiek in één bestand (.ppkiosk). Logboeken, caches en de beheercode gaan niet mee.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QByteArray>
#include <QString>

namespace Backup {
// Maakt het back-upbestand in het geheugen; files = aantal bestanden erin
QByteArray create(const QString &dataDir, int *files = nullptr);
// Zet een back-up terug. Bestaande bestanden met dezelfde naam worden overschreven.
bool restore(const QString &dataDir, const QString &archive, QString *error, int *files = nullptr);
}
