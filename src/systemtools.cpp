// SPDX-License-Identifier: GPL-2.0-or-later
#include "systemtools.h"
#include "paths.h"
#include "securestore.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QRegularExpression>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <mmdeviceapi.h>
#include <propidl.h>
#endif

namespace {
const QString runKey = QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
const QString runName = QStringLiteral("PP Kiosk");

QString startupShortcut()
{
    // Snelkoppeling die de installer bij "automatisch starten" in de map Opstarten zet
    return QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) + QStringLiteral("/Startup/PP Kiosk.lnk");
}

QString desktopDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
}

#ifdef Q_OS_WIN
// Niet-openbare, maar al sinds Windows 7 gebruikte interface om de standaard-geluidsuitgang te wisselen
// (zoals SoundSwitch en EarTrumpet doen)
struct __declspec(uuid("f8679f50-850a-41cf-9c72-430f290290c8")) IPolicyConfig : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(PCWSTR, void **) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(PCWSTR, INT, void **) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(PCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(PCWSTR, void *, void *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(PCWSTR, INT, PINT64, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(PCWSTR, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(PCWSTR, void *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(PCWSTR, void *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PCWSTR, const PROPERTYKEY &, PROPVARIANT *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(PCWSTR, const PROPERTYKEY &, PROPVARIANT *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR, ERole) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(PCWSTR, INT) = 0;
};
const CLSID clsidPolicyConfig = {0x870af99c, 0x171d, 0x4f9e, {0xaf, 0x0d, 0xe6, 0x3d, 0xf4, 0x0c, 0x2b, 0xc9}};
// PKEY_Device_FriendlyName
const PROPERTYKEY friendlyNameKey = {{0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14};

template<typename T>
void release(T *&p)
{
    if (p) {
        p->Release();
        p = nullptr;
    }
}

struct Output {
    QString id;
    QString name;
    bool isDefault = false;
};

QList<Output> listOutputs()
{
    QList<Output> result;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IMMDeviceEnumerator *enumerator = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                reinterpret_cast<void **>(&enumerator)))) {
        return result;
    }
    QString defaultId;
    IMMDevice *def = nullptr;
    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &def))) {
        LPWSTR id = nullptr;
        if (SUCCEEDED(def->GetId(&id))) {
            defaultId = QString::fromWCharArray(id);
            CoTaskMemFree(id);
        }
        release(def);
    }
    IMMDeviceCollection *devices = nullptr;
    if (SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices))) {
        UINT count = 0;
        devices->GetCount(&count);
        for (UINT i = 0; i < count; ++i) {
            IMMDevice *device = nullptr;
            if (FAILED(devices->Item(i, &device))) {
                continue;
            }
            Output out;
            LPWSTR id = nullptr;
            if (SUCCEEDED(device->GetId(&id))) {
                out.id = QString::fromWCharArray(id);
                CoTaskMemFree(id);
            }
            IPropertyStore *store = nullptr;
            if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &store))) {
                PROPVARIANT value;
                PropVariantInit(&value);
                if (SUCCEEDED(store->GetValue(friendlyNameKey, &value)) && value.vt == VT_LPWSTR) {
                    out.name = QString::fromWCharArray(value.pwszVal);
                }
                PropVariantClear(&value);
                release(store);
            }
            out.isDefault = out.id == defaultId;
            if (!out.id.isEmpty()) {
                result.append(out);
            }
            release(device);
        }
        release(devices);
    }
    release(enumerator);
    return result;
}
#endif
} // namespace

SystemTools::SystemTools(QObject *parent)
    : QObject(parent)
    , m_lastInput(QDateTime::currentMSecsSinceEpoch())
{
    qApp->installEventFilter(this);
}

bool SystemTools::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:
    case QEvent::Wheel:
    case QEvent::TouchBegin:
        m_lastInput = QDateTime::currentMSecsSinceEpoch();
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

