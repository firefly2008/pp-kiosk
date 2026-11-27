// SPDX-License-Identifier: GPL-2.0-or-later
#include "adminserver.h"

#include <QBuffer>
#include <QImage>
#include <QQuickWindow>
#include "agenda.h"
#include "appmodel.h"
#include "radiomodel.h"
#include "presentations.h"
#include "shortcuts.h"
#include "stats.h"
#include "backup.h"
#include "menuboard.h"
#include <QCoreApplication>
#include <QProcess>
#include "paths.h"
#ifndef PPKIOSK_VERSION
#define PPKIOSK_VERSION "0.0.0"
#endif
#include "thirdparty/qrcodegen.hpp"

#include <QBuffer>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QHostInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QImage>
#include <QRegularExpression>
#include <QSaveFile>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPasswordDigestor>
#include <QCryptographicHash>
#include <QPdfWriter>
#include <QPointer>
#include <QRandomGenerator>
#include <QSettings>
#include <QTcpSocket>
#include <QTimer>
#include <QUrlQuery>

namespace {
constexpr quint16 firstPort = 8642;
constexpr qint64 maxBody = 1024 * 1024;
constexpr qint64 maxUpload = 400LL * 1024 * 1024;   // grote PowerPoint met video's
constexpr qint64 maxPicto = 8LL * 1024 * 1024;
constexpr qint64 maxRestore = 2048LL * 1024 * 1024;   // back-up met presentaties

QString cleanPictoName(const QString &name)
{
    QString n = QFileInfo(name).fileName();
    n.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9 ._-]")), QStringLiteral("_"));
    while (n.startsWith(QLatin1Char('.')) || n.startsWith(QLatin1Char(' ')) || n.startsWith(QLatin1Char('_'))) {
        n.remove(0, 1);
    }
    static const QRegularExpression ok(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9 ._-]{0,80}\\.(png|jpg|jpeg|webp|svg)$"), QRegularExpression::CaseInsensitiveOption);
    return ok.match(n).hasMatch() ? n : QString();
}

bool sameCode(const QString &a, const QString &b)
{
    // Vergelijken zonder vroeg te stoppen (geen tijdverschil per goed cijfer)
    if (a.size() != b.size() || b.isEmpty()) {
        return false;
    }
    int diff = 0;
    for (int i = 0; i < a.size(); ++i) {
        diff |= a.at(i).unicode() ^ b.at(i).unicode();
    }
    return diff == 0;
}
} // namespace

AdminServer::AdminServer(const QString &dataDir, Agenda *agenda, Presentations *presentations, Shortcuts *shortcuts, QObject *parent)
    : QObject(parent)
    , m_dataDir(dataDir)
    , m_agenda(agenda)
    , m_presentations(presentations)
    , m_shortcuts(shortcuts)
{
    m_code = QSettings(appSettingsFile(), QSettings::IniFormat).value(QStringLiteral("admin/code")).toString();
    if (m_code.size() != 6) {
        m_code = QString::number(QRandomGenerator::system()->bounded(100000, 1000000));
        QSettings(appSettingsFile(), QSettings::IniFormat).setValue(QStringLiteral("admin/code"), m_code);
    }
    connect(&m_server, &QTcpServer::newConnection, this, [this] {
        while (QTcpSocket *socket = m_server.nextPendingConnection()) {
            if (m_open >= 32) {
                socket->abort();
                socket->deleteLater();
                continue;
            }
            ++m_open;
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] { handle(socket); });
            connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
                --m_open;
                m_pending.remove(socket);
                socket->deleteLater();
            });
            // Gewone aanvragen: 30 seconden. Een upload die loopt krijgt 20 minuten.
            QTimer::singleShot(30000, socket, [this, socket] {
                if (m_pending.value(socket).file) {
                    QTimer::singleShot(20 * 60 * 1000, socket, [socket] { socket->abort(); });
                    return;
                }
                socket->abort();
            });
        }
    });
    start();
}

void AdminServer::start()
{
    for (quint16 port = firstPort; port < firstPort + 10; ++port) {
        if (m_server.listen(QHostAddress::AnyIPv4, port)) {
            break;
        }
    }
    qInfo() << "Admin: listening" << m_server.isListening() << address();
    makeQr();
    Q_EMIT changed();
}

QString AdminServer::address() const
{
    if (!m_server.isListening()) {
        return {};
    }
    // Adres op het netwerk (192.168.x.x heeft voorrang, daarna 10.x en de rest)
    QString best;
    int bestScore = -1;
    for (const QNetworkInterface &nif : QNetworkInterface::allInterfaces()) {
        const auto flags = nif.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning) || (flags & QNetworkInterface::IsLoopBack)
            || nif.type() == QNetworkInterface::Virtual) {
            continue;
        }
        const QString name = nif.humanReadableName().toLower();
        const bool virtualAdapter = name.contains(QLatin1String("vethernet")) || name.contains(QLatin1String("virtualbox"))
                                    || name.contains(QLatin1String("vmware")) || name.contains(QLatin1String("wsl"));
        for (const QNetworkAddressEntry &e : nif.addressEntries()) {
            const QHostAddress a = e.ip();
            if (a.protocol() != QAbstractSocket::IPv4Protocol || a.isLoopback()) {
                continue;
            }
            const QString ip = a.toString();
            int score = ip.startsWith(QLatin1String("192.168.")) ? 3 : ip.startsWith(QLatin1String("10.")) ? 2 : 1;
            if (virtualAdapter) {
                score -= 2;
            }
            if (score > bestScore) {
                bestScore = score;
                best = ip;
            }
        }
    }
    if (best.isEmpty()) {
        best = QStringLiteral("127.0.0.1");
    }
    return QStringLiteral("http://%1:%2").arg(best).arg(m_server.serverPort());
}

void AdminServer::newCode()
{
    m_code = QString::number(QRandomGenerator::system()->bounded(100000, 1000000));
    QSettings(appSettingsFile(), QSettings::IniFormat).setValue(QStringLiteral("admin/code"), m_code);
    makeQr();
    Q_EMIT changed();
}

