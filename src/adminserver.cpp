// SPDX-License-Identifier: GPL-2.0-or-later
#include "adminserver.h"
#include "agenda.h"
#include "paths.h"
#include "thirdparty/qrcodegen.hpp"

#include <QDateTime>
#include <QFile>
#include <QHostInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QPainter>
#include <QPointer>
#include <QRandomGenerator>
#include <QSettings>
#include <QTcpSocket>
#include <QTimer>
#include <QUrlQuery>

namespace {
constexpr quint16 firstPort = 8642;
constexpr int maxBody = 1024 * 1024;

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

AdminServer::AdminServer(const QString &dataDir, Agenda *agenda, QObject *parent)
    : QObject(parent)
    , m_dataDir(dataDir)
    , m_agenda(agenda)
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
                socket->deleteLater();
            });
            QTimer::singleShot(30000, socket, [socket] { socket->abort(); });
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

void AdminServer::respond(QTcpSocket *socket, int status, const QByteArray &type, const QByteArray &body)
{
    const QByteArray reason = status == 200 ? "OK" : status == 403 ? "Forbidden" : status == 404 ? "Not Found"
                              : status == 413 ? "Payload Too Large" : status == 429 ? "Too Many Requests" : "Bad Request";
    QByteArray head = "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason + "\r\n";
    head += "Content-Type: " + type + "\r\nContent-Length: " + QByteArray::number(body.size())
            + "\r\nCache-Control: no-store\r\nConnection: close"
              "\r\nX-Content-Type-Options: nosniff\r\nX-Frame-Options: DENY\r\nReferrer-Policy: no-referrer"
              "\r\nContent-Security-Policy: default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; frame-ancestors 'none'"
              "\r\n\r\n";
    socket->write(head + body);
    socket->disconnectFromHost();
}

void AdminServer::handle(QTcpSocket *socket)
{
    QByteArray data = socket->property("buffer").toByteArray() + socket->readAll();
    const int headerEnd = data.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        if (data.size() > 64 * 1024) {
            socket->abort();
            return;
        }
        socket->setProperty("buffer", data);
        return;
    }
    const QList<QByteArray> lines = data.left(headerEnd).split('\n');
    const QList<QByteArray> request = lines.value(0).trimmed().split(' ');
    qint64 length = 0;
    QByteArray host;
    QString code;
    for (const QByteArray &raw : lines) {
        const QByteArray line = raw.trimmed();
        const QByteArray lower = line.toLower();
        if (lower.startsWith("content-length:")) {
            length = line.mid(15).trimmed().toLongLong();
        } else if (lower.startsWith("host:")) {
            host = line.mid(5).trimmed().toLower();
        } else if (lower.startsWith("x-code:")) {
            code = QString::fromLatin1(line.mid(7).trimmed());
        }
    }
    if (length < 0 || length > maxBody) {
        respond(socket, 413, "text/plain; charset=utf-8", "te groot");
        return;
    }
    if (data.size() < headerEnd + 4 + length) {
        socket->setProperty("buffer", data);
        return;
    }
    const QByteArray body = data.mid(headerEnd + 4, length);
    const QByteArray method = request.value(0);
    const QUrl url(QString::fromUtf8(request.value(1)));
    const QString path = url.path();
    const QHostAddress peer = socket->peerAddress();
    const QString ip = peer.toString();
    const bool local = peer.isLoopback() || peer == QHostAddress(QStringLiteral("::ffff:127.0.0.1"));
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
            return;
        }
    }

    // Pagina zelf mag iedereen ophalen; gegevens alleen met de code (of op deze pc zelf)
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
    if (!path.startsWith(QLatin1String("/api/"))) {
        respond(socket, 404, "text/plain; charset=utf-8", "niet gevonden");
        return;
    }

    // Na 5 foute codes binnen 10 minuten: dit apparaat 10 minuten geblokkeerd
    if (!local) {
        if (m_blockedUntil.value(ip) > now) {
            respond(socket, 429, "text/plain; charset=utf-8", "te veel pogingen, probeer het over 10 minuten");
            return;
        }
        if (!sameCode(code, m_code)) {
            auto &fails = m_failures[ip];
            fails.append(now);
            while (!fails.isEmpty() && now - fails.first() > 600) {
                fails.removeFirst();
            }
            if (fails.size() >= 5) {
                m_blockedUntil.insert(ip, now + 600);
                qInfo() << "Admin: blocked" << ip;
            }
            respond(socket, 403, "text/plain; charset=utf-8", "code klopt niet");
            return;
        }
        m_failures.remove(ip);
    }

    if (path == QLatin1String("/api/agenda")) {
        if (method == "GET") {
            respond(socket, 200, "application/json", m_agenda->toJson());
            return;
        }
        if (method == "PUT" || method == "POST") {
            QString error;
            if (m_agenda->setJson(body, &error)) {
                respond(socket, 200, "application/json", "{\"ok\":true}");
            } else {
                respond(socket, 400, "application/json", QJsonDocument(QJsonObject{{QStringLiteral("error"), error}}).toJson(QJsonDocument::Compact));
            }
            return;
        }
    }
    if (path == QLatin1String("/api/check") && method == "GET") {
        respond(socket, 200, "application/json", "{\"ok\":true}");
        return;
    }
    respond(socket, 404, "text/plain; charset=utf-8", "niet gevonden");
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
