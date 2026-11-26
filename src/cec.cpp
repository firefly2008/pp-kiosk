// SPDX-License-Identifier: GPL-2.0-or-later
#include "cec.h"

#include <QCryptographicHash>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QStandardPaths>

#include <SDL3/SDL_gamepad.h>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <shellapi.h>
#endif

Cec::Cec(QObject *parent)
    : QObject(parent)
{
    m_retry.setSingleShot(true);
    m_retry.setInterval(60 * 1000);
    connect(&m_retry, &QTimer::timeout, this, &Cec::start);
    m_status = installed() ? tr("Nog niet gestart") : tr("libCEC is niet geïnstalleerd");
}

Cec::~Cec()
{
    if (m_process) {
        m_process->disconnect(this);
        m_process->write("q\n");
        if (!m_process->waitForFinished(1500)) {
            m_process->kill();
        }
    }
}

QString Cec::clientPath() const
{
    const QString fromPath = QStandardPaths::findExecutable(QStringLiteral("cec-client"));
    if (!fromPath.isEmpty()) {
        return fromPath;
    }
    const QStringList roots{qEnvironmentVariable("ProgramFiles"), qEnvironmentVariable("ProgramFiles(x86)")};
    for (const QString &root : roots) {
        if (root.isEmpty()) {
            continue;
        }
        const QString base = QDir(root).filePath(QStringLiteral("Pulse-Eight"));
        for (const QString &sub : {QStringLiteral("USB-CEC Adapter/x64"), QStringLiteral("USB-CEC Adapter")}) {
            const QString exe = QDir(base).filePath(sub + QStringLiteral("/cec-client.exe"));
            if (QFile::exists(exe)) {
                return exe;
            }
        }
        QDirIterator it(base, {QStringLiteral("cec-client.exe")}, QDir::Files, QDirIterator::Subdirectories);
        if (it.hasNext()) {
            return it.next();
        }
    }
    return {};
}

void Cec::setStatus(const QString &status, bool busy)
{
    m_status = status;
    m_busy = busy;
    Q_EMIT changed();
}

void Cec::start()
{
    if (m_process || !installed()) {
        return;
    }
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::readyRead, this, [this] {
        m_buffer += m_process->readAll();
        int nl;
        while ((nl = m_buffer.indexOf('\n')) >= 0) {
            parseLine(QString::fromLocal8Bit(m_buffer.left(nl)).trimmed());
            m_buffer.remove(0, nl + 1);
        }
    });
    connect(m_process, &QProcess::finished, this, [this] {
        qInfo() << "CEC: cec-client stopped";
        m_process->deleteLater();
        m_process = nullptr;
        const bool was = m_connected;
        m_connected = false;
        if (was || m_status.isEmpty()) {
            setStatus(tr("Verbinding met de tv verbroken"));
        }
        Q_EMIT changed();
        m_retry.start();   // adapter later ingeprikt of tv aangezet: opnieuw proberen
    });
    // -t p: afspeelapparaat, -o: naam op de tv, -d 8: knoppen staan in de debug-uitvoer
    setStatus(tr("Zoeken naar de CEC-adapter…"), true);
    qInfo() << "CEC: starting" << clientPath();
    m_process->start(clientPath(), {QStringLiteral("-t"), QStringLiteral("p"), QStringLiteral("-o"), QStringLiteral("Kijkluister"),
                                    QStringLiteral("-d"), QStringLiteral("8")});
}

bool Cec::parseLine(const QString &line)
{
    if (line.isEmpty()) {
        return false;
    }
    if (line.contains(QLatin1String("autodetect FAILED")) || line.contains(QLatin1String("could not open a connection"))) {
        setStatus(tr("Geen CEC-adapter gevonden. Prik de USB-CEC-adapter in en zet de tv aan."));
        return false;
    }
    if (!m_connected && (line.contains(QLatin1String("connection opened")) || line.contains(QLatin1String("waiting for input")))) {
        m_connected = true;
        qInfo() << "CEC: connected";
        setStatus(tr("Verbonden met de tv"));
        return false;
    }
    static const QRegularExpression keyRe(QStringLiteral("key (pressed|released): [^(]*\\(([0-9a-fA-F]+)\\)"));
    const QRegularExpressionMatch m = keyRe.match(line);
    if (!m.hasMatch()) {
        return false;
    }
    const bool pressed = m.captured(1) == QLatin1String("pressed");
    const int code = m.captured(2).toInt(nullptr, 16);
    qInfo() << "CEC key" << (pressed ? "pressed" : "released") << Qt::hex << code;
    if (code == 0x09) {   // Home / hoofdmenu op de afstandsbediening
        if (pressed) {
            Q_EMIT homeRequested();
        }
        return true;
    }
    const int sdl = sdlButtonFor(code);
    if (sdl < 0) {
        return false;
    }
    if (pressed) {
        if (m_down == sdl) {
            return true;   // herhaling: cec-client meldt een ingedrukte knop meerdere keren
        }
        if (m_down >= 0) {
            Q_EMIT button(m_down, false);
        }
        m_down = sdl;
        Q_EMIT button(sdl, true);
    } else {
        Q_EMIT button(sdl, false);
        m_down = -1;
    }
    return true;
}