void AdminServer::makeQr()
{
    if (!m_server.isListening()) {
        return;
    }
    const QString url = address() + QStringLiteral("/#c=") + m_code;
    const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(url.toUtf8().constData(), qrcodegen::QrCode::Ecc::MEDIUM);
    const int border = 2;
    const int scale = 10;
    const int size = (qr.getSize() + border * 2) * scale;
    QImage image(size, size, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter p(&image);
    for (int y = 0; y < qr.getSize(); ++y) {
        for (int x = 0; x < qr.getSize(); ++x) {
            if (qr.getModule(x, y)) {
                p.fillRect((x + border) * scale, (y + border) * scale, scale, scale, Qt::black);
            }
        }
    }
    p.end();
    const QString file = m_dataDir + QStringLiteral("/admin-qr-%1.png").arg(m_code);
    image.save(file);
    m_qr = QUrl::fromLocalFile(file);
}

void AdminServer::respond(QTcpSocket *socket, int status, const QByteArray &type, const QByteArray &body, const QByteArray &extraHeaders)
{
    Pending &p = m_pending[socket];
    p.done = true;
    // Beheer van meerdere kiosken: de beheerpagina van een andere kiosk mag deze kiosk aanspreken (met code)
    QByteArray cors;
    if (!p.origin.isEmpty()) {
        cors = "Access-Control-Allow-Origin: " + p.origin + "\r\nVary: Origin\r\n";
    }
    const QByteArray reason = status == 200 ? "OK" : status == 204 ? "No Content" : status == 403 ? "Forbidden" : status == 404 ? "Not Found"
                              : status == 413 ? "Payload Too Large" : status == 415 ? "Unsupported Media Type" : status == 429 ? "Too Many Requests" : "Bad Request";
    QByteArray head = "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason + "\r\n";
    head += "Content-Type: " + type + "\r\nContent-Length: " + QByteArray::number(body.size())
            + "\r\nCache-Control: no-store\r\nConnection: close\r\n" + cors + extraHeaders
            + "X-Content-Type-Options: nosniff\r\nX-Frame-Options: DENY\r\nReferrer-Policy: no-referrer"
              "\r\nContent-Security-Policy: default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob: http: https://images.unsplash.com; connect-src 'self' http:; frame-ancestors 'none'"
              "\r\n\r\n";
    socket->write(head + body);
    socket->disconnectFromHost();
}

void AdminServer::handle(QTcpSocket *socket)
{
    Pending &p = m_pending[socket];
    if (p.done) {
        socket->readAll();
        return;
    }
    QByteArray data = socket->readAll();
    if (!p.headersDone) {
        p.buffer += data;
        const int headerEnd = p.buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0) {
            if (p.buffer.size() > 64 * 1024) {
                socket->abort();
            }
            return;
        }
        const QList<QByteArray> lines = p.buffer.left(headerEnd).split('\n');
        const QList<QByteArray> request = lines.value(0).trimmed().split(' ');
        QByteArray host;
        QString code;
        p.length = 0;
        for (const QByteArray &raw : lines) {
            const QByteArray line = raw.trimmed();
            const QByteArray lower = line.toLower();
            if (lower.startsWith("content-length:")) {
                p.length = line.mid(15).trimmed().toLongLong();
            } else if (lower.startsWith("host:")) {
                host = line.mid(5).trimmed().toLower();
            } else if (lower.startsWith("x-code:")) {
                code = QString::fromLatin1(line.mid(7).trimmed());
            } else if (lower.startsWith("origin:")) {
                p.origin = line.mid(7).trimmed();
                if (!QRegularExpression(QStringLiteral("^https?://[A-Za-z0-9.:\\[\\]-]{1,100}$")).match(QString::fromLatin1(p.origin)).hasMatch()) {
                    p.origin.clear();
                    code.clear();
                    host = "?";   // vreemde Origin: weigeren
                }
            }
        }
        p.method = request.value(0);
        p.url = QUrl(QString::fromUtf8(request.value(1)));
        data = p.buffer.mid(headerEnd + 4);
        p.buffer.clear();
        p.headersDone = true;

        // Voorcontrole van de browser (beheer vanaf een andere kiosk): toestaan, de echte aanvraag vraagt de code
        if (p.method == "OPTIONS") {
            respond(socket, 204, "text/plain", {},
                    "Access-Control-Allow-Methods: GET, POST, PUT, DELETE\r\nAccess-Control-Allow-Headers: X-Code, Content-Type\r\n"
                    "Access-Control-Allow-Private-Network: true\r\nAccess-Control-Max-Age: 600\r\n");
            return;
        }
        // Plaatjes op de beheerpagina (<img>) sturen de code mee als ?c=
        if (code.isEmpty() && p.url.path().startsWith(QLatin1String("/pictos/"))) {
            code = QUrlQuery(p.url).queryItemValue(QStringLiteral("c"));
        }
        // Eerst controleren (host, code), pas daarna de inhoud aannemen
        if (!checkAccess(socket, host, code, p.url.path(), p.origin)) {
            return;
        }
        const bool picto = p.method == "POST" && p.url.path() == QLatin1String("/api/picto");
        const bool restore = p.method == "POST" && p.url.path() == QLatin1String("/api/restore");
        const bool upload = (p.method == "POST" && p.url.path() == QLatin1String("/api/upload")) || picto || restore;
        const qint64 limit = picto ? maxPicto : restore ? maxRestore : upload ? maxUpload : maxBody;
        if (p.length < 0 || p.length > limit) {
            respond(socket, 413, "text/plain; charset=utf-8", picto ? "plaatje is te groot (max. 8 MB)" : upload ? "bestand is te groot (max. 400 MB)" : "te groot");
            return;
        }
        if (upload) {
            p.file = new QTemporaryFile(m_dataDir + QStringLiteral("/upload-XXXXXX.tmp"), socket);
            if (!p.file->open()) {
                respond(socket, 400, "application/json", R"({"error":"Tijdelijk bestand maken lukt niet."})");
                return;
            }
            qInfo() << "Admin: upload started" << p.length << "bytes";
        }
    }

    if (p.file) {
        const qint64 take = qMin<qint64>(data.size(), p.length - p.received);
        if (take > 0 && p.file->write(data.constData(), take) != take) {
            respond(socket, 400, "application/json", R"({"error":"Schijf vol of niet schrijfbaar."})");
            return;
        }
        p.received += qMax<qint64>(take, 0);
    } else {
        p.buffer += data;
        p.received = p.buffer.size();
    }
    if (p.received < p.length) {
        return;
    }
    dispatch(socket, p);
}

