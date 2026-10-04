#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QFileSystemWatcher>
#include <QDir>
#include <QCoreApplication>
#include <QFontDatabase>
#include <QColor>
#include "core/config.h"
#include "core/theme.h"
#include "core/module_registry.h"
#include "core/hotkeys.h"
#include "services/clock_service.h"
#include "services/workspaces_service.h"
#include "services/window_status_service.h"
#include "services/indicators_service.h"
#include "services/graph_service.h"
#include "services/contrast_service.h"
#include "services/network_service.h"
#include "services/volume_service.h"
#include "ui/popup_host.h"
#include "ui/bar_manager.h"
#include <QFile>
#include <QTextStream>

void qbarLog(const QString& msg) {
    QFile f("qbar.log");
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream s(&f);
        s << msg << "\n";
        s.flush();
        f.close();
    }
}

// Load bundled fonts so they ship with the app and need no system install.
// MaterialSymbolsRounded is shipped as a 36.6 KB subset: the full variable font is
// 14.5 MB and 88% of it is the gvar table, which is never touched because every
// glyph is drawn at the default weight.
static void loadFonts() {
    const QStringList fonts = {
        "Poppins-Regular.ttf", "Poppins-Medium.ttf",
        "Poppins-SemiBold.ttf", "Poppins-Bold.ttf",
    };
    for (const auto& f : fonts) {
        QFontDatabase::addApplicationFont(":/fonts/" + f);
    }
}

// Resolve the real family name of the icon font at runtime. The subset reports
// its weight-named families (ExtraLight, Light, ...), so a hardcoded name breaks.
static QString loadIconFont() {
    const int id = QFontDatabase::addApplicationFont(":/fonts/MaterialSymbolsRounded.ttf");
    if (id < 0) return QStringLiteral("MaterialSymbolsRounded");
    const QStringList families = QFontDatabase::applicationFontFamilies(id);
    if (families.isEmpty()) return QStringLiteral("MaterialSymbolsRounded");
    return families.first();
}

