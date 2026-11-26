// SPDX-License-Identifier: GPL-2.0-or-later
#include "profiles.h"

#include <QCoreApplication>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>

Profiles::Profiles(QObject *parent)
    : QObject(parent)
    , m_current(currentProfile())
{
    if (!m_current.isEmpty() && !ids().contains(m_current)) {
        m_current.clear();   // verwijderd profiel: terug naar het hoofdprofiel
        m_settings.remove(QStringLiteral("profile/current"));
    }
    qInfo() << "Profile:" << (m_current.isEmpty() ? QStringLiteral("(hoofdprofiel)") : m_current);
}

QStringList Profiles::ids() const
{
    QStringList list{QString()};   // hoofdprofiel
    list << m_settings.value(QStringLiteral("profile/list")).toStringList();
    return list;
}

QVariantList Profiles::list() const
{
    QVariantList out;
    for (const QString &id : ids()) {
        out.append(QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("name"), nameOf(id)},
                               {QStringLiteral("kids"), isKids(id)}, {QStringLiteral("current"), id == m_current}});
    }
    return out;
}

QString Profiles::nameOf(const QString &id) const
{
    if (id.isEmpty()) {
        return m_settings.value(QStringLiteral("profile/mainName"), tr("Hoofdprofiel")).toString();
    }
    return m_settings.value(QStringLiteral("profile/%1/name").arg(id), id).toString();
}

bool Profiles::isKids(const QString &id) const
{
    return !id.isEmpty() && m_settings.value(QStringLiteral("profile/%1/kids").arg(id)).toBool();
}

QString Profiles::add(const QString &name, bool kids)
{
    QString id = name.trimmed().toLower();
    id.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    if (id.isEmpty()) {
        id = QStringLiteral("profiel");
    }
    const QString base = id;
    for (int n = 2; ids().contains(id); ++n) {
        id = base + QString::number(n);
    }
    QStringList list = m_settings.value(QStringLiteral("profile/list")).toStringList();
    list << id;
    m_settings.setValue(QStringLiteral("profile/list"), list);
    m_settings.setValue(QStringLiteral("profile/%1/name").arg(id), name.trimmed());
    m_settings.setValue(QStringLiteral("profile/%1/kids").arg(id), kids);
    qInfo() << "Profile added" << id << kids;
    Q_EMIT changed();
    return id;
}

void Profiles::remove(const QString &id)
{
    if (id.isEmpty() || id == m_current) {
        return;
    }
    QStringList list = m_settings.value(QStringLiteral("profile/list")).toStringList();
    list.removeAll(id);
    m_settings.setValue(QStringLiteral("profile/list"), list);
    m_settings.remove(QStringLiteral("profile/") + id);
    m_settings.remove(QStringLiteral("profiles/") + id);
    Q_EMIT changed();
}

void Profiles::rename(const QString &id, const QString &name)
{
    if (name.trimmed().isEmpty()) {
        return;
    }
    m_settings.setValue(id.isEmpty() ? QStringLiteral("profile/mainName") : QStringLiteral("profile/%1/name").arg(id), name.trimmed());
    Q_EMIT changed();
}

void Profiles::switchTo(const QString &id)
{
    if (id == m_current || !ids().contains(id)) {
        return;
    }
    m_settings.setValue(QStringLiteral("profile/current"), id);
    m_settings.sync();
    qInfo() << "Profile switch to" << id;
    // Nieuw proces in het andere profiel; dit proces sluit
    // "--profile-chosen": niet opnieuw vragen wie er kijkt
    QStringList args = QCoreApplication::arguments().mid(1);
    args.removeAll(QStringLiteral("--autostart"));
    if (!args.contains(QStringLiteral("--profile-chosen"))) {
        args << QStringLiteral("--profile-chosen");
    }
    QProcess::startDetached(QCoreApplication::applicationFilePath(), args);
    QTimer::singleShot(200, qApp, &QCoreApplication::quit);
}