bool AdminServer::checkAccess(QTcpSocket *socket, const QByteArray &host, const QString &code, const QString &path, const QByteArray &origin)
{
    const QHostAddress peer = socket->peerAddress();
    const QString ip = peer.toString();
    // Zonder code alleen vanaf deze pc zelf, en alleen vanaf de eigen beheerpagina:
    // een aanvraag van een andere website (Origin hoort niet bij deze server) moet de code hebben
    const bool sameOrigin = origin.isEmpty() || origin == "http://" + host;
    const bool local = sameOrigin && (peer.isLoopback() || peer == QHostAddress(QStringLiteral("::ffff:127.0.0.1")));
    const qint64 now = QDateTime::currentSecsSinceEpoch();

    // Alleen aanvragen voor een IP-adres, localhost of de naam van deze pc (tegen "DNS rebinding")
    {
        QString h = QString::fromLatin1(host);
        if (h.startsWith(QLatin1Char('['))) {
            h = h.mid(1, h.indexOf(QLatin1Char(']')) - 1);
        } else if (h.count(QLatin1Char(':')) == 1) {
            h = h.section(QLatin1Char(':'), 0, 0);
        }
        const QString machine = QHostInfo::localHostName().toLower();
        const bool okHost = !QHostAddress(h).isNull() || h == QLatin1String("localhost") || h == machine
                            || h == machine + QLatin1String(".local") || h == machine + QLatin1String(".home") || h == machine + QLatin1String(".lan");
        if (!okHost) {
            qInfo() << "Admin: refused host" << h << "from" << ip;
            respond(socket, 403, "text/plain; charset=utf-8", "verboden");
            return false;
        }
    }
    // De pagina zelf mag iedereen ophalen; gegevens alleen met de code (of op deze pc zelf)
    if (!path.startsWith(QLatin1String("/api/")) && !path.startsWith(QLatin1String("/pictos/"))) {
        return true;
    }
    if (local) {
        return true;
    }
    // Inloggen met naam + wachtwoord: de controle (en het tellen van foute pogingen) zit in dispatch
    if (path == QLatin1String("/api/login") && m_blockedUntil.value(ip) <= now) {
        return true;
    }
    // Na 5 foute codes binnen 10 minuten: dit apparaat 10 minuten geblokkeerd
    if (m_blockedUntil.value(ip) > now) {
        respond(socket, 429, "text/plain; charset=utf-8", "te veel pogingen, probeer het over 10 minuten");
        return false;
    }
    if (!sameCode(code, m_code) && !validToken(code)) {
        countFailure(ip);
        respond(socket, 403, "text/plain; charset=utf-8", "code klopt niet");
        return false;
    }
    m_failures.remove(ip);
    return true;
}

QString AdminServer::unsplashKey() const
{
    return QSettings(appSettingsFile(), QSettings::IniFormat).value(QStringLiteral("unsplash/key")).toString();
}

namespace {
QNetworkRequest unsplashRequest(const QUrl &url, const QString &key)
{
    QNetworkRequest req(url);
    req.setRawHeader("Accept-Version", "v1");
    if (!key.isEmpty()) {
        req.setRawHeader("Authorization", "Client-ID " + key.toLatin1());
    }
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("PPKiosk"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(20000);
    return req;
}
const QString unsplashRef = QStringLiteral("utm_source=pp_kiosk&utm_medium=referral");
}

void AdminServer::unsplashSearch(QTcpSocket *socket, const QString &q, int page)
{
    const QString key = unsplashKey();
    if (key.isEmpty()) {
        respond(socket, 400, "application/json", R"({"error":"Nog geen Unsplash-sleutel ingesteld.","nokey":true})");
        return;
    }
    if (q.trimmed().isEmpty()) {
        respond(socket, 400, "application/json", R"({"error":"Typ een zoekwoord."})");
        return;
    }
    QUrl url(QStringLiteral("https://api.unsplash.com/search/photos"));
    QUrlQuery uq;
    uq.addQueryItem(QStringLiteral("query"), q);
    uq.addQueryItem(QStringLiteral("page"), QString::number(page));
    uq.addQueryItem(QStringLiteral("per_page"), QStringLiteral("24"));
    uq.addQueryItem(QStringLiteral("orientation"), QStringLiteral("landscape"));
    url.setQuery(uq);
    QPointer<QTcpSocket> sock(socket);
    QNetworkReply *reply = m_network.get(unsplashRequest(url, key));
    connect(reply, &QNetworkReply::finished, this, [this, reply, sock] {
        reply->deleteLater();
        if (!sock) {
            return;
        }
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status != 200) {
            qInfo() << "Unsplash: search failed" << status << reply->errorString();
            respond(sock, 502, "application/json", QJsonDocument(QJsonObject{{QStringLiteral("error"),
                status == 401 ? QStringLiteral("De Unsplash-sleutel klopt niet.") : status == 403 ? QStringLiteral("Unsplash: te veel zoekopdrachten dit uur. Probeer het later.")
                              : QStringLiteral("Unsplash is niet bereikbaar. Is er internet?")}}).toJson(QJsonDocument::Compact));
            return;
        }
        QJsonArray out;
        for (const QJsonValue &v : QJsonDocument::fromJson(reply->readAll()).object().value(QStringLiteral("results")).toArray()) {
            const QJsonObject o = v.toObject();
            const QJsonObject user = o.value(QStringLiteral("user")).toObject();
            out.append(QJsonObject{
                {QStringLiteral("id"), o.value(QStringLiteral("id")).toString()},
                {QStringLiteral("thumb"), o.value(QStringLiteral("urls")).toObject().value(QStringLiteral("small")).toString()},
                {QStringLiteral("alt"), o.value(QStringLiteral("alt_description")).toString()},
                {QStringLiteral("author"), user.value(QStringLiteral("name")).toString()},
                {QStringLiteral("authorUrl"), user.value(QStringLiteral("links")).toObject().value(QStringLiteral("html")).toString() + QLatin1Char('?') + unsplashRef},
            });
        }
        respond(sock, 200, "application/json", QJsonDocument(QJsonObject{{QStringLiteral("results"), out}}).toJson(QJsonDocument::Compact));
    });
}

