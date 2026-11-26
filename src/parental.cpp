// SPDX-License-Identifier: GPL-2.0-or-later
#include "parental.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QRandomGenerator>
#include <QRegularExpression>

Parental::Parental(QObject *parent)
    : QObject(parent)
{
}

QString Parental::hash(const QString &pin) const
{
    QString salt = sval(QStringLiteral("parental/salt")).toString();
    return QString::fromLatin1(QCryptographicHash::hash((salt + QLatin1Char(':') + pin).toUtf8(), QCryptographicHash::Sha256).toHex());
}

bool Parental::unlocked() const
{
    return QDateTime::currentSecsSinceEpoch() < m_unlockedUntil;
}

bool Parental::setPin(const QString &pin)
{
    static const QRegularExpression digits(QStringLiteral("^\\d{4}$"));
    if (!digits.match(pin).hasMatch()) {
        return false;
    }
    const bool first = !enabled();
    sset(QStringLiteral("parental/salt"), QString::number(QRandomGenerator::global()->generate64(), 36));
    sset(QStringLiteral("parental/pin"), hash(pin));
    if (first && !m_settings.contains(QStringLiteral("parental/locked_dpapi")) && !m_settings.contains(QStringLiteral("parental/locked"))) {
        sset(QStringLiteral("parental/locked"), QStringList{QStringLiteral("bigscreen:iptv"), QStringLiteral("bigscreen:movies"), QStringLiteral("bigscreen:shows")});
    }
    m_unlockedUntil = 0;
    qInfo() << "Parental: pin set";
    Q_EMIT changed();
    return true;
}

int Parental::waitSeconds() const
{
    return int(qMax<qint64>(0, sval(QStringLiteral("parental/lockedUntil")).toLongLong() - QDateTime::currentSecsSinceEpoch()));
}

bool Parental::check(const QString &pin)
{
    if (!enabled()) {
        return true;
    }
    // Raden tegengaan: na 5 foute pogingen eerst 1 minuut wachten, daarna steeds langer (max. 15 min);
    // ook na herstarten van Kijkluister
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const qint64 lockedUntil = sval(QStringLiteral("parental/lockedUntil")).toLongLong();
    if (lockedUntil > now) {
        qInfo() << "Parental: locked for" << (lockedUntil - now) << "s";
        return false;
    }
    if (hash(pin) != sval(QStringLiteral("parental/pin")).toString()) {
        const int fails = sval(QStringLiteral("parental/fails")).toInt() + 1;
        sset(QStringLiteral("parental/fails"), fails);
        if (fails >= 5) {
            const qint64 wait = qMin<qint64>(900, 60LL << qMin(fails - 5, 4));
            sset(QStringLiteral("parental/lockedUntil"), qlonglong(now + wait));
            qInfo() << "Parental: too many wrong pins, waiting" << wait << "s";
        }
        qInfo() << "Parental: wrong pin";
        return false;
    }
    SecureStore::remove(m_settings, QStringLiteral("parental/fails"));
    SecureStore::remove(m_settings, QStringLiteral("parental/lockedUntil"));
    m_unlockedUntil = QDateTime::currentSecsSinceEpoch() + 3600;
    qInfo() << "Parental: unlocked";
    Q_EMIT changed();
    return true;
}

bool Parental::disable(const QString &pin)
{
    if (!check(pin)) {
        return false;
    }
    SecureStore::remove(m_settings, QStringLiteral("parental/pin"));
    SecureStore::remove(m_settings, QStringLiteral("parental/salt"));
    qInfo() << "Parental: disabled";
    Q_EMIT changed();
    return true;
}

void Parental::lockNow()
{
    m_unlockedUntil = 0;
    Q_EMIT changed();
}

bool Parental::isLocked(const QString &path) const
{
    return !path.isEmpty() && locked().contains(path);
}

void Parental::setLocked(const QString &path, bool on)
{
    QStringList list = locked();
    list.removeAll(path);
    if (on) {
        list.append(path);
    }
    sset(QStringLiteral("parental/locked"), list);
    Q_EMIT changed();
}

bool Parental::isAdultChannel(const QString &group, const QString &name) const
{
    static const QRegularExpression adult(QStringLiteral("(?<!\\w)(xxx|adult|adults|18\\+|porn|erotic|erotiek|playboy|hustler|brazzers|private tv)(?!\\w)"),
                                          QRegularExpression::CaseInsensitiveOption);
    return adult.match(group).hasMatch() || adult.match(name).hasMatch();
}
