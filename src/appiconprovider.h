// Levert het Windows-pictogram van een snelkoppeling/programma aan QML
// (image://appicon/<pad>). Vervangt KIconTheme/freedesktop-icoonthema's.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QQuickImageProvider>

class AppIconProvider : public QQuickImageProvider
{
public:
    AppIconProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
