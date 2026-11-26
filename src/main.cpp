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
#include <QTranslator>
#include <QQuickView>
#include <QQuickWindow>
#include <QScreen>
#include <QStandardPaths>
#include <QtQml>

#include "appiconprovider.h"
#include "appmodel.h"
#include "gamepadnavigator.h"
#include "medialibrary.h"
#include "artframe.h"
#include "paths.h"
#include "apikeys.h"
#include "recorder.h"
#include "streamrelay.h"
#include "components.h"
#include "radiomodel.h"
#include "parental.h"
#include "weather.h"
#include "updater.h"
#include "cec.h"
#include "homebridge.h"
#include "systemtools.h"
#include "remoteserver.h"
#include "nowtvmodel.h"
#include "photosearch.h"
#include "profiles.h"
#include "usagemodel.h"
#include "resumemodel.h"
#include "mpcplayer.h"
#include "iptvmodel.h"

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
                                     "Mozilla/5.0 (Windows NT 10.0; Win64; x64) Kijkluister/3.8 (+https://github.com/firefly2008/bigscreen-host-windows)");
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
        const QString oldDir = base + QStringLiteral("/BigscreenHost");
        const QString newDir = base + QStringLiteral("/Kijkluister");
        // Oude map (tot en met 3.8.1) meenemen: instellingen, bibliotheek, zenders en cache
        if (QDir(oldDir).exists() && !QDir(newDir).exists()) {
            if (!QDir().rename(oldDir, newDir)) {
                return oldDir; // in gebruik of geen rechten: dan de oude map blijven gebruiken
            }
        }
        QDir().mkpath(newDir);
        return newDir;
    }();
    return dir;
}