static QString findConfigPath() {    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + "/config.json",
        QCoreApplication::applicationDirPath() + "/../resources/config.json",
        QDir::current().filePath("config.json"),
        QDir::current().filePath("resources/config.json"),
        QString("D:/project/Qbar/resources/config.json"),
    };
    for (const auto& p : candidates) {
        if (QFile::exists(p)) return QDir(p).absolutePath();
    }
    return candidates.last();
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("qbar");
    app.setOrganizationName("qbar");

    loadFonts();

    const QString configPath = findConfigPath();
    Config config;
    if (!config.load(configPath)) {
        qbarLog("config not found, using defaults: " + configPath);
    }

    Theme theme;
    theme.generate(config.themeSeed());
    theme.setFont(config.theme.font);

    // Module registry
    ModuleRegistry registry;
    registry.setConfig(&config);

    // Services
    ClockService clock;
    clock.setFormat(config.moduleConfig("clock").value("format").toString("HH:mm"));

    WorkspacesService workspaces;
    WindowStatusService windowStatus;
    windowStatus.setWorkspaces(&workspaces);
    IndicatorsService indicators;
    GraphService graph;
    NetworkService network;
    VolumeService volume;
    ContrastService contrast;
    PopupHost popupHost;

    // Wire window status to foreground window changes
    QTimer windowStatusTimer;
    windowStatusTimer.setInterval(500);
    QObject::connect(&windowStatusTimer, &QTimer::timeout, &windowStatus, &WindowStatusService::refresh);
    windowStatusTimer.start();

    // Network state changes on Wi-Fi association and on connectivity transitions
    QTimer networkTimer;
    networkTimer.setInterval(2000);
    QObject::connect(&networkTimer, &QTimer::timeout, &network, &NetworkService::refresh);
    networkTimer.start();

    BarManager barManager;
    barManager.setConfig(&config);
    barManager.setTheme(&theme);

    // Global hotkeys. The bar is WS_EX_NOACTIVATE and never takes focus, so a
    // registered hotkey is the only way to trigger an action from outside it.
    Hotkeys hotkeys;
    for (auto it = config.hotkeys.constBegin(); it != config.hotkeys.constEnd(); ++it) {
        if (!hotkeys.registerHotkey(it.key(), it.value()))
            qbarLog("hotkey registration failed: " + it.key() + " = " + it.value());
    }
    QObject::connect(&hotkeys, &Hotkeys::triggered, &app, [&](const QString& action) {
        if (action == "reload") barManager.reload();
        else if (action == "toggleBar") barManager.toggleBar();
    });
    // The QWidget must exist before its context property is registered. QML is
    // parsed inside setupWindow(), and a binding evaluated while the context
    // property is still undefined latches that undefined value permanently —
    // setContextProperty afterwards does not notify bindings. Creating the window
    // first is what keeps every barWindow.* property live.
    barManager.createWindow();

    // Bar text colour follows the wallpaper strip behind the bar, so the service
    // needs to know the surface it is drawing over.
    contrast.setAutoContrast(config.theme.autoContrast);
    contrast.setTextColors(QColor(config.theme.textOnLight), QColor(config.theme.textOnDark));
    contrast.setLightThreshold(config.theme.lightThreshold);
    contrast.setBarHeight(config.bar.height);
    contrast.setBackdrop(config.bar.backdrop, QColor(config.bar.backdropColor),
                         config.bar.surfaceOpacity);
    // QML reads a single property, so the contrast result is pushed onto the
    // theme object rather than exposed directly to every module. The setters
    // above already triggered the first evaluation, which emitted before this
    // connection existed, so the current value is applied once by hand.
    QObject::connect(&contrast, &ContrastService::changed,
                     &theme, [&]() { theme.setBarTextColor(contrast.textColorValue()); });
    theme.setBarTextColor(contrast.textColorValue());

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("barManager", &barManager);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("iconFont", loadIconFont());
    engine.rootContext()->setContextProperty("moduleRegistry", &registry);
    engine.rootContext()->setContextProperty("clockService", &clock);
    engine.rootContext()->setContextProperty("workspacesService", &workspaces);
    engine.rootContext()->setContextProperty("windowStatusService", &windowStatus);
    engine.rootContext()->setContextProperty("indicatorsService", &indicators);
    engine.rootContext()->setContextProperty("graphService", &graph);
    engine.rootContext()->setContextProperty("networkService", &network);
    engine.rootContext()->setContextProperty("popupHost", &popupHost);
    engine.rootContext()->setContextProperty("volumeService", &volume);
    engine.rootContext()->setContextProperty("contrastService", &contrast);

    // Register `barWindow` before setupWindow() parses the QML, otherwise every
    // barWindow.* binding latches undefined and never revives.
    engine.rootContext()->setContextProperty("barWindow", barManager.window());
    barManager.setupWindow(engine.rootContext());

    QFileSystemWatcher watcher;
    watcher.addPath(configPath);
    QObject::connect(&watcher, &QFileSystemWatcher::fileChanged, &app, [&]() {
        config.load(configPath);
        theme.generate(config.themeSeed());
        theme.setFont(config.theme.font);
        registry.setConfig(&config);
        clock.setFormat(config.moduleConfig("clock").value("format").toString("HH:mm"));
        contrast.setAutoContrast(config.theme.autoContrast);
        contrast.setTextColors(QColor(config.theme.textOnLight), QColor(config.theme.textOnDark));
        contrast.setLightThreshold(config.theme.lightThreshold);
        contrast.setBarHeight(config.bar.height);
        contrast.setBackdrop(config.bar.backdrop, QColor(config.bar.backdropColor),
                             config.bar.surfaceOpacity);
        barManager.reload();
        if (!watcher.files().contains(configPath)) {
            watcher.addPath(configPath);
        }
    });

    return app.exec();
}
