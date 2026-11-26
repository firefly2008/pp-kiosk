// SPDX-License-Identifier: GPL-2.0-or-later
#include "remoteserver.h"
#include "homebridge.h"
#include "paths.h"
#include "thirdparty/qrcodegen.hpp"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QNetworkInterface>
#include <QPainter>
#include <QRandomGenerator>
#include <QSettings>
#include <QTcpSocket>
#include <QTimer>
#include <QHostInfo>
#include <QUrlQuery>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace {
constexpr quint16 firstPort = 8642;

// Pagina voor de telefoon: groot, donker, één hand te bedienen
const char page[] = R"HTML(<!doctype html>
<html lang="nl"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover">
<meta name="theme-color" content="#0b0c0f">
<meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<meta name="apple-mobile-web-app-title" content="PP Kiosk">
<link rel="manifest" href="/manifest.webmanifest?c=__CODE__">
<link rel="icon" href="/icon-192.png">
<link rel="apple-touch-icon" href="/icon-192.png">
<title>PP Kiosk</title>
<style>
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent;user-select:none;-webkit-user-select:none}
html,body{height:100%}
body{margin:0;background:#0b0c0f;color:#fff;font:16px system-ui,sans-serif;display:flex;flex-direction:column;align-items:center;
padding:calc(env(safe-area-inset-top) + 12px) 14px calc(env(safe-area-inset-bottom) + 20px);overscroll-behavior:none}
header{width:284px;display:flex;align-items:center;gap:10px;margin:2px 0 14px}
header img{width:30px;height:30px;border-radius:8px}
header b{font-size:19px;font-weight:600;flex:1}
#dot{width:10px;height:10px;border-radius:50%;background:#555}
#dot.on{background:#3fb950}#dot.off{background:#e5484d}
#fs{background:none;font-size:20px;width:36px;height:36px;border-radius:10px}
.pad{display:grid;grid-template-columns:repeat(3,88px);grid-template-rows:repeat(3,88px);gap:10px;margin-bottom:16px}
button{border:0;border-radius:18px;background:#22262e;color:#fff;font-size:28px;touch-action:manipulation}
button:active{background:#3d6fb4}
.ok{background:#3d6fb4;font-size:22px;font-weight:600;border-radius:50%}
.row{display:grid;grid-template-columns:repeat(3,88px);gap:10px;margin-bottom:10px}
.row button{height:62px;font-size:17px}
.type{display:flex;gap:8px;width:284px;margin-top:10px}
input{flex:1;border:0;border-radius:14px;background:#22262e;color:#fff;font-size:18px;padding:14px;user-select:text;-webkit-user-select:text}
.type button{font-size:16px;padding:0 16px;height:52px}
#s{min-height:20px;margin-top:10px;color:#8a8f98;font-size:14px;text-align:center;width:284px}
#hint{display:none;width:284px;background:#1d2026;border:1px solid #3d6fb4;border-radius:14px;padding:12px 38px 12px 14px;margin-bottom:14px;font-size:14px;line-height:1.4;color:#d4d7dc;position:relative}
#hint button{position:absolute;right:6px;top:6px;width:28px;height:28px;font-size:16px;background:none}
</style></head><body>
<header><img src="/icon-192.png" alt=""><b>PP Kiosk</b><span id="dot" title=""></span><button id="fs" aria-label="Volledig scherm">⛶</button></header>
<div id="hint"><span id="hintText">Zet PP Kiosk als app op je telefoon: tik in Chrome op ⋮ en kies <b>Toevoegen aan startscherm</b>.</span><button id="hx" aria-label="Sluiten">✕</button></div>
<div class="pad">
<span></span><button data-k="up">▲</button><span></span>
<button data-k="left">◀</button><button class="ok" data-k="ok">OK</button><button data-k="right">▶</button>
<span></span><button data-k="down">▼</button><span></span>
</div>
<div class="row"><button data-k="back">↩ Terug</button><button data-k="home">⌂ Home</button><button data-k="menu">☰ Menu</button></div>
<div class="row"><button data-k="voldown">🔉</button><button data-k="playpause">⏯</button><button data-k="volup">🔊</button></div>
<div class="row"><button data-k="mute">🔇</button><button data-k="search">🔍 Zoek</button><button data-k="backspace">⌫</button></div>
<div class="type"><input id="t" placeholder="Typen…" autocomplete="off"><button id="send">Stuur</button></div>
<div id="s"></div>
<script>
// Code uit de link (QR-code) of eerder bewaard; zo blijft het werken als app op het beginscherm
const q=new URLSearchParams(location.search).get('c');
let c=q||'';try{if(q)localStorage.setItem('kl-code',q);else c=localStorage.getItem('kl-code')||''}catch(e){}
const s=document.getElementById('s'),dot=document.getElementById('dot');
const T={wrong:'Code klopt niet: scan de QR-code opnieuw',off:'Geen verbinding met PP Kiosk. Staat de pc aan en zit je op dezelfde wifi?',on:'Verbonden'};
function state(ok,msg){dot.className=ok?'on':'off';dot.title=ok?T.on:T.off;s.textContent=msg||''}
function send(path,body){fetch(path+'?c='+encodeURIComponent(c),{method:'POST',body:body||''})
.then(r=>state(r.status!==403,r.status===403?T.wrong:'')).catch(()=>state(false,T.off))}
function ping(){fetch('/ping?c='+encodeURIComponent(c),{cache:'no-store'}).then(r=>state(r.ok,r.status===403?T.wrong:'')).catch(()=>state(false,T.off))}
ping();setInterval(()=>{if(!document.hidden)ping()},5000);document.addEventListener('visibilitychange',()=>{if(!document.hidden)ping()});
const standalone=matchMedia('(display-mode: standalone)').matches||matchMedia('(display-mode: fullscreen)').matches||navigator.standalone;
// Volledig scherm: knop ⛶, en bij de eerste tik (niet als app, die is al schermvullend)
const fs=document.getElementById('fs');
function full(){const d=document.documentElement;if(!document.fullscreenElement&&d.requestFullscreen)d.requestFullscreen({navigationUI:'hide'}).catch(()=>{})}
fs.onclick=()=>{if(document.fullscreenElement)document.exitFullscreen();else full()};
if(standalone)fs.style.display='none';
let first=!standalone;document.addEventListener('pointerdown',()=>{if(first){first=false;full()}},{once:false,capture:true});
// Uitleg "als app installeren", één keer
const hint=document.getElementById('hint');let seen='';try{seen=localStorage.getItem('kl-hint')||''}catch(e){}
if(!standalone&&!seen)hint.style.display='block';
document.getElementById('hx').onclick=e=>{e.stopPropagation();hint.style.display='none';try{localStorage.setItem('kl-hint','1')}catch(x){}};
let deferred=null;window.addEventListener('beforeinstallprompt',e=>{e.preventDefault();deferred=e;
document.getElementById('hintText').innerHTML='Zet PP Kiosk als app op je telefoon: <b>tik hier</b>.';
hint.onclick=()=>{deferred.prompt();deferred=null;hint.style.display='none'}});
document.querySelectorAll('[data-k]').forEach(b=>{let t=null;const k=b.dataset.k;
b.addEventListener('pointerdown',e=>{e.preventDefault();navigator.vibrate&&navigator.vibrate(12);send('/k',k);
if(['up','down','left','right','voldown','volup','backspace'].includes(k))t=setTimeout(function r(){send('/k',k);t=setTimeout(r,140)},450)});
['pointerup','pointerleave','pointercancel'].forEach(ev=>b.addEventListener(ev,()=>{clearTimeout(t);t=null}))});
const i=document.getElementById('t');function go(){if(i.value){send('/t',i.value);i.value=''}}
document.getElementById('send').onclick=go;i.addEventListener('keydown',e=>{if(e.key==='Enter')go()});
if('serviceWorker' in navigator&&window.isSecureContext)navigator.serviceWorker.register('/sw.js').catch(()=>{});
</script></body></html>)HTML";

// Thuisnetwerk: 10.x, 172.16-31.x, 192.168.x, link-local, deze pc (ook IPv6 ULA/link-local)
bool isLocalPeer(QHostAddress a)
{
    bool ok = false;
    const quint32 v4 = a.toIPv4Address(&ok);
    if (ok) {
        a = QHostAddress(v4);
    }
    return a.isLoopback() || a.isPrivateUse() || a.isLinkLocal() || a.isSiteLocal() || a.isUniqueLocalUnicast();
}

// Vergelijken zonder dat de duur verraadt hoeveel cijfers goed zijn
bool sameCode(const QString &a, const QString &b)
{
    if (a.size() != b.size() || b.isEmpty()) {
        return false;
    }
    ushort diff = 0;
    for (int i = 0; i < a.size(); ++i) {
        diff |= a.at(i).unicode() ^ b.at(i).unicode();
    }
    return diff == 0;
}

#ifdef Q_OS_WIN
void sendKeys(std::initializer_list<WORD> keys)
{
    // Ingedrukt in volgorde, losgelaten in omgekeerde volgorde (voor combinaties zoals Alt+Links)
    QList<INPUT> inputs;
    for (WORD vk : keys) {
        INPUT in = {};
        in.type = INPUT_KEYBOARD;
        in.ki.wVk = vk;
        if (vk == VK_UP || vk == VK_DOWN || vk == VK_LEFT || vk == VK_RIGHT) {
            in.ki.dwFlags = KEYEVENTF_EXTENDEDKEY;
        }
        inputs.append(in);
    }
    for (auto it = keys.end(); it != keys.begin();) {
        --it;
        INPUT in = {};
        in.type = INPUT_KEYBOARD;
        in.ki.wVk = *it;
        in.ki.dwFlags = KEYEVENTF_KEYUP;
        if (*it == VK_UP || *it == VK_DOWN || *it == VK_LEFT || *it == VK_RIGHT) {
            in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        }
        inputs.append(in);
    }
    SendInput(UINT(inputs.size()), inputs.data(), sizeof(INPUT));
}

bool browserInFront()
{
    HWND w = GetForegroundWindow();
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    if (pid == GetCurrentProcessId()) {
        return false;
    }
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) {
        return false;
    }
    wchar_t path[MAX_PATH];
    DWORD size = MAX_PATH;
    QString exe;
    if (QueryFullProcessImageNameW(process, 0, path, &size)) {
        exe = QFileInfo(QString::fromWCharArray(path, int(size))).fileName().toLower();
    }
    CloseHandle(process);
    return exe == QLatin1String("msedge.exe") || exe == QLatin1String("chrome.exe");
}
#endif
} // namespace

RemoteServer::RemoteServer(const QString &dataDir, QObject *parent)
    : QObject(parent)
    , m_dataDir(dataDir)
{
    QSettings settings(appSettingsFile(), QSettings::IniFormat);
    m_code = settings.value(QStringLiteral("remote/code")).toString();
    // 4.7: zes cijfers (oudere codes van vier cijfers waren te makkelijk te raden)
    if (m_code.size() < 6) {
        newCode();
    }
    connect(&m_server, &QTcpServer::newConnection, this, [this] {
        while (QTcpSocket *socket = m_server.nextPendingConnection()) {
            // Alleen apparaten op het eigen netwerk (of deze pc); nooit vanaf internet
            if (!isLocalPeer(socket->peerAddress()) || m_open >= 32) {
                qInfo() << "Remote: refused connection from" << socket->peerAddress().toString();
                socket->abort();
                socket->deleteLater();
                continue;
            }
            ++m_open;
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] { handle(socket); });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QObject::destroyed, this, [this] { --m_open; });
            // Trage of half-open verbindingen niet eindeloos openhouden
            QTimer::singleShot(10000, socket, [socket] { socket->abort(); });
        }
    });
    if (settings.value(QStringLiteral("remote/enabled"), false).toBool()) {
        setEnabled(true);
    }
}