QVariant SystemTools::globalSetting(const QString &name, const QVariant &fallback) const
{
    return QSettings(appSettingsFile(), QSettings::IniFormat).value(QStringLiteral("global/") + name, fallback);
}

void SystemTools::setGlobalSetting(const QString &name, const QVariant &value)
{
    QSettings(appSettingsFile(), QSettings::IniFormat).setValue(QStringLiteral("global/") + name, value);
}

int SystemTools::idleSeconds() const
{
    return int((QDateTime::currentMSecsSinceEpoch() - m_lastInput) / 1000);
}

bool SystemTools::hasArgument(const QString &arg) const
{
    return QCoreApplication::arguments().contains(arg);
}

bool SystemTools::autostart() const
{
#ifdef Q_OS_WIN
    QSettings run(runKey, QSettings::NativeFormat);
    return run.contains(runName) || QFile::exists(startupShortcut());
#else
    return false;
#endif
}

void SystemTools::setAutostart(bool on)
{
#ifdef Q_OS_WIN
    QSettings run(runKey, QSettings::NativeFormat);
    // Eén manier tegelijk: de snelkoppeling van de installer gaat weg, de registerwaarde blijft over
    QFile::remove(startupShortcut());
    if (on) {
        run.setValue(runName, QStringLiteral("\"%1\" --autostart")
                                  .arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath())));
    } else {
        run.remove(runName);
    }
    run.sync();
    qInfo() << "Autostart" << on << "now" << autostart();
#else
    Q_UNUSED(on)
#endif
    Q_EMIT autostartChanged();
}

QVariantList SystemTools::audioOutputs() const
{
    QVariantList list;
#ifdef Q_OS_WIN
    for (const Output &o : listOutputs()) {
        list.append(QVariantMap{{QStringLiteral("id"), o.id}, {QStringLiteral("name"), o.name}, {QStringLiteral("isDefault"), o.isDefault}});
    }
#endif
    return list;
}

QString SystemTools::audioOutputName() const
{
#ifdef Q_OS_WIN
    for (const Output &o : listOutputs()) {
        if (o.isDefault) {
            return o.name;
        }
    }
#endif
    return {};
}

bool SystemTools::setAudioOutput(const QString &id)
{
#ifdef Q_OS_WIN
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IPolicyConfig *policy = nullptr;
    if (FAILED(CoCreateInstance(clsidPolicyConfig, nullptr, CLSCTX_ALL, __uuidof(IPolicyConfig), reinterpret_cast<void **>(&policy)))) {
        qWarning() << "Audio: policy config not available";
        return false;
    }
    const std::wstring wid = id.toStdWString();
    bool ok = true;
    for (ERole role : {eConsole, eMultimedia, eCommunications}) {
        ok = SUCCEEDED(policy->SetDefaultEndpoint(wid.c_str(), role)) && ok;
    }
    release(policy);
    qInfo() << "Audio output set" << ok << audioOutputName();
    return ok;
#else
    Q_UNUSED(id)
    return false;
#endif
}

QString SystemTools::shareLog() const
{
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH.mm"));
    const QString target = desktopDir() + QStringLiteral("/PP Kiosk-log ") + stamp + QStringLiteral(".txt");
    QFile out(target);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {};
    }
    QString info;
    info += QStringLiteral("PP Kiosk %1\n").arg(QStringLiteral(PPKIOSK_VERSION));
    info += QStringLiteral("Windows: %1 (%2)\n").arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture());
    info += QStringLiteral("Qt: %1\n").arg(QString::fromLatin1(qVersion()));
    for (QScreen *s : QGuiApplication::screens()) {
        info += QStringLiteral("Scherm: %1 %2x%3 @%4\n").arg(s->name()).arg(s->size().width()).arg(s->size().height()).arg(s->devicePixelRatio());
    }
#ifdef Q_OS_WIN
    info += QStringLiteral("Geluid: %1\n").arg(audioOutputName());