void AdminServer::unsplashUse(QTcpSocket *socket, const QString &idOrLink)
{
    // Een id, of een link zoals https://unsplash.com/photos/zonsondergang-aan-zee-AbC123xyz
    static const QRegularExpression idRe(QStringLiteral("([A-Za-z0-9_-]{11})(?:[/?#].*)?$"));
    const QRegularExpressionMatch m = idRe.match(idOrLink);
    if (!m.hasMatch()) {
        respond(socket, 400, "application/json", R"({"error":"Dit is geen Unsplash-foto. Plak de link van de foto, bijvoorbeeld https://unsplash.com/photos/..."})");
        return;
    }
    const QString id = m.captured(1);
    const QString key = unsplashKey();
    QPointer<QTcpSocket> sock(socket);
    const QString pictoDir = m_agenda->pictoDir();
    auto download = [this, sock, id, pictoDir](const QUrl &imageUrl, const QString &credit, const QString &creditUrl) {
        QNetworkReply *img = m_network.get(unsplashRequest(imageUrl, QString()));
        connect(img, &QNetworkReply::finished, this, [this, img, sock, id, pictoDir, credit, creditUrl] {
            img->deleteLater();
            const QByteArray data = img->readAll();
            QImage image;
            if (img->error() != QNetworkReply::NoError || !image.loadFromData(data)) {
                qInfo() << "Unsplash: download failed" << img->errorString();
                if (sock) {
                    respond(sock, 502, "application/json", R"({"error":"De foto ophalen lukt niet. Is er internet?"})");
                }
                return;
            }
            if (image.width() > 2560) {
                image = image.scaledToWidth(2560, Qt::SmoothTransformation);
            }
            QDir().mkpath(pictoDir);
            const QString name = QStringLiteral("unsplash-%1.jpg").arg(id);
            image.save(pictoDir + QLatin1Char('/') + name, "JPG", 88);
            qInfo() << "Unsplash: background saved" << name << image.size();
            if (sock) {
                respond(sock, 200, "application/json", QJsonDocument(QJsonObject{{QStringLiteral("name"), name}, {QStringLiteral("credit"), credit},
                                                                               {QStringLiteral("creditUrl"), creditUrl}}).toJson(QJsonDocument::Compact));
            }
        });
    };
    if (key.isEmpty()) {
        // Zonder sleutel: de gewone downloadlink van Unsplash (volgt een doorverwijzing naar de foto)
        download(QUrl(QStringLiteral("https://unsplash.com/photos/%1/download?w=1920&").arg(id) + unsplashRef), QStringLiteral("Unsplash"),
                 QStringLiteral("https://unsplash.com/photos/%1?").arg(id) + unsplashRef);
        return;
    }
    QNetworkReply *reply = m_network.get(unsplashRequest(QUrl(QStringLiteral("https://api.unsplash.com/photos/") + id), key));
    connect(reply, &QNetworkReply::finished, this, [this, reply, sock, key, download] {
        reply->deleteLater();
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        const QString raw = o.value(QStringLiteral("urls")).toObject().value(QStringLiteral("raw")).toString();
        if (reply->error() != QNetworkReply::NoError || raw.isEmpty()) {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            qInfo() << "Unsplash: photo failed" << status << reply->errorString();
            if (sock) {
                respond(sock, 502, "application/json", status == 404 ? QByteArray(R"({"error":"Deze foto is niet gevonden op Unsplash."})")
                                                       : status == 401 ? QByteArray(R"({"error":"De Unsplash-sleutel klopt niet."})")
                                                       : QByteArray(R"({"error":"Unsplash is niet bereikbaar. Is er internet?"})"));
            }
            return;
        }
        // Volgens de regels van Unsplash: melden dat de foto gebruikt wordt
        const QString ping = o.value(QStringLiteral("links")).toObject().value(QStringLiteral("download_location")).toString();
        if (!ping.isEmpty()) {
            QNetworkReply *p = m_network.get(unsplashRequest(QUrl(ping), key));
            connect(p, &QNetworkReply::finished, p, &QObject::deleteLater);
        }
        const QJsonObject user = o.value(QStringLiteral("user")).toObject();
        download(QUrl(raw + QStringLiteral("&w=1920&h=1080&fit=crop&q=85&fm=jpg")), user.value(QStringLiteral("name")).toString(),
                 user.value(QStringLiteral("links")).toObject().value(QStringLiteral("html")).toString() + QLatin1Char('?') + unsplashRef);
    });
}

void AdminServer::countFailure(const QString &ip)
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    auto &fails = m_failures[ip];
    fails.append(now);
    while (!fails.isEmpty() && now - fails.first() > 600) {
        fails.removeFirst();
    }
    if (fails.size() >= 5) {
        m_blockedUntil.insert(ip, now + 600);
        qInfo() << "Admin: blocked" << ip;
    }
}

namespace {
QByteArray loginHash(const QString &pass, const QByteArray &salt)
{
    return QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256, pass.toUtf8(), salt, 120000, 32).toHex();
}
}

bool AdminServer::loginSet() const
{
    const QSettings st(appSettingsFile(), QSettings::IniFormat);
    return !st.value(QStringLiteral("admin/user")).toString().isEmpty() && !st.value(QStringLiteral("admin/hash")).toString().isEmpty();
}

bool AdminServer::checkLogin(const QString &user, const QString &pass) const
{
    const QSettings st(appSettingsFile(), QSettings::IniFormat);
    const QString u = st.value(QStringLiteral("admin/user")).toString();
    const QString h = st.value(QStringLiteral("admin/hash")).toString();
    const QByteArray salt = QByteArray::fromHex(st.value(QStringLiteral("admin/salt")).toByteArray());
    if (u.isEmpty() || h.isEmpty() || pass.isEmpty()) {
        return false;
    }
    // Altijd de hash uitrekenen (ook bij een foute naam): geen tijdverschil
    const bool passOk = sameCode(QString::fromLatin1(loginHash(pass, salt)), h);
    return sameCode(user.trimmed().toLower(), u.toLower()) && passOk;
}

void AdminServer::setLogin(const QString &user, const QString &pass)
{
    QSettings st(appSettingsFile(), QSettings::IniFormat);
    if (user.trimmed().isEmpty()) {
        st.remove(QStringLiteral("admin/user"));
        st.remove(QStringLiteral("admin/hash"));
        st.remove(QStringLiteral("admin/salt"));
    } else {
        QByteArray salt(16, Qt::Uninitialized);
        QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(salt.data()), salt.size() / 4);
        st.setValue(QStringLiteral("admin/user"), user.trimmed());
        st.setValue(QStringLiteral("admin/salt"), salt.toHex());
        st.setValue(QStringLiteral("admin/hash"), loginHash(pass, salt));
    }
    m_tokens.clear();   // wie met het oude wachtwoord was ingelogd, moet opnieuw
    qInfo() << "Admin: login" << (user.trimmed().isEmpty() ? "removed" : "set");
}