namespace {

// De app is een GUI-programma zonder console: zonder logbestand verdwijnen alle
// foutmeldingen. Log staat in %LOCALAPPDATA%\\Kijkluister\\kijkluister.log
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
    MessageBoxW(nullptr, reinterpret_cast<LPCWSTR>(full.utf16()), L"Kijkluister", MB_OK | MB_ICONERROR);
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
        g_logPath = logDir + QStringLiteral("/kijkluister.log");
        // Vorig log bewaren (voor "Logbestand delen")
        QFile::remove(logDir + QStringLiteral("/kijkluister-vorige.log"));
        QFile::rename(g_logPath, logDir + QStringLiteral("/kijkluister-vorige.log"));
        QFile::remove(logDir + QStringLiteral("/bigscreen-host.log")); // oude naam
        qInstallMessageHandler(logHandler);
        qInfo() << "main() reached; Qt" << qVersion() << "data" << logDir;
    }

    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Kijkluister"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/kijkluister-256.png")));
    QGuiApplication::setOrganizationName(QStringLiteral("KDE"));

    // Taal: Nederlands (standaard) of Engels (Instellingen → Meer instellingen → Taal / Language)
    // (BIGSCREEN_LANGUAGE: voor de rooktest)
    const QString uiLanguage = qEnvironmentVariableIsSet("BIGSCREEN_LANGUAGE")
                                   ? qEnvironmentVariable("BIGSCREEN_LANGUAGE")
                                   : QSettings(appSettingsFile(), QSettings::IniFormat)
                                         .value(QStringLiteral("global/language"), QStringLiteral("nl")).toString();
    static QTranslator translator;
    if (uiLanguage == QLatin1String("en") && translator.load(QStringLiteral(":/i18n/kijkluister_en.qm"))) {
        QCoreApplication::installTranslator(&translator);
        qInfo() << "Translator loaded: en";
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedKingdom));
    } else {
        QLocale::setDefault(QLocale(QLocale::Dutch, QLocale::Netherlands));
    }

    qInfo() << "QGuiApplication created; platform" << QGuiApplication::platformName();

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Kijkluister: tv, films, series, muziek en podcasts op een groot scherm"));
    parser.addHelpOption();
    QCommandLineOption screenOption(
        QStringList{QStringLiteral("screen")},
        QStringLiteral("Index van de monitor om fullscreen op te openen (bv. de HDMI-tv-uitgang)."),
        QStringLiteral("index"),
        QStringLiteral("-1"));
    parser.addOption(screenOption);
    // Gebruikt door "Starten met Windows" en het wisselen van profiel (zonder deze stopte Kijkluister met
    // "Unknown option" in 4.4.0)
    parser.addOption(QCommandLineOption(QStringLiteral("autostart"), QStringLiteral("Gestart door Windows bij het aanmelden.")));
    parser.addOption(QCommandLineOption(QStringLiteral("profile-chosen"), QStringLiteral("Profiel is al gekozen; niet opnieuw vragen.")));
    parser.process(app);

    // Gamepad-navigatie (SDL3) en de lijst met toepassingen voor de startpagina
    GamepadNavigator gamepad;
    AppModel appModel(AppModel::Apps);
    AppModel webVideo(AppModel::Web, QStringLiteral("video"));
    AppModel webMusic(AppModel::Web, QStringLiteral("muziek"));
    AppModel webPodcasts(AppModel::Web, QStringLiteral("podcasts"));
    AppModel webNews(AppModel::Web, QStringLiteral("nieuws"));
    MediaLibrary library(appDataDir());
    IptvModel iptv(appDataDir());
    ArtFrame frame(appDataDir());
    Recorder recorder(appDataDir());
    StreamRelay streamRelay;
    ApiKeys apiKeys(appDataDir());
    Components components(appDataDir());
    MpcPlayer mpc(appDataDir());

    auto inputSystem = createInputSystem();
    Q_UNUSED(inputSystem); // in dit skeleton nog niet aan QML-signalen gekoppeld

    const int screenIndex = parser.value(screenOption).toInt();
    const auto screens = QGuiApplication::screens();
    QScreen *targetScreen = (screenIndex >= 0 && screenIndex < screens.size())
        ? screens.at(screenIndex)
        : QGuiApplication::primaryScreen();

    QQuickView view;
    view.setFlags(Qt::FramelessWindowHint); // StaysOnTop bewust uit tijdens de PoC-fase
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setScreen(targetScreen);
    view.setGeometry(targetScreen->geometry());
    view.setColor(Qt::black);
    QObject::connect(view.engine(), &QQmlEngine::quit, &app, &QCoreApplication::quit);
    view.engine()->addImageProvider(QStringLiteral("appicon"), new AppIconProvider);
    view.rootContext()->setContextProperty(QStringLiteral("appModel"), &appModel);
    view.rootContext()->setContextProperty(QStringLiteral("webVideo"), &webVideo);
    view.rootContext()->setContextProperty(QStringLiteral("webMusic"), &webMusic);
    view.rootContext()->setContextProperty(QStringLiteral("webPodcasts"), &webPodcasts);
    view.rootContext()->setContextProperty(QStringLiteral("webNews"), &webNews);
    view.rootContext()->setContextProperty(QStringLiteral("library"), &library);
    view.rootContext()->setContextProperty(QStringLiteral("iptv"), &iptv);
    view.rootContext()->setContextProperty(QStringLiteral("frame"), &frame);
    view.rootContext()->setContextProperty(QStringLiteral("recorder"), &recorder);
    view.rootContext()->setContextProperty(QStringLiteral("streamRelay"), &streamRelay);
    view.rootContext()->setContextProperty(QStringLiteral("apiKeys"), &apiKeys);
    view.rootContext()->setContextProperty(QStringLiteral("mpc"), &mpc);
    view.rootContext()->setContextProperty(QStringLiteral("components"), &components);
    RadioModel radio(appDataDir());
    view.rootContext()->setContextProperty(QStringLiteral("radio"), &radio);
    ResumeModel resume(appDataDir(), &library);
    view.rootContext()->setContextProperty(QStringLiteral("resume"), &resume);
    Parental parental;
    view.rootContext()->setContextProperty(QStringLiteral("parental"), &parental);
    Weather weather;
    view.rootContext()->setContextProperty(QStringLiteral("weather"), &weather);
    Updater updater;
    view.rootContext()->setContextProperty(QStringLiteral("updater"), &updater);
    Cec cec;
    view.rootContext()->setContextProperty(QStringLiteral("cec"), &cec);
    QObject::connect(&cec, &Cec::button, &gamepad, &GamepadNavigator::handleButton);
    if (qEnvironmentVariable("BIGSCREEN_SELFTEST").isEmpty()) {
        QTimer::singleShot(4000, &weather, &Weather::refresh);
        updater.startAutomatic();
        QTimer::singleShot(2000, &cec, &Cec::start);
    }
    // MPC-HC en ffmpeg op de achtergrond ophalen of bijwerken (niet tijdens de tests)
    if (qEnvironmentVariable("BIGSCREEN_SELFTEST").isEmpty()) {
        QTimer::singleShot(20000, &components, &Components::startBackground);
    }
    // Na de film of serie terug naar Kijkluister
    QObject::connect(&mpc, &MpcPlayer::finished, &view, [&view] {
        view.raise();
        view.requestActivate();
    });
    view.engine()->setNetworkAccessManagerFactory(new CachingNetworkFactory(appDataDir() + QStringLiteral("/imagecache")));
    view.rootContext()->setContextProperty(QStringLiteral("gamepad"), &gamepad);
    // Alleen voor de geautomatiseerde test: opent bij het starten een menu
    view.rootContext()->setContextProperty(QStringLiteral("uiLanguage"), uiLanguage == QLatin1String("en") ? QStringLiteral("en") : QStringLiteral("nl"));
    view.rootContext()->setContextProperty(QStringLiteral("selfTest"), qEnvironmentVariable("BIGSCREEN_SELFTEST"));
    view.rootContext()->setContextProperty(QStringLiteral("selfTestM3u"), qEnvironmentVariable("BIGSCREEN_TEST_M3U"));
    view.rootContext()->setContextProperty(QStringLiteral("selfTestRecordDir"), qEnvironmentVariable("BIGSCREEN_TEST_RECORD_DIR"));
    // Gratis zenders klaarzetten (de tests gebruiken hun eigen lijst)
    const QString selfTestMode = qEnvironmentVariable("BIGSCREEN_SELFTEST");
    if (selfTestMode.isEmpty() || selfTestMode == QLatin1String("iptvfree")) {
        iptv.addFreeSourcesOnce();
    }
    Profiles profiles;
    view.rootContext()->setContextProperty(QStringLiteral("profiles"), &profiles);
    parental.setKids(profiles.kids());
    UsageModel usage;
    usage.addSource(QStringLiteral("media"), &appModel);
    usage.addSource(QStringLiteral("video"), &webVideo);
    usage.addSource(QStringLiteral("muziek"), &webMusic);
    usage.addSource(QStringLiteral("podcasts"), &webPodcasts);
    usage.addSource(QStringLiteral("nieuws"), &webNews);
    usage.addSource(QStringLiteral("radio"), &radio);
    view.rootContext()->setContextProperty(QStringLiteral("usage"), &usage);
    RemoteServer remote(appDataDir());
    view.rootContext()->setContextProperty(QStringLiteral("remote"), &remote);
    // Serie opnemen: elke gevonden uitzending inplannen (dubbele slaat de recorder over)
    QObject::connect(&iptv, &IptvModel::seriesEpisode, &recorder,
                     [&recorder](const QString &channel, const QString &url, const QString &title, qint64 start, qint64 stop) {
                         if (!recorder.isScheduled(url, start)) {
                             recorder.addToSchedule(channel, url, title, start, stop);
                         }
                     });
    NowTvModel nowTv(&iptv);
    view.rootContext()->setContextProperty(QStringLiteral("nowTv"), &nowTv);
    SystemTools systemTools;
    view.rootContext()->setContextProperty(QStringLiteral("systemTools"), &systemTools);
    PhotoSearch photos(appDataDir());
    view.rootContext()->setContextProperty(QStringLiteral("photos"), &photos);
    HomeBridge home(&view);
    view.rootContext()->setContextProperty(QStringLiteral("homeBridge"), &home);
    home.setEscMode(QSettings(appDataDir() + QStringLiteral("/settings.ini"), QSettings::IniFormat).value(QStringLiteral("global/escMode")).toString());
    QObject::connect(&home, &HomeBridge::homeRequested, &gamepad, &GamepadNavigator::homeRequested);
    QObject::connect(&gamepad, &GamepadNavigator::guidePressed, &home, &HomeBridge::goHome);
    QObject::connect(&cec, &Cec::homeRequested, &home, &HomeBridge::goHome);
    QObject::connect(&mpc, &MpcPlayer::runningChanged, &home, [&mpc, &home] {
        if (mpc.running()) {
            home.externalLaunched();
        }
    });
    QObject::connect(&gamepad, &GamepadNavigator::homeRequested, &view, [&view] {
        view.showFullScreen();
        view.raise();
        view.requestActivate();
    });
    qInfo() << "Target screen" << targetScreen->name() << targetScreen->geometry();
    view.setSource(QUrl(QStringLiteral("qrc:/qt/qml/org/kde/plasma/bigscreen/host/Main.qml")));

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
