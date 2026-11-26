// SPDX-License-Identifier: GPL-2.0-or-later
#include "securestore.h"

#include <QDataStream>
#include <QDebug>
#include <QIODevice>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <wincrypt.h>
#include <dpapi.h>
#endif

namespace {
const QString suffix = QStringLiteral("_dpapi");

QByteArray protect(const QByteArray &plain)
{
#ifdef Q_OS_WIN
    DATA_BLOB in{DWORD(plain.size()), reinterpret_cast<BYTE *>(const_cast<char *>(plain.constData()))};
    DATA_BLOB out{};
    if (!CryptProtectData(&in, L"PP Kiosk", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        return {};
    }
    QByteArray result(reinterpret_cast<const char *>(out.pbData), int(out.cbData));
    LocalFree(out.pbData);
    return result;
#else
    return plain;
#endif
}

QByteArray unprotect(const QByteArray &blob)
{
#ifdef Q_OS_WIN
    DATA_BLOB in{DWORD(blob.size()), reinterpret_cast<BYTE *>(const_cast<char *>(blob.constData()))};
    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        return {};
    }
    QByteArray result(reinterpret_cast<const char *>(out.pbData), int(out.cbData));
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return result;
#else
    return blob;
#endif
}

QByteArray serialize(const QVariant &value)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_5);
    stream << value;
    return data;
}

QVariant deserialize(const QByteArray &data)
{
    QDataStream stream(data);
    stream.setVersion(QDataStream::Qt_6_5);
    QVariant value;
    stream >> value;
    return value;
}
} // namespace

QVariant SecureStore::value(QSettings &settings, const QString &key, const QVariant &fallback)
{
    const QString blob = settings.value(key + suffix).toString();
    if (!blob.isEmpty()) {
        const QByteArray plain = unprotect(QByteArray::fromBase64(blob.toLatin1()));
        if (plain.isEmpty()) {
            // Andere gebruiker of andere pc (bv. een gekopieerd settings.ini): niet te lezen
            qWarning() << "SecureStore: cannot decrypt" << key;
            return fallback;
        }
        return deserialize(plain);
    }
    if (settings.contains(key)) {
        // Oude, onversleutelde waarde: omzetten
        const QVariant old = settings.value(key);
        setValue(settings, key, old);
        qInfo() << "SecureStore: protected" << key;
        return old;
    }
    return fallback;
}

void SecureStore::setValue(QSettings &settings, const QString &key, const QVariant &value)
{
    const QByteArray blob = protect(serialize(value));
    if (blob.isEmpty()) {
        // Versleutelen lukt niet: liever onversleuteld bewaren dan gegevens kwijtraken
        qWarning() << "SecureStore: protect failed, storing plain" << key;
        settings.setValue(key, value);
        return;
    }
    settings.setValue(key + suffix, QString::fromLatin1(blob.toBase64()));
    settings.remove(key);
}

void SecureStore::remove(QSettings &settings, const QString &key)
{
    settings.remove(key + suffix);
    settings.remove(key);
}

int SecureStore::unprotectFile(const QString &iniFile)
{
    QSettings file(iniFile, QSettings::IniFormat);
    int n = 0;
    const QStringList keys = file.allKeys();
    for (const QString &k : keys) {
        if (!k.endsWith(suffix)) {
            continue;
        }
        const QByteArray plain = unprotect(QByteArray::fromBase64(file.value(k).toString().toLatin1()));
        if (plain.isEmpty()) {
            continue;
        }
        file.setValue(k.left(k.size() - suffix.size()), deserialize(plain));
        file.remove(k);
        ++n;
    }
    file.sync();
    return n;
}