#endif
    info += QStringLiteral("Tijd: %1\n\n").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    out.write(info.toUtf8());
    // Wachtwoorden en sleutels uit links halen (lijsten van aanbieders bevatten ze in de url)
    static const QRegularExpression secrets(QStringLiteral(R"((password|passwd|pwd|username|user|token|api_key|apikey|key|access_token|auth)=[^&\s"']+)"),
                                            QRegularExpression::CaseInsensitiveOption);
    for (const QString &name : {QStringLiteral("ppkiosk-vorige.log"), QStringLiteral("ppkiosk.log")}) {
        QFile log(appDataDir() + QLatin1Char('/') + name);
        if (log.open(QIODevice::ReadOnly)) {
            out.write(QStringLiteral("===== %1 =====\n").arg(name).toUtf8());
            QString text = QString::fromUtf8(log.readAll());
            text.replace(secrets, QStringLiteral("\\1=***"));
            // Xtream-adressen: gebruiker en wachtwoord staan in het pad (ook in logs van oudere versies)
            static const QRegularExpression xtream(QStringLiteral(R"((https?://[^/\s"']+)/((?:live|movie|series|timeshift)/)?[^/\s"']+/[^/\s"']+/(\d+(?:\.\w+)?))"),
                                                   QRegularExpression::CaseInsensitiveOption);
            text.replace(xtream, QStringLiteral("\\1/\\2***/***/\\3"));
            out.write(text.toUtf8());
            out.write("\n");
        }
    }
    out.close();
    qInfo() << "Log shared to" << target;
    return QFileInfo(target).fileName();
}

QString SystemTools::makeBackup() const
{
    QSettings(appSettingsFile(), QSettings::IniFormat).sync();
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH.mm"));
    const QString target = desktopDir() + QStringLiteral("/PP Kiosk-backup ") + stamp + QStringLiteral(".ini");
    QFile::remove(target);
    if (!QFile::copy(appSettingsFile(), target)) {
        qWarning() << "Backup failed" << target;
        return {};
    }
    // Inloggegevens staan versleuteld voor deze Windows-gebruiker; in de back-up weer leesbaar,
    // zodat hij ook op een andere pc werkt (de back-up zelf dus goed bewaren)
    const int opened = SecureStore::unprotectFile(target);
    qInfo() << "Backup made" << target << "protected values exported:" << opened;
    return QFileInfo(target).fileName();
}

QStringList SystemTools::backups() const
{
    QDir dir(desktopDir());
    return dir.entryList({QStringLiteral("PP Kiosk-backup*.ini")}, QDir::Files, QDir::Time);
}

bool SystemTools::restoreBackup(const QString &fileName)
{
    const QString source = desktopDir() + QLatin1Char('/') + QFileInfo(fileName).fileName();
    QSettings check(source, QSettings::IniFormat);
    if (!QFile::exists(source) || check.status() != QSettings::NoError || check.allKeys().isEmpty()) {
        qWarning() << "Restore: not a valid backup" << source;
        return false;
    }
    const QString current = appSettingsFile();
    QFile::remove(current + QStringLiteral(".voor-terugzetten"));
    QFile::copy(current, current + QStringLiteral(".voor-terugzetten"));
    QFile::remove(current);
    if (!QFile::copy(source, current)) {
        QFile::copy(current + QStringLiteral(".voor-terugzetten"), current);
        return false;
    }
    qInfo() << "Restored backup" << source << "; restarting";
    QProcess::startDetached(QCoreApplication::applicationFilePath(), QCoreApplication::arguments().mid(1));
    QTimer::singleShot(300, qApp, &QCoreApplication::quit);
    return true;
}

void SystemTools::restart()
{
    QStringList args = QCoreApplication::arguments().mid(1);
    args.removeAll(QStringLiteral("--autostart"));
    if (!args.contains(QStringLiteral("--profile-chosen"))) {
        args << QStringLiteral("--profile-chosen");
    }
    qInfo() << "Restarting PP Kiosk";
    QProcess::startDetached(QCoreApplication::applicationFilePath(), args);
    QTimer::singleShot(300, qApp, &QCoreApplication::quit);
}