void RemoteServer::setEnabled(bool on)
{
    if (on == m_enabled && on == listening()) {
        return;
    }
    m_enabled = on;
    QSettings(appSettingsFile(), QSettings::IniFormat).setValue(QStringLiteral("remote/enabled"), on);
    if (on) {
        start();
    } else {
        m_server.close();
        qInfo() << "Remote: off";
    }
    Q_EMIT changed();
}

void RemoteServer::start()
{
    if (m_server.isListening()) {
        return;
    }
    for (quint16 port = firstPort; port < firstPort + 10; ++port) {
        if (m_server.listen(QHostAddress::AnyIPv4, port)) {
            break;
        }
    }
    qInfo() << "Remote: listening" << m_server.isListening() << address();
    makeQr();
}

QString RemoteServer::address() const
{
    if (!m_server.isListening()) {
        return {};
    }
    // Adres op het thuisnetwerk (192.168.x.x heeft voorrang, daarna 10.x en 172.16-31.x)
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
            int score = ip.startsWith(QLatin1String("192.168.")) ? 3 : ip.startsWith(QLatin1String("10.")) ? 2
                      : a.isPrivateUse() ? 1 : 0;
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

int RemoteServer::clients() const
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    int n = 0;
    for (qint64 t : m_seen) {
        if (now - t < 120) {
            ++n;
        }
    }
    return n;
}

