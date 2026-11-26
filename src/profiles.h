// Profielen (bv. Arthur, Petra, Kinderen): eigen tegels, rijen, radiozenders en Vaak gebruikt.
// Een kinderprofiel gebruikt altijd het kinderslot. Wisselen herstart Kijkluister in dat profiel.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "paths.h"

#include <QObject>
#include <QSettings>
#include <QVariantList>

class Profiles : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList list READ list NOTIFY changed)          // [{id, name, kids, current}]
    Q_PROPERTY(QString current READ current CONSTANT)
    Q_PROPERTY(QString currentName READ currentName NOTIFY changed)
    Q_PROPERTY(bool kids READ kids CONSTANT)
    Q_PROPERTY(int count READ count NOTIFY changed)

public:
    explicit Profiles(QObject *parent = nullptr);

    QVariantList list() const;
    QString current() const { return m_current; }
    QString currentName() const { return nameOf(m_current); }
    bool kids() const { return isKids(m_current); }
    int count() const { return int(ids().size()); }

    Q_INVOKABLE QString add(const QString &name, bool kids);
    Q_INVOKABLE void remove(const QString &id);
    Q_INVOKABLE void rename(const QString &id, const QString &name);
    Q_INVOKABLE bool isKids(const QString &id) const;
    Q_INVOKABLE QString nameOf(const QString &id) const;
    // Opslaan en Kijkluister in dat profiel herstarten
    Q_INVOKABLE void switchTo(const QString &id);

Q_SIGNALS:
    void changed();

private:
    QStringList ids() const;
    QSettings m_settings{appSettingsFile(), QSettings::IniFormat};
    QString m_current;
};
