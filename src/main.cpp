// Bigscreen Host -- Fase 0/1 proof-of-concept (Optie A uit het migratieplan)
//
// Vervangt op Windows de combinatie van: startplasma-wayland (sessiestart),
// KWin (fullscreen containment + screenshots), envmanager (sessie-instellingen)
// en de XDG-portal-input-injectie uit inputhandler/. Draait als EEN proces,
// geen D-Bus, geen compositor-aanroepen.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QCommandLineParser>
#include <QFile>
#include <QMutex>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QRegularExpression>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QLocale>
#include <QSettings>
#include <QQuickView>
#include <QQuickWindow>
#include <QScreen>
#include <QStandardPaths>
#include <QtQml>

#include "appiconprovider.h"
#include "appmodel.h"
#include "paths.h"
#include "radiomodel.h"
#include "weather.h"
#include "updater.h"
#include "homebridge.h"
#include "systemtools.h"
#include "remoteserver.h"

#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QTimer>
#include <QQmlNetworkAccessManagerFactory>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "inputsystem.h"
#ifdef BIGSCREEN_HAVE_WIN32_INPUT
#include "win32inputsystem.h"
#else
#include "noopinputsystem.h"
#endif

namespace {

// Posters, achtergronden en castfoto's op schijf bewaren, zodat de bibliotheek
// ook zonder internet en zonder opnieuw downloaden snel laadt.
class CachingNetworkFactory : public QQmlNetworkAccessManagerFactory
{
public:
    explicit CachingNetworkFactory(const QString &dir)
        : m_dir(dir)
    {
    }
    // Afbeeldingen in QML krijgen een herkenbare User-Agent: sommige beeldbanken
    // (museum-IIIF-servers, Wikimedia) weigeren verzoeken zonder.
    class Manager : public QNetworkAccessManager
    {
    public:
        using QNetworkAccessManager::QNetworkAccessManager;
    protected:
        QNetworkReply *createRequest(Operation op, const QNetworkRequest &original, QIODevice *data) override
        {
            QNetworkRequest request(original);
            if (!request.hasRawHeader("User-Agent")) {
                request.setRawHeader("User-Agent",
                                     "Mozilla/5.0 (Windows NT 10.0; Win64; x64) PPKiosk/1.0");
            }
            return QNetworkAccessManager::createRequest(op, request, data);
        }
    };

    QNetworkAccessManager *create(QObject *parent) override
    {
        auto *manager = new Manager(parent);
        auto *cache = new QNetworkDiskCache(manager);
        cache->setCacheDirectory(m_dir);
        cache->setMaximumCacheSize(512LL * 1024 * 1024);
        manager->setCache(cache);
        return manager;
    }

private:
    QString m_dir;
};

} // namespace

QString appDataDir()
{
    static const QString dir = [] {
        QString base = qEnvironmentVariable("LOCALAPPDATA");
        if (base.isEmpty()) {
            base = QDir::tempPath();
        }
        base = QDir::fromNativeSeparators(base);
        const QString newDir = base + QStringLiteral("/PPKiosk");
        QDir().mkpath(newDir);
        return newDir;
    }();
    return dir;
}