void RemoteServer::newCode()
{
    m_code = QString::number(QRandomGenerator::system()->bounded(100000, 1000000));
    QSettings(appSettingsFile(), QSettings::IniFormat).setValue(QStringLiteral("remote/code"), m_code);
    m_seen.clear();
    makeQr();
    Q_EMIT changed();
    Q_EMIT clientsChanged();
}

void RemoteServer::makeQr()
{
    if (!m_server.isListening()) {
        return;
    }
    const QString url = address() + QStringLiteral("/?c=") + m_code;
    const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(url.toUtf8().constData(), qrcodegen::QrCode::Ecc::MEDIUM);
    const int border = 2;
    const int scale = 12;
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
    // Nieuwe bestandsnaam per code, zodat het scherm niet het oude plaatje uit de cache toont
    const QString file = m_dataDir + QStringLiteral("/remote-qr-%1.png").arg(m_code);
    image.save(file);
    m_qr = QUrl::fromLocalFile(file);
}

void RemoteServer::respond(QTcpSocket *socket, int status, const QByteArray &type, const QByteArray &body)
{
    const QByteArray reason = status == 200 ? "OK" : status == 403 ? "Forbidden" : status == 404 ? "Not Found"
                              : status == 429 ? "Too Many Requests" : "Bad Request";
    QByteArray head = "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason + "\r\n";
    head += "Content-Type: " + type + "\r\nContent-Length: " + QByteArray::number(body.size())
            + "\r\nCache-Control: no-store\r\nConnection: close"
            "\r\nX-Content-Type-Options: nosniff\r\nX-Frame-Options: DENY\r\nReferrer-Policy: no-referrer"
            "\r\nContent-Security-Policy: default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'unsafe-inline'; img-src 'self'; frame-ancestors 'none'"
            "\r\n\r\n";
    socket->write(head + body);
    socket->disconnectFromHost();
}

