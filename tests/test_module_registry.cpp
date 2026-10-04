#include <QtTest>

#include "core/config.h"
#include "core/module_registry.h"

class TestModuleRegistry : public QObject {
    Q_OBJECT

private:
    // Writes a config file with the given layout and enabled flags.
    QString writeConfig(const QStringList& left, const QStringList& right,
                        const QMap<QString, bool>& enabled);

private slots:
    // The registry is the only thing standing between config.json and a Loader
    // pointed at a qrc URL that does not exist. That failure happens at runtime,
    // not at build time, which is why the ids shipped in the registry had to be
    // checked by hand: media, tray, notifications and volume were all listed
    // with no QML file behind them.
    void everyRegisteredIdResolvesToRealQml();
    void unregisteredIdHasNoUrl();
    void layoutRespectsEnabledFlag();
    void unknownIdIsSilentlyDropped();
    void duplicateIdAppearsOnce();

    void rebuildIsIdempotent();
};

// QTemporaryDir is created once per test process by QTest's temp handling; here
// each writeConfig gets its own directory so parallel runs cannot collide.
QString TestModuleRegistry::writeConfig(const QStringList& left,
                                        const QStringList& right,
                                        const QMap<QString, bool>& enabled) {
    static int counter = 0;
    const QString path =
        QStringLiteral("%1/qbar-registry-%2-%3.json")
            .arg(QDir::tempPath())
            .arg(QCoreApplication::applicationPid())
            .arg(++counter);

    QJsonObject modules;
    for (auto it = enabled.constBegin(); it != enabled.constEnd(); ++it)
        modules[it.key()] = QJsonObject{{"enabled", it.value()}};

    QJsonObject root{
        {"layout", QJsonObject{
            {"left", QJsonArray::fromStringList(left)},
            {"center", QJsonArray{}},
            {"right", QJsonArray::fromStringList(right)}
        }},
        {"modules", modules}
    };

    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(QJsonDocument(root).toJson());
    f.close();
    return path;
}

void TestModuleRegistry::everyRegisteredIdResolvesToRealQml() {
    Config config;
    ModuleRegistry registry;

    // Ids the app intends to ship. If you add one, add it here too - that is the
    // whole point of this test.
    const QStringList expected{"clock", "graph", "indicators",
                               "network", "windowstatus", "workspaces"};

    const QString path = writeConfig(expected, {}, {});
    QVERIFY(config.load(path));
    registry.setConfig(&config);

    for (const QString& id : expected) {
        const QString url = registry.qmlUrlFor(id);
        QVERIFY2(!url.isEmpty(), qPrintable(QStringLiteral("no url for %1").arg(id)));
        QVERIFY2(url.startsWith("qrc:/"),
                 qPrintable(QStringLiteral("unexpected url shape: %1").arg(url)));

        // The real check. resources.qrc is linked into this test binary, so a
        // missing entry here means the Loader would resolve to nothing at
        // runtime - which is exactly how "graph" ended up mapped twice while
        // other ids had no QML file at all.
        //
        // QML accepts the qrc:/ URL form in a Loader source, but QFile only
        // understands the :/ path form, hence the conversion.
        const QString resourcePath = QStringLiteral(":/") + url.mid(QStringLiteral("qrc:/").length());
        QVERIFY2(QFile::exists(resourcePath),
                 qPrintable(QStringLiteral(
                     "%1 resolves to %2, which is not in resources.qrc")
                     .arg(id, url)));
    }
}

void TestModuleRegistry::unregisteredIdHasNoUrl() {
    ModuleRegistry registry;
    // These were shipped in the registry with no QML behind them, which meant a
    // crash the moment anyone put them in layout.
    QVERIFY(registry.qmlUrlFor("media").isEmpty());
    QVERIFY(registry.qmlUrlFor("tray").isEmpty());
    QVERIFY(registry.qmlUrlFor("notifications").isEmpty());
    QVERIFY(registry.qmlUrlFor("volume").isEmpty());
    QVERIFY(registry.qmlUrlFor("nonexistent").isEmpty());
}

void TestModuleRegistry::layoutRespectsEnabledFlag() {
    const QString path = writeConfig({"clock", "graph"}, {"network"},
                                     {{"clock", true}, {"graph", false}});
    Config config;
    QVERIFY(config.load(path));

    ModuleRegistry registry;
    registry.setConfig(&config);

    QCOMPARE(registry.leftModules().size(), 1);
    QCOMPARE(registry.leftModules().first().toString(), QString("clock"));
    QCOMPARE(registry.rightModules().size(), 1);
    QCOMPARE(registry.rightModules().first().toString(), QString("network"));
}

void TestModuleRegistry::unknownIdIsSilentlyDropped() {
    const QString path = writeConfig({"clock", "notAModule"}, {"alsoFake"},
                                     {});
    Config config;
    QVERIFY(config.load(path));

    ModuleRegistry registry;
    registry.setConfig(&config);

    // A typo in config.json should not break the bar, and should not leave an
    // empty Loader behind either.
    QCOMPARE(registry.leftModules().size(), 1);
    QCOMPARE(registry.leftModules().first().toString(), QString("clock"));
    QCOMPARE(registry.rightModules().size(), 0);
}

void TestModuleRegistry::duplicateIdAppearsOnce() {
    // "graph" was listed twice in the registry map. The filter also de-dupes the
    // layout list, so this guards the layout side of the same mistake.
    const QString path = writeConfig({"clock", "graph", "clock"}, {"network", "network"}, {});
    Config config;
    QVERIFY(config.load(path));

    ModuleRegistry registry;
    registry.setConfig(&config);

    QCOMPARE(registry.leftModules().size(), 2);
    QCOMPARE(registry.rightModules().size(), 1);
}

void TestModuleRegistry::rebuildIsIdempotent() {
    const QString path = writeConfig({"clock", "graph"}, {"network", "clock"}, {});
    Config config;
    QVERIFY(config.load(path));

    ModuleRegistry registry;
    registry.setConfig(&config);
    const int leftCount = registry.leftModules().size();

    for (int i = 0; i < 5; ++i)
        registry.rebuild();

    QCOMPARE(registry.leftModules().size(), leftCount);
}

QTEST_GUILESS_MAIN(TestModuleRegistry)
#include "test_module_registry.moc"