bool AdminServer::validToken(const QString &token)
{
    if (token.size() != 32) {
        return false;
    }
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    for (auto it = m_tokens.begin(); it != m_tokens.end();) {
        it = it.value() < now ? m_tokens.erase(it) : std::next(it);
    }
    bool ok = false;
    for (auto it = m_tokens.cbegin(); it != m_tokens.cend(); ++it) {
        ok |= sameCode(token, it.key());
    }
    return ok;
}

void AdminServer::dispatch(QTcpSocket *socket, Pending &p)
{
    const QByteArray method = p.method;
    const QString path = p.url.path();
    const QUrlQuery query(p.url);
    const QString pres = query.queryItemValue(QStringLiteral("pres"), QUrl::FullyDecoded);
    const QByteArray body = p.buffer;
    auto error = [this, socket](const QString &message) {
        respond(socket, 400, "application/json", QJsonDocument(QJsonObject{{QStringLiteral("error"), message}}).toJson(QJsonDocument::Compact));
    };
    constexpr QByteArrayView okJson = R"({"ok":true})";

    if (method == "GET" && (path == QLatin1String("/") || path == QLatin1String("/index.html"))) {
        QFile f(QStringLiteral(":/admin/index.html"));
        f.open(QIODevice::ReadOnly);
        respond(socket, 200, "text/html; charset=utf-8", f.readAll());
        return;
    }
    if (method == "GET" && path == QLatin1String("/icon.png")) {
        QFile f(QStringLiteral(":/assets/web-192.png"));
        f.open(QIODevice::ReadOnly);
        respond(socket, 200, "image/png", f.readAll());
        return;
    }

    if (path == QLatin1String("/api/agenda")) {
        if (method == "GET") {
            respond(socket, 200, "application/json", m_agenda->toJson());
            return;
        }
        if (method == "PUT" || method == "POST") {
            QString message;
            if (m_agenda->setJson(body, &message)) {
                respond(socket, 200, "application/json", okJson.toByteArray());
            } else {
                error(message);
            }
            return;
        }
    }
    if (path == QLatin1String("/api/shortcuts") && m_shortcuts) {
        if (method == "GET") {
            respond(socket, 200, "application/json", m_shortcuts->toJson());
            return;
        }
        if (method == "PUT" || method == "POST") {
            QString message;
            if (m_shortcuts->setJson(body, &message)) {
                respond(socket, 200, "application/json", okJson.toByteArray());
            } else {
                error(message);
            }
            return;
        }
    }
    if (m_presentations) {
        if (path == QLatin1String("/api/presentations") && method == "GET") {
            respond(socket, 200, "application/json", m_presentations->toJson());
            return;
        }
        if (path == QLatin1String("/api/upload") && method == "POST" && p.file) {
            const QString fileName = query.queryItemValue(QStringLiteral("file"), QUrl::FullyDecoded);
            p.file->close();
            QString message;
            const bool ok = m_presentations->addUpload(pres, fileName, p.file->fileName(), &message);
            qInfo() << "Admin: upload" << pres << fileName << ok << message;
            if (ok) {
                respond(socket, 200, "application/json", okJson.toByteArray());
            } else {
                error(message);
            }
            return;
        }
        if (path == QLatin1String("/api/presentation") && method == "DELETE") {
            if (m_presentations->remove(pres)) {
                respond(socket, 200, "application/json", okJson.toByteArray());
            } else {
                error(QStringLiteral("Verwijderen lukt niet."));
            }
            return;
        }
        if (path == QLatin1String("/api/original") && method == "GET") {
            const QString file = m_presentations->originalFile(pres);
            QFile f(file);
            if (file.isEmpty() || !f.open(QIODevice::ReadOnly)) {
                respond(socket, 404, "text/plain; charset=utf-8", "geen origineel");
                return;
            }
            const QByteArray name = QUrl::toPercentEncoding(QFileInfo(file).fileName());
            respond(socket, 200, "application/octet-stream", f.readAll(),
                    "Content-Disposition: attachment; filename*=UTF-8''" + name + "\r\n");
            return;
        }
    }
    // ---------- Picto's ----------
    const QString pictoDir = m_agenda->pictoDir();
    if (path == QLatin1String("/api/pictos") && method == "GET") {
        QJsonArray names;
        for (const QString &f : QDir(pictoDir).entryList({QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"),
                                                          QStringLiteral("*.webp"), QStringLiteral("*.svg")}, QDir::Files, QDir::Name | QDir::IgnoreCase)) {
            names.append(f);
        }
        respond(socket, 200, "application/json", QJsonDocument(QJsonObject{{QStringLiteral("items"), names}}).toJson(QJsonDocument::Compact));
        return;
    }
    if (path == QLatin1String("/api/picto") && method == "POST" && p.file) {
        const QString name = cleanPictoName(query.queryItemValue(QStringLiteral("file"), QUrl::FullyDecoded));
        p.file->close();
        const bool svg = name.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive);
        if (name.isEmpty() || (!svg && QImage(p.file->fileName()).isNull())) {
            error(QStringLiteral("Dit plaatje kan niet: gebruik .png, .jpg, .webp of .svg."));
            return;
        }
        if (svg) {
            QFile f(p.file->fileName());
            f.open(QIODevice::ReadOnly);
            const QByteArray head = f.read(4096).toLower();
            if (!head.contains("<svg") || head.contains("<script")) {
                error(QStringLiteral("Dit SVG-bestand kan niet."));
                return;
            }
        }
        const QString target = pictoDir + QLatin1Char('/') + name;
        QFile::remove(target);
        if (!QFile::rename(p.file->fileName(), target) && !QFile::copy(p.file->fileName(), target)) {
            error(QStringLiteral("Opslaan mislukt."));
            return;
        }
        qInfo() << "Admin: picto saved" << name;
        respond(socket, 200, "application/json", QJsonDocument(QJsonObject{{QStringLiteral("ok"), true}, {QStringLiteral("name"), name}}).toJson(QJsonDocument::Compact));
        return;
    }
    if (path == QLatin1String("/api/picto") && method == "DELETE") {
        const QString name = cleanPictoName(query.queryItemValue(QStringLiteral("file"), QUrl::FullyDecoded));
        if (!name.isEmpty() && QFile::remove(pictoDir + QLatin1Char('/') + name)) {
            respond(socket, 200, "application/json", okJson.toByteArray());
        } else {
            error(QStringLiteral("Verwijderen lukt niet."));
        }
        return;
    }
    // Standaard etensfoto's (voor de keuze bij het weekmenu)
    if (path.startsWith(QLatin1String("/food/")) && method == "GET") {
        const QString name = path.mid(6).section(QLatin1Char('.'), 0, 0);
        QFile f(QStringLiteral(":/food/") + name + QStringLiteral(".jpg"));
        if (!MenuBoard::photoNames().contains(name) || !f.open(QIODevice::ReadOnly)) {
            respond(socket, 404, "text/plain; charset=utf-8", "niet gevonden");
            return;
        }
        respond(socket, 200, "image/jpeg", f.readAll());
        return;
    }
    if (path.startsWith(QLatin1String("/pictos/")) && method == "GET") {
        const QString name = cleanPictoName(path.mid(8));
        QFile f(pictoDir + QLatin1Char('/') + name);
        if (name.isEmpty() || !f.open(QIODevice::ReadOnly)) {
            respond(socket, 404, "text/plain; charset=utf-8", "niet gevonden");
            return;
        }
        const QString ext = QFileInfo(name).suffix().toLower();
        const QByteArray type = ext == QLatin1String("svg") ? "image/svg+xml" : ext == QLatin1String("png") ? "image/png"
                              : ext == QLatin1String("webp") ? "image/webp" : "image/jpeg";
        respond(socket, 200, type, f.readAll(), "Content-Security-Policy: default-src 'none'; style-src 'unsafe-inline'\r\n");
        return;
    }

    // ---------- Weekoverzicht ----------
    if (path == QLatin1String("/api/week") && method == "GET") {
        QDate start = QDate::fromString(query.queryItemValue(QStringLiteral("start")), Qt::ISODate);
        if (!start.isValid()) {
            start = QDate::currentDate().addDays(1 - QDate::currentDate().dayOfWeek());
        }
        QJsonArray days;
        for (int i = 0; i < 7; ++i) {
            const QDate d = start.addDays(i);
            QJsonArray items;
            for (const QVariant &v : m_agenda->itemsFor(d)) {
                const QVariantMap m = v.toMap();
                items.append(QJsonObject{{QStringLiteral("start"), m.value(QStringLiteral("start")).toString()},
                                         {QStringLiteral("title"), m.value(QStringLiteral("title")).toString()},
                                         {QStringLiteral("icon"), m.value(QStringLiteral("icon")).toString()},
                                         {QStringLiteral("image"), m.value(QStringLiteral("image")).toString()},
                                         {QStringLiteral("color"), m.value(QStringLiteral("color")).toString()},
                                         {QStringLiteral("once"), !m.value(QStringLiteral("date")).toString().isEmpty()}});
            }
            days.append(QJsonObject{{QStringLiteral("date"), d.toString(Qt::ISODate)}, {QStringLiteral("items"), items}});
        }
        respond(socket, 200, "application/json", QJsonDocument(QJsonObject{{QStringLiteral("days"), days}}).toJson(QJsonDocument::Compact));
        return;
    }

    // ---------- Diensten en radiozenders ----------
    if (path == QLatin1String("/api/media")) {
        if (method == "GET") {
            respond(socket, 200, "application/json", mediaJson());
            return;
        }
        if (method == "PUT" || method == "POST") {
            if (setMediaJson(body)) {
                respond(socket, 200, "application/json", okJson.toByteArray());
            } else {
                error(QStringLiteral("Geen geldige gegevens"));
            }
            return;
        }
    }

    // ---------- Andere kiosken (adres en code, voor beheer vanaf één pagina) ----------
    if (path == QLatin1String("/api/kiosks")) {
        const QString file = m_dataDir + QStringLiteral("/kiosken.json");
        if (method == "GET") {
            QFile f(file);
            QByteArray data = f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
            if (!QJsonDocument::fromJson(data).isObject()) {
                data = R"({"items":[]})";
            }
            respond(socket, 200, "application/json", data);
            return;
        }
        if (method == "PUT" || method == "POST") {
            QJsonArray out;
            static const QRegularExpression addr(QStringLiteral("^https?://[A-Za-z0-9.-]{1,100}(:\\d{1,5})?$"));
            for (const QJsonValue &v : QJsonDocument::fromJson(body).object().value(QStringLiteral("items")).toArray()) {
                const QJsonObject o = v.toObject();
                QString address = o.value(QStringLiteral("address")).toString().trimmed();
                if (!address.contains(QLatin1String("://"))) {
                    address.prepend(QStringLiteral("http://"));
                }
                while (address.endsWith(QLatin1Char('/'))) {
                    address.chop(1);
                }
                const QString name = o.value(QStringLiteral("name")).toString().trimmed().left(40);
                const QString c = o.value(QStringLiteral("code")).toString().trimmed().left(6);
                if (name.isEmpty() || !addr.match(address).hasMatch() || out.size() >= 30) {
                    error(QStringLiteral("Kiosk \"%1\": vul een naam en een adres als 192.168.1.20:8642 in.").arg(name));
                    return;
                }
                out.append(QJsonObject{{QStringLiteral("name"), name}, {QStringLiteral("address"), address}, {QStringLiteral("code"), c}});
            }
            QSaveFile f(file);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(QJsonDocument(QJsonObject{{QStringLiteral("items"), out}}).toJson());
                f.commit();
            }
            respond(socket, 200, "application/json", okJson.toByteArray());
            return;
        }
    }
    if (path == QLatin1String("/api/screen") && method == "GET") {
        if (!m_window) {
            respond(socket, 404, "text/plain; charset=utf-8", "geen scherm");
            return;
        }
        const QImage full = m_window->grabWindow();
        if (full.isNull()) {
            respond(socket, 503, "text/plain; charset=utf-8", "scherm nog niet klaar");
            return;
        }
        const QImage shot = full.scaledToWidth(960, Qt::SmoothTransformation);
        QByteArray jpg;
        QBuffer buffer(&jpg);
        buffer.open(QIODevice::WriteOnly);
        shot.save(&buffer, "JPG", 80);
        respond(socket, 200, "image/jpeg", jpg, "Cache-Control: no-store\r\n");
        return;
    }
    if (path == QLatin1String("/api/backup") && method == "GET") {
        if (m_stats) {
            m_stats->flush();
        }
        QSettings(appSettingsFile(), QSettings::IniFormat).sync();
        const QByteArray data = Backup::create(m_dataDir);
        const QString name = QStringLiteral("PP Kiosk-backup %1.ppkiosk").arg(QDate::currentDate().toString(Qt::ISODate));
        respond(socket, 200, "application/octet-stream", data, "Content-Disposition: attachment; filename=\"" + name.toUtf8() + "\"\r\n");
        return;
    }
    if (path == QLatin1String("/api/restore") && method == "POST" && p.file) {
        p.file->close();
        QString error;
        int files = 0;
        if (!Backup::restore(m_dataDir, p.file->fileName(), &error, &files)) {
            respond(socket, 400, "application/json", QJsonDocument(QJsonObject{{QStringLiteral("error"), error}}).toJson(QJsonDocument::Compact));
            return;
        }
        respond(socket, 200, "application/json", QJsonDocument(QJsonObject{{QStringLiteral("files"), files}}).toJson(QJsonDocument::Compact));
        // Opnieuw starten, zodat alles uit de back-up geladen wordt
        qInfo() << "Admin: backup restored; restarting";
        QTimer::singleShot(1500, qApp, [] {
            QStringList args = QCoreApplication::arguments().mid(1);
            args.removeAll(QStringLiteral("--autostart"));
            QProcess::startDetached(QCoreApplication::applicationFilePath(), args);
            QCoreApplication::quit();
        });
        return;
    }
    if (path == QLatin1String("/api/stats") && m_stats) {
        if (method == "GET") {
            respond(socket, 200, "application/json", m_stats->toJson());
            return;
        }
        if (method == "DELETE") {
            m_stats->clear();
            respond(socket, 200, "application/json", okJson.toByteArray());
            return;
        }
    }
    // ---------- Unsplash (achtergrondfoto) ----------
    if (path == QLatin1String("/api/unsplash/key")) {
        if (method == "GET") {
            respond(socket, 200, "application/json", QJsonDocument(QJsonObject{{QStringLiteral("set"), !unsplashKey().isEmpty()}}).toJson(QJsonDocument::Compact));
            return;
        }
        if (method == "PUT") {
            const QString key = QJsonDocument::fromJson(body).object().value(QStringLiteral("key")).toString().trimmed();
            static const QRegularExpression keyRe(QStringLiteral("^[A-Za-z0-9_-]{20,80}$"));
            if (!key.isEmpty() && !keyRe.match(key).hasMatch()) {
                error(QStringLiteral("Deze sleutel ziet er niet goed uit. Plak de \"Access Key\" van je Unsplash-app."));
                return;
            }
            QSettings(appSettingsFile(), QSettings::IniFormat).setValue(QStringLiteral("unsplash/key"), key);
            respond(socket, 200, "application/json", okJson.toByteArray());
            return;
        }
    }
    if (path == QLatin1String("/api/unsplash") && method == "GET") {
        unsplashSearch(socket, query.queryItemValue(QStringLiteral("q"), QUrl::FullyDecoded).left(80), qBound(1, query.queryItemValue(QStringLiteral("page")).toInt(), 20));
        return;
    }
    if (path == QLatin1String("/api/unsplash") && method == "POST") {
        const QJsonObject o = QJsonDocument::fromJson(body).object();
        unsplashUse(socket, o.value(QStringLiteral("id")).toString().trimmed().left(300));
        return;
    }
    // Inloggen met vaste naam + wachtwoord: geeft een sessiecode die 12 uur geldig is
    if (path == QLatin1String("/api/login") && method == "POST") {
        const QJsonObject o = QJsonDocument::fromJson(p.buffer).object();
        const QString ip = socket->peerAddress().toString();
        if (!checkLogin(o.value(QStringLiteral("user")).toString(), o.value(QStringLiteral("pass")).toString())) {
            countFailure(ip);
            qInfo() << "Admin: login failed from" << ip;
            respond(socket, 403, "text/plain; charset=utf-8", "inlognaam of wachtwoord klopt niet");
            return;
        }
        m_failures.remove(ip);
        QByteArray raw(16, Qt::Uninitialized);
        QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(raw.data()), raw.size() / 4);
        const QString token = QString::fromLatin1(raw.toHex());
        if (m_tokens.size() > 50) {
            m_tokens.clear();
        }
        m_tokens.insert(token, QDateTime::currentSecsSinceEpoch() + 12 * 3600);
        qInfo() << "Admin: login ok from" << ip;
        respond(socket, 200, "application/json", QJsonDocument(QJsonObject{{QStringLiteral("token"), token}}).toJson(QJsonDocument::Compact));
        return;
    }
    // Toegang: vaste inlog instellen, en de adressen van deze pc tonen
    if (path == QLatin1String("/api/access")) {
        if (method == "GET") {
            QJsonArray addresses;
            for (const QHostAddress &a : QNetworkInterface::allAddresses()) {
                if (a.protocol() == QAbstractSocket::IPv4Protocol && !a.isLoopback() && !a.isLinkLocal()) {
                    addresses.append(QStringLiteral("http://%1:%2").arg(a.toString()).arg(m_server.serverPort()));
                }
            }
            const QSettings st(appSettingsFile(), QSettings::IniFormat);
            respond(socket, 200, "application/json", QJsonDocument(QJsonObject{
                {QStringLiteral("user"), loginSet() ? st.value(QStringLiteral("admin/user")).toString() : QString()},
                {QStringLiteral("set"), loginSet()},
                {QStringLiteral("address"), address()},
                {QStringLiteral("addresses"), addresses},
                {QStringLiteral("port"), int(m_server.serverPort())},
            }).toJson(QJsonDocument::Compact));
            return;
        }
        if (method == "PUT") {
            const QJsonObject o = QJsonDocument::fromJson(p.buffer).object();
            const QString user = o.value(QStringLiteral("user")).toString().trimmed();
            const QString pass = o.value(QStringLiteral("pass")).toString();
            if (!user.isEmpty()) {
                if (user.size() < 3 || user.size() > 40 || user.contains(QLatin1Char(' '))) {
                    error(QStringLiteral("Inlognaam: 3 tot 40 tekens, zonder spaties."));
                    return;
                }
                if (pass.size() < 8 || pass.size() > 128) {
                    error(QStringLiteral("Wachtwoord: minstens 8 tekens."));
                    return;
                }
            }
            setLogin(user, pass);
            respond(socket, 200, "application/json", okJson.toByteArray());
            return;
        }
    }
    if (path == QLatin1String("/api/status") && method == "GET") {
        const QVariantMap cfg = m_agenda ? m_agenda->settings() : QVariantMap();
        const QJsonObject st{
            {QStringLiteral("version"), QStringLiteral(PPKIOSK_VERSION)},
            {QStringLiteral("name"), cfg.value(QStringLiteral("name")).toString()},
            {QStringLiteral("uptime"), QDateTime::currentSecsSinceEpoch() - m_startedAt},
            {QStringLiteral("offline"), cfg.value(QStringLiteral("offline")).toBool()},
            {QStringLiteral("host"), QHostInfo::localHostName()},
        };
        respond(socket, 200, "application/json", QJsonDocument(st).toJson(QJsonDocument::Compact));
        return;
    }
    if (path == QLatin1String("/api/check") && method == "GET") {
        respond(socket, 200, "application/json", okJson.toByteArray());
        return;
    }
    respond(socket, 404, "text/plain; charset=utf-8", "niet gevonden");
}