void RemoteServer::handle(QTcpSocket *socket)
{
    QByteArray data = socket->property("buffer").toByteArray() + socket->readAll();
    if (data.size() > 64 * 1024) {
        socket->abort();
        return;
    }
    const int headerEnd = data.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        socket->setProperty("buffer", data);
        return;
    }
    const QList<QByteArray> lines = data.left(headerEnd).split('\n');
    const QList<QByteArray> request = lines.value(0).trimmed().split(' ');
    int length = 0;
    for (const QByteArray &line : lines) {
        if (line.toLower().startsWith("content-length:")) {
            length = line.mid(15).trimmed().toInt();
        }
    }
    if (data.size() < headerEnd + 4 + length) {
        socket->setProperty("buffer", data);
        return;
    }
    const QByteArray body = data.mid(headerEnd + 4, length);
    const QByteArray method = request.value(0);
    const QUrl url(QString::fromUtf8(request.value(1)));
    const QString path = url.path();
    const QString code = QUrlQuery(url).queryItemValue(QStringLiteral("c"));
    const QString ip = socket->peerAddress().toString();
    const qint64 now = QDateTime::currentSecsSinceEpoch();

    // Alleen aanvragen voor een IP-adres, localhost of de naam van deze pc (tegen "DNS rebinding":
    // een website die zich voordoet als dit adres)
    QByteArray host;
    for (const QByteArray &line : lines) {
        if (line.toLower().startsWith("host:")) {
            host = line.mid(5).trimmed().toLower();
        }
    }
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
            qInfo() << "Remote: refused host" << h << "from" << ip;
            respond(socket, 403, "text/plain", "verboden");
            return;
        }
    }

    // Na 5 foute codes binnen 10 minuten: dit apparaat 10 minuten geblokkeerd (raden onmogelijk maken)
    const bool needsCode = method == "POST" || path == QLatin1String("/ping");
    if (needsCode && m_blockedUntil.value(ip) > now) {
        respond(socket, 429, "text/plain", "te veel pogingen, probeer het later");
        return;
    }
    if (needsCode && !sameCode(code, m_code)) {
        auto &fails = m_failures[ip];
        fails.append(now);
        while (!fails.isEmpty() && now - fails.first() > 600) {
            fails.removeFirst();
        }
        if (fails.size() >= 5) {
            m_blockedUntil.insert(ip, now + 600);
            fails.clear();
            qWarning() << "Remote: blocked" << ip << "after 5 wrong codes";
        }
    }

    const bool english = QSettings(appSettingsFile(), QSettings::IniFormat).value(QStringLiteral("global/language")).toString() == QLatin1String("en");
    if (method == "GET" && (path == QLatin1String("/") || path == QLatin1String("/index.html"))) {
        QByteArray html(page);
        // Code meegeven aan het manifest, zodat de app op het beginscherm meteen de juiste code heeft
        html.replace("__CODE__", QUrl::toPercentEncoding(code.isEmpty() ? QString() : code));
        if (english) {
            const QList<QPair<QByteArray, QByteArray>> en = {
                {"lang=\"nl\"", "lang=\"en\""}, {"↩ Terug", "↩ Back"}, {"🔍 Zoek", "🔍 Search"},
                {"Typen…", "Type…"}, {">Stuur<", ">Send<"}, {"Volledig scherm", "Full screen"}, {"\"Sluiten\"", "\"Close\""},
                {"Code klopt niet: scan de QR-code opnieuw", "Wrong code: scan the QR code again"},
                {"Geen verbinding met PP Kiosk. Staat de pc aan en zit je op dezelfde wifi?",
                 "No connection to PP Kiosk. Is the PC on and are you on the same Wi-Fi?"},
                {"on:'Verbonden'", "on:'Connected'"},
                {"Zet PP Kiosk als app op je telefoon: tik in Chrome op ⋮ en kies <b>Toevoegen aan startscherm</b>.",
                 "Put PP Kiosk on your phone as an app: tap ⋮ in Chrome and choose <b>Add to Home screen</b>."},
                {"Zet PP Kiosk als app op je telefoon: <b>tik hier</b>.", "Put PP Kiosk on your phone as an app: <b>tap here</b>."}};
            for (const auto &pair : en) {
                html.replace(pair.first, pair.second);
            }
        }
        respond(socket, 200, "text/html; charset=utf-8", html);
        return;
    }
    // App op het beginscherm (Android: Chrome → Toevoegen aan startscherm)
    if (method == "GET" && path == QLatin1String("/manifest.webmanifest")) {
        const QString start = code.isEmpty() ? QStringLiteral("/") : QStringLiteral("/?c=") + code;
        const QByteArray manifest = QStringLiteral(R"({
  "name": "PP Kiosk",
  "short_name": "PP Kiosk",
  "description": "%1",
  "lang": "%2",
  "start_url": "%3",
  "scope": "/",
  "display": "standalone",
  "orientation": "portrait",
  "background_color": "#0b0c0f",
  "theme_color": "#0b0c0f",
  "icons": [
    {"src": "/icon-192.png", "sizes": "192x192", "type": "image/png"},
    {"src": "/icon-512.png", "sizes": "512x512", "type": "image/png"},
    {"src": "/icon-maskable.png", "sizes": "512x512", "type": "image/png", "purpose": "maskable"}
  ]
})").arg(english ? QStringLiteral("Remote control for PP Kiosk") : QStringLiteral("Afstandsbediening voor PP Kiosk"),
         english ? QStringLiteral("en") : QStringLiteral("nl"), start).toUtf8();
        respond(socket, 200, "application/manifest+json", manifest);
        return;
    }
    if (method == "GET" && (path == QLatin1String("/icon-192.png") || path == QLatin1String("/icon-512.png") || path == QLatin1String("/icon-maskable.png"))) {
        const QString file = path == QLatin1String("/icon-192.png") ? QStringLiteral(":/assets/web-192.png")
                             : path == QLatin1String("/icon-512.png") ? QStringLiteral(":/assets/web-512.png")
                                                                      : QStringLiteral(":/assets/web-maskable-512.png");
        // Eerst openen en lezen, dan antwoorden (de volgorde van argumenten ligt in C++ niet vast:
        // in 4.6.0 kwam er zo een leeg plaatje)
        QFile f(file);
        const QByteArray png = f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
        respond(socket, png.isEmpty() ? 404 : 200, "image/png", png);
        return;
    }
    // Lege service worker (alleen gebruikt als de pagina ooit via https komt)
    if (method == "GET" && path == QLatin1String("/sw.js")) {
        respond(socket, 200, "text/javascript", "self.addEventListener('fetch',()=>{});");
        return;
    }
    // Verbonden? (bolletje rechtsboven in de app)
    if (method == "GET" && path == QLatin1String("/ping")) {
        const bool ok = sameCode(code, m_code);
        respond(socket, ok ? 200 : 403, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
        return;
    }
    if (method != "POST" || (path != QLatin1String("/k") && path != QLatin1String("/t"))) {
        respond(socket, 404, "text/plain", "niet gevonden");
        return;
    }
    if (!sameCode(code, m_code)) {
        qInfo() << "Remote: wrong code from" << ip;
        respond(socket, 403, "text/plain", "code klopt niet");
        return;
    }
    m_failures.remove(ip);
    const bool isNew = !m_seen.contains(ip) || QDateTime::currentSecsSinceEpoch() - m_seen.value(ip) >= 120;
    m_seen.insert(ip, QDateTime::currentSecsSinceEpoch());
    if (isNew) {
        qInfo() << "Remote: phone connected" << ip;
        Q_EMIT clientsChanged();
    }
    if (path == QLatin1String("/t")) {
        typeText(QString::fromUtf8(body).left(200));
        respond(socket, 200, "text/plain", "ok");
    } else {
        const bool ok = command(QString::fromUtf8(body).trimmed());
        respond(socket, ok ? 200 : 400, "text/plain", ok ? "ok" : "onbekend");
    }
}

