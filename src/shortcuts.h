// Snelkoppelingen: Nederlandse mediaproviders en eigen websites, als grote tegels.
// Opgeslagen in %LOCALAPPDATA%\PPKiosk\snelkoppelingen.json; aan te passen via de beheerpagina.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QJsonArray>
#include <QObject>
#include <QVariantList>

class Shortcuts : public QObject
{
    Q_OBJECT
    // Elk item: name, url, icon (emoji), color, group
    Q_PROPERTY(QVariantList list READ list NOTIFY changed)

public:
    explicit Shortcuts(const QString &dataDir, QObject *parent = nullptr);

    QVariantList list() const;
    QByteArray toJson() const;
    bool setJson(const QByteArray &json, QString *error);

Q_SIGNALS:
    void changed();

private:
    static QJsonArray defaults();
    void save() const;

    QString m_file;
    QJsonArray m_items;
};