int Cec::sdlButtonFor(int code)
{
    switch (code) {
    case 0x00: // select / OK
    case 0x44: // play
    case 0x46: // pause
    case 0x61: // play/pause
        return SDL_GAMEPAD_BUTTON_SOUTH;
    case 0x01: return SDL_GAMEPAD_BUTTON_DPAD_UP;
    case 0x02: return SDL_GAMEPAD_BUTTON_DPAD_DOWN;
    case 0x03: return SDL_GAMEPAD_BUTTON_DPAD_LEFT;
    case 0x04: return SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
    case 0x0D: // exit / terug
    case 0x91: // return (sommige merken)
    case 0x45: // stop
        return SDL_GAMEPAD_BUTTON_EAST;
    case 0x0A: // setup-menu
    case 0x0B: // contents-menu
    case 0x0C: // favoritesmenu
    case 0x71: // blauw
        return SDL_GAMEPAD_BUTTON_WEST;
    case 0x53: // tv-gids (EPG)
    case 0x35: // informatie
    case 0x74: // geel
        return SDL_GAMEPAD_BUTTON_NORTH;
    case 0x47: // opnemen
    case 0x72: // rood
        return SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
    }
    return -1;
}

void Cec::install()
{
    if (m_busy) {
        return;
    }
    setStatus(tr("libCEC ophalen…"), true);
    QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/Pulse-Eight/libcec/releases/latest")));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Kijkluister"));
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        QUrl asset;
        QString name;
        QString digest;
        for (const QJsonValue &v : QJsonDocument::fromJson(reply->readAll()).object().value(QStringLiteral("assets")).toArray()) {
            const QString n = v.toObject().value(QStringLiteral("name")).toString();
            if (n.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive)) {
                asset = QUrl(v.toObject().value(QStringLiteral("browser_download_url")).toString());
                name = n;
                digest = v.toObject().value(QStringLiteral("digest")).toString();
            }
        }
        if (!asset.isValid()) {
            setStatus(tr("libCEC niet gevonden op GitHub. Installeer het via pulse-eight.com."));
            return;
        }
        const QString file = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath(name);
        QNetworkRequest dl(asset);
        dl.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply *download = m_network.get(dl);
        connect(download, &QNetworkReply::finished, this, [this, download, file, digest] {
            download->deleteLater();
            QFile out(file);
            if (download->error() != QNetworkReply::NoError || !out.open(QIODevice::WriteOnly)) {
                setStatus(tr("Ophalen van libCEC lukte niet."));
                return;
            }
            const QByteArray data = download->readAll();
            // Controlegetal van GitHub: een vervalste installer niet met beheerdersrechten starten
            if (digest.startsWith(QLatin1String("sha256:"))
                && QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()) != digest.mid(7).toLower()) {
                qWarning() << "CEC: installer checksum mismatch";
                setStatus(tr("Ophalen van libCEC lukte niet."));
                return;
            }
            out.write(data);
            out.close();
            qInfo() << "CEC: running installer" << file;
#ifdef Q_OS_WIN
            // Installer vraagt beheerdersrechten (Windows toont een melding)
            const std::wstring path = QDir::toNativeSeparators(file).toStdWString();
            SHELLEXECUTEINFOW info{};
            info.cbSize = sizeof(info);
            info.fMask = SEE_MASK_NOCLOSEPROCESS;
            info.lpVerb = L"runas";
            info.lpFile = path.c_str();
            info.nShow = SW_SHOWNORMAL;
            if (!ShellExecuteExW(&info)) {
                setStatus(tr("Installeren van libCEC is geannuleerd."));
                return;
            }
            setStatus(tr("Volg de stappen van de installer. Daarna verbindt Kijkluister vanzelf."), false);
            // Na afloop van de installer opnieuw proberen
            auto *poll = new QTimer(this);
            poll->setInterval(3000);
            HANDLE process = info.hProcess;
            connect(poll, &QTimer::timeout, this, [this, poll, process] {
                if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0) {
                    CloseHandle(process);
                    poll->deleteLater();
                    Q_EMIT changed();
                    if (installed()) {
                        start();
                    } else {
                        setStatus(tr("libCEC is niet geïnstalleerd"));
                    }
                }
            });
            poll->start();
#else
            setStatus(tr("Alleen op Windows."));
#endif
        });
    });
}
