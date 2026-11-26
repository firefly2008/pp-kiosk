// Gevoelige instellingen (inloggegevens van de tv-aanbieder, eigen TMDB-sleutel) versleuteld bewaren
// met Windows DPAPI: alleen deze Windows-gebruiker op deze pc kan ze lezen. In settings.ini staan ze
// dan als "<sleutel>_dpapi". Oude, onversleutelde waarden worden bij het lezen vanzelf omgezet.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QSettings>
#include <QVariant>

namespace SecureStore {
QVariant value(QSettings &settings, const QString &key, const QVariant &fallback = {});
void setValue(QSettings &settings, const QString &key, const QVariant &value);
void remove(QSettings &settings, const QString &key);
// Voor de back-up (naar een andere pc): alle "_dpapi"-waarden in dit bestand weer leesbaar maken
int unprotectFile(const QString &iniFile);
}