QByteArray AdminServer::mediaJson() const
{
    QJsonObject out;
    for (AppModel *m : m_web) {
        out.insert(m->category(), QJsonArray::fromVariantList(m->webCatalog()));
    }
    if (m_radio) {
        QJsonArray radio;
        for (const QVariant &v : m_radio->catalog()) {
            const QVariantMap m = v.toMap();
            radio.append(QJsonObject{{QStringLiteral("name"), m.value(QStringLiteral("name")).toString()},
                                     {QStringLiteral("on"), m.value(QStringLiteral("on")).toBool()}});
        }
        out.insert(QStringLiteral("radio"), radio);
    }
    return QJsonDocument(out).toJson(QJsonDocument::Compact);
}

bool AdminServer::setMediaJson(const QByteArray &json)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        return false;
    }
    const QJsonObject in = doc.object();
    auto chosen = [](const QJsonArray &list, const QString &field) {
        QStringList out;
        for (const QJsonValue &v : list) {
            if (v.toObject().value(QStringLiteral("on")).toBool()) {
                out << v.toObject().value(field).toString();
            }
        }
        return out;
    };
    for (AppModel *m : m_web) {
        if (in.contains(m->category())) {
            m->setWebShown(chosen(in.value(m->category()).toArray(), QStringLiteral("url")));
        }
    }
    if (m_radio && in.contains(QStringLiteral("radio"))) {
        m_radio->setChosen(chosen(in.value(QStringLiteral("radio")).toArray(), QStringLiteral("name")));
    }
    qInfo() << "Admin: media choices saved";
    return true;
}