namespace {

// De app is een GUI-programma zonder console: zonder logbestand verdwijnen alle
// foutmeldingen. Log staat in %LOCALAPPDATA%\\PPKiosk\\ppkiosk.log
QString g_logPath;
QMutex g_logMutex;

// Inloggegevens nooit in het logbestand: wachtwoorden/sleutels in links (…?password=…) en
// Xtream-adressen met gebruiker en wachtwoord in het pad (http://server:8080[/live]/<gebruiker>/<wachtwoord>/<id>)
QString redactSecrets(QString text)
{
    static const QRegularExpression query(QStringLiteral(R"(\b(password|passwd|pwd|username|user|token|api_key|apikey|key|access_token|auth)=[^&\s"']+)"),
                                          QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression xtream(QStringLiteral(R"((https?://[^/\s"']+)/((?:live|movie|series|timeshift)/)?[^/\s"']+/[^/\s"']+/(\d+(?:\.\w+)?))"),
                                           QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression userinfo(QStringLiteral(R"(://[^/\s"'@]+:[^/\s"'@]+@)"));
    text.replace(query, QStringLiteral("\\1=***"));
    text.replace(xtream, QStringLiteral("\\1/\\2***/***/\\3"));
    text.replace(userinfo, QStringLiteral("://***:***@"));
    return text;
}

void logHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    static const char *names[] = {"DEBUG", "WARN", "CRIT", "FATAL", "INFO"};
    const QString safe = redactSecrets(msg);
    QMutexLocker lock(&g_logMutex);
    QFile f(g_logPath);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream(&f) << QDateTime::currentDateTime().toString(Qt::ISODate) << " [" << names[type] << "] " << safe << "\n";
    }
}

void showFatal(const QString &text)
{
    qCritical().noquote() << text;
#ifdef Q_OS_WIN
    const QString full = text + QStringLiteral("\n\nLog: ") + g_logPath;
    MessageBoxW(nullptr, reinterpret_cast<LPCWSTR>(full.utf16()), L"PP Kiosk", MB_OK | MB_ICONERROR);
#endif
}

std::unique_ptr<InputSystem> createInputSystem()
{
#ifdef BIGSCREEN_HAVE_WIN32_INPUT
    auto system = std::make_unique<Win32InputSystem>();
#else
    auto system = std::make_unique<NoopInputSystem>();
#endif
    if (!system->init()) {
        qWarning() << "Input system could not be initialized; input injection will be disabled.";
    }
    return system;
}

// Vervangt Global::takeScreenshot() (components/bigscreenplugin/global.cpp),
// dat op Linux org.kde.KWin.ScreenShot2 over D-Bus aanriep. Hier heeft het
// Host-proces zelf het venster in handen, dus is dat niet meer nodig.
[[maybe_unused]] void takeScreenshot(QQuickWindow *window)
{
    if (!window) {
        qWarning() << "Could not take screenshot: no window";
        return;
    }

    const QImage result = window->grabWindow();

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
        + QStringLiteral("/Screenshots");
    QDir().mkpath(dir);

    const QString path = dir + QStringLiteral("/Screenshot_%1.png")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss")));

    if (!result.save(path)) {
        qWarning() << "Screenshot failed, could not save to" << path;
    } else {
        qInfo() << "Screenshot saved to" << path;
    }
}

} // namespace

int main(int argc, char *argv[])
{
    // Log zo vroeg mogelijk opzetten, nog voor QGuiApplication, zodat ook een
    // mislukte start van Qt zelf (platform-plugin, grafische backend) in het log komt.
    {
        const QString logDir = appDataDir();
        g_logPath = logDir + QStringLiteral("/ppkiosk.log");
        // Vorig log bewaren (voor "Logbestand delen")
        QFile::remove(logDir + QStringLiteral("/ppkiosk-vorige.log"));
        QFile::rename(g_logPath, logDir + QStringLiteral("/ppkiosk-vorige.log"));
        qInstallMessageHandler(logHandler);
        qInfo() << "main() reached; Qt" << qVersion() << "data" << logDir;
    }

    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("PP Kiosk"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/ppkiosk-256.png")));
    QGuiApplication::setOrganizationName(QStringLiteral("PP Kiosk"));

    // Alleen Nederlands
    const QString uiLanguage = QStringLiteral("nl");
    QLocale::setDefault(QLocale(QLocale::Dutch, QLocale::Netherlands));

    qInfo() << "QGuiApplication created; platform" << QGuiApplication::platformName();

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("PP Kiosk: dagstructuur, presentaties en media op een groot touchscreen"));
    parser.addHelpOption();
    QCommandLineOption screenOption(
        QStringList{QStringLiteral("screen")},
        QStringLiteral("Index van de monitor om fullscreen op te openen (bv. de HDMI-tv-uitgang)."),
        QStringLiteral("index"),
        QStringLiteral("-1"));
    parser.addOption(screenOption);
    // Gebruikt door "Starten met Windows"
    parser.addOption(QCommandLineOption(QStringLiteral("autostart"), QStringLiteral("Gestart door Windows bij het aanmelden.")));
    parser.process(app);

    // Snelkoppelingen naar websites (mediaproviders) en de radiozenders
    AppModel appModel(AppModel::Apps);
    AppModel webVideo(AppModel::Web, QStringLiteral("video"));
    AppModel webMusic(AppModel::Web, QStringLiteral("muziek"));
    AppModel webPodcasts(AppModel::Web, QStringLiteral("podcasts"));
    AppModel webNews(AppModel::Web, QStringLiteral("nieuws"));

    auto inputSystem = createInputSystem();
    Q_UNUSED(inputSystem);

    const int screenIndex = parser.value(screenOption).toInt();
    const auto screens = QGuiApplication::screens();
    QScreen *targetScreen = (screenIndex >= 0 && screenIndex < screens.size())
        ? screens.at(screenIndex)
        : QGuiApplication::primaryScreen();

    QQuickView view;
    view.setFlags(Qt::FramelessWindowHint);
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setScreen(targetScreen);
    view.setGeometry(targetScreen->geometry());
    view.setColor(Qt::white);
    QObject::connect(view.engine(), &QQmlEngine::quit, &app, &QCoreApplication::quit);
    view.engine()->addImageProvider(QStringLiteral("appicon"), new AppIconProvider);
    auto *ctx = view.rootContext();
    ctx->setContextProperty(QStringLiteral("appModel"), &appModel);
    ctx->setContextProperty(QStringLiteral("webVideo"), &webVideo);
    ctx->setContextProperty(QStringLiteral("webMusic"), &webMusic);
    ctx->setContextProperty(QStringLiteral("webPodcasts"), &webPodcasts);
    ctx->setContextProperty(QStringLiteral("webNews"), &webNews);
    RadioModel radio(appDataDir());
    ctx->setContextProperty(QStringLiteral("radio"), &radio);
    Weather weather;
    ctx->setContextProperty(QStringLiteral("weather"), &weather);
    Updater updater;
    ctx->setContextProperty(QStringLiteral("updater"), &updater);
    const QString selfTestMode = qEnvironmentVariable("BIGSCREEN_SELFTEST");
    if (selfTestMode.isEmpty()) {
        QTimer::singleShot(4000, &weather, &Weather::refresh);
        updater.startAutomatic();
    }
    view.engine()->setNetworkAccessManagerFactory(new CachingNetworkFactory(appDataDir() + QStringLiteral("/imagecache")));
    ctx->setContextProperty(QStringLiteral("uiLanguage"), uiLanguage == QLatin1String("en") ? QStringLiteral("en") : QStringLiteral("nl"));
    ctx->setContextProperty(QStringLiteral("selfTest"), selfTestMode);
    RemoteServer remote(appDataDir());
    ctx->setContextProperty(QStringLiteral("remote"), &remote);
    SystemTools systemTools;
    ctx->setContextProperty(QStringLiteral("systemTools"), &systemTools);
    HomeBridge home(&view);
    ctx->setContextProperty(QStringLiteral("homeBridge"), &home);
    home.setEscMode(QSettings(appSettingsFile(), QSettings::IniFormat).value(QStringLiteral("global/escMode")).toString());
    QObject::connect(&home, &HomeBridge::homeRequested, &view, [&view] {
        view.showFullScreen();
        view.raise();
        view.requestActivate();
    });
    qInfo() << "Target screen" << targetScreen->name() << targetScreen->geometry();
    view.setSource(QUrl(QStringLiteral("qrc:/qt/qml/nl/ppkiosk/Main.qml")));

    if (view.status() != QQuickView::Ready) {
        QString details = QStringLiteral("Main.qml kon niet geladen worden.");
        const auto errors = view.errors();
        for (const QQmlError &e : errors) {
            details += QStringLiteral("\n") + e.toString();
        }
        showFatal(details);
        return 1;
    }

    view.showFullScreen();
    view.raise();
    view.requestActivate();
    qInfo() << "Window shown" << view.geometry() << "visible" << view.isVisible();
    QObject::connect(view.engine(), &QQmlEngine::warnings, &view, [](const QList<QQmlError> &warnings) {
        for (const QQmlError &w : warnings) {
            qWarning().noquote() << "QML:" << w.toString();
        }
    });

    QObject::connect(&view, &QQuickWindow::sceneGraphError, &view, [](QQuickWindow::SceneGraphError, const QString &message) {
        showFatal(QStringLiteral("Grafische fout (scene graph): ") + message);
    });

    // takeScreenshot(&view) wordt in de echte app getriggerd door een QML-signaal
    // of controllerknop (zie Global::takeScreenshot() in het migratieplan),
    // niet hier op een vast moment -- de functie staat hierboven als referentie.

    // Bij afsluiten eerst de QML-schermen opruimen, dan pas de onderdelen waar ze naar kijken
    // (anders ~100 meldingen "Cannot read property … of null" en kans op een vastloper)
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &view, [&view] { view.setSource(QUrl()); });
    return app.exec();
}