bool RemoteServer::command(const QString &key)
{
    Q_EMIT commandReceived(key);
#ifdef Q_OS_WIN
    if (key == QLatin1String("up")) sendKeys({VK_UP});
    else if (key == QLatin1String("down")) sendKeys({VK_DOWN});
    else if (key == QLatin1String("left")) sendKeys({VK_LEFT});
    else if (key == QLatin1String("right")) sendKeys({VK_RIGHT});
    else if (key == QLatin1String("ok")) sendKeys({VK_RETURN});
    else if (key == QLatin1String("back")) {
        // In een website: vorige pagina; in PP Kiosk en spelers: Esc
        if (browserInFront()) sendKeys({VK_MENU, VK_LEFT});
        else sendKeys({VK_ESCAPE});
    } else if (key == QLatin1String("home")) {
        if (HomeBridge *home = HomeBridge::instance()) {
            QMetaObject::invokeMethod(home, &HomeBridge::goHome, Qt::QueuedConnection);
        }
    } else if (key == QLatin1String("menu")) sendKeys({VK_APPS});
    else if (key == QLatin1String("playpause")) sendKeys({VK_MEDIA_PLAY_PAUSE});
    else if (key == QLatin1String("volup")) sendKeys({VK_VOLUME_UP});
    else if (key == QLatin1String("voldown")) sendKeys({VK_VOLUME_DOWN});
    else if (key == QLatin1String("mute")) sendKeys({VK_VOLUME_MUTE});
    else if (key == QLatin1String("backspace")) sendKeys({VK_BACK});
    else if (key == QLatin1String("search")) sendKeys({VK_F3});
    else return false;
    qInfo() << "Remote:" << key;
    return true;
#else
    return !key.isEmpty();
#endif
}

void RemoteServer::typeText(const QString &text)
{
#ifdef Q_OS_WIN
    QList<INPUT> inputs;
    for (const QChar ch : text) {
        INPUT down = {};
        down.type = INPUT_KEYBOARD;
        down.ki.wScan = ch.unicode();
        down.ki.dwFlags = KEYEVENTF_UNICODE;
        INPUT up = down;
        up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        inputs << down << up;
    }
    if (!inputs.isEmpty()) {
        SendInput(UINT(inputs.size()), inputs.data(), sizeof(INPUT));
    }
#endif
    qInfo() << "Remote: typed" << text.size() << "characters";
}