void AdminServer::selfTest()
{
    // Zonder code van "buiten" (via het netwerkadres) moet geweigerd worden; lokaal moet de agenda komen
    auto ask = [this](const QString &base, const QByteArray &extra, const QString &label) {
        auto *s = new QTcpSocket(this);
        const QUrl u(base);
        connect(s, &QTcpSocket::connected, s, [s, u, extra] {
            s->write("GET /api/agenda HTTP/1.1\r\nHost: " + u.host().toLatin1() + "\r\n" + extra + "\r\n");
        });
        connect(s, &QTcpSocket::disconnected, s, [s, label] {
            const QByteArray reply = s->readAll();
            qInfo().noquote() << "Admin test" << label << QString::fromLatin1(reply.left(12).mid(9)) << (reply.contains("\"items\"") ? "items" : "");
            s->deleteLater();
        });
        s->connectToHost(u.host(), u.port());
    };
    const QString localBase = QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort());
    ask(localBase, {}, QStringLiteral("local"));
    if (!address().startsWith(QLatin1String("http://127."))) {
        ask(address(), "X-Code: 000000\r\n", QStringLiteral("lan-wrong"));
        ask(address(), "X-Code: " + m_code.toLatin1() + "\r\n", QStringLiteral("lan-right"));
    }
}

void AdminServer::loginTest()
{
    setLogin(QStringLiteral("Rooktest"), QStringLiteral("geheim-123"));
    const bool good = checkLogin(QStringLiteral("rooktest"), QStringLiteral("geheim-123"));
    const bool wrong = checkLogin(QStringLiteral("rooktest"), QStringLiteral("geheim-124")) || checkLogin(QStringLiteral("iemand"), QStringLiteral("geheim-123"));
    setLogin(QString(), QString());
    qInfo() << "Login test:" << (good && !wrong && !loginSet() ? "ok" : "fout") << good << wrong;
}

void AdminServer::backupTest()
{
    int made = 0;
    const QByteArray data = Backup::create(m_dataDir, &made);
    QTemporaryFile tmp;
    tmp.open();
    tmp.write(data);
    tmp.close();
    QString error;
    int restored = 0;
    const bool ok = Backup::restore(m_dataDir, tmp.fileName(), &error, &restored);
    // Kapot bestand moet geweigerd worden
    QTemporaryFile bad;
    bad.open();
    bad.write("geen back-up");
    bad.close();
    QString badError;
    const bool badOk = Backup::restore(m_dataDir, bad.fileName(), &badError);
    qInfo().noquote() << "Backup test:" << made << "files, restore" << (ok ? "ok" : error) << restored << "files, bad file" << (badOk ? "accepted" : "refused");
}

void AdminServer::uploadTest()
{
    QByteArray pdf;
    {
        QBuffer buffer(&pdf);
        buffer.open(QIODevice::WriteOnly);
        QPdfWriter writer(&buffer);
        writer.setPageSize(QPageSize(QPageSize::A4));
        writer.setPageOrientation(QPageLayout::Landscape);
        QPainter p(&writer);
        for (int page = 1; page <= 2; ++page) {
            if (page > 1) {
                writer.newPage();
            }
            QFont f = p.font();
            f.setPointSize(48);
            p.setFont(f);
            p.drawText(QRect(0, 0, writer.width(), writer.height()), Qt::AlignCenter, QStringLiteral("Dia %1").arg(page));
        }
    }
    if (m_presentations) {
        connect(m_presentations, &Presentations::conversionFinished, this, [this](const QString &name, bool ok, const QString &message) {
            qInfo().noquote() << "Upload test:" << name << (ok ? "converted" : "failed") << "slides" << m_presentations->slides(name).size() << message;
        });
    }
    auto *s = new QTcpSocket(this);
    connect(s, &QTcpSocket::connected, s, [s, pdf] {
        s->write("POST /api/upload?pres=Rooktest&file=rooktest.pdf HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: "
                 + QByteArray::number(pdf.size()) + "\r\n\r\n" + pdf);
    });
    connect(s, &QTcpSocket::disconnected, s, [s] {
        const QByteArray reply = s->readAll();
        qInfo().noquote() << "Upload test reply" << QString::fromLatin1(reply.left(12).mid(9)) << QString::fromUtf8(reply.mid(reply.indexOf("\r\n\r\n") + 4));
        s->deleteLater();
    });
    s->connectToHost(QHostAddress::LocalHost, m_server.serverPort());
}
