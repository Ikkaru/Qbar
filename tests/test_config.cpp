#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>

#include "core/config.h"
#include "core/hotkeys.h"

class TestConfig : public QObject {
    Q_OBJECT

private:
    // Each config file is written from JSON so a case states exactly the shape it
    // needs, instead of depending on resources/config.json staying put.
    QString writeConfig(const QJsonObject& obj);

    QTemporaryDir m_dir;

private slots:
    // Regression: load() ran again on every hot reload and appended to
    // layout.left / modules without clearing them first, so saving config.json
    // repeatedly filled the bar with duplicate modules.
    void reloadDoesNotAccumulate();
    void reloadDoesNotDuplicateModules();

    void hotkeysHaveDefaults();
    void hotkeysOverrideDefaults();
    void hotkeysSurviveSaveRoundTrip();

    void missingSectionsFallBackToDefaults();
    void malformedModuleEntryIsIgnored();

    // The chord comes from config.json, so it is untrusted input. A typo used
    // to fail silently: the hotkey simply never registered.
    void chordParsesSingleCharacter();
    void chordParsesNamedKey();
    void chordParsesFunctionKey();
    void chordAcceptsModifierOrder();
    void chordRejectsGarbage();
    void chordAlwaysSetsNoRepeat();
};

QString TestConfig::writeConfig(const QJsonObject& obj) {
    const QString path = m_dir.filePath(QStringLiteral("config.json"));
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(QJsonDocument(obj).toJson());
    f.close();
    return path;
}

void TestConfig::reloadDoesNotAccumulate() {
    const QString path = writeConfig(QJsonObject{
        {"layout", QJsonObject{
            {"left", QJsonArray::fromStringList({"clock", "graph"})},
            {"center", QJsonArray()},
            {"right", QJsonArray::fromStringList({"network"})}}}
    });

    Config c;
    QVERIFY(c.load(path));
    QCOMPARE(c.layout.left, QStringList({"clock", "graph"}));
    QCOMPARE(c.layout.right, QStringList({"network"}));

    // Ten hot reloads is not an exaggeration: every save of config.json fires the
    // file watcher, and editing one value means saving more than once.
    for (int i = 0; i < 10; ++i)
        QVERIFY(c.load(path));

    QCOMPARE(c.layout.left, QStringList({"clock", "graph"}));
    QCOMPARE(c.layout.right, QStringList({"network"}));
}

void TestConfig::reloadDoesNotDuplicateModules() {
    const QString path = writeConfig(QJsonObject{
        {"modules", QJsonObject{
            {"clock", QJsonObject{{"enabled", true}, {"format", "HH:mm"}}},
            {"graph", QJsonObject{{"enabled", true}}},
        }}
    });

    Config c;
    for (int i = 0; i < 5; ++i)
        QVERIFY(c.load(path));

    QCOMPARE(c.modules.size(), 2);
    QCOMPARE(c.modules.keys(), QStringList({"clock", "graph"}));
}

void TestConfig::hotkeysHaveDefaults() {
    const QString path = writeConfig(QJsonObject{});
    Config c;
    QVERIFY(c.load(path));

    // A config with no hotkeys section still gets both, otherwise deleting the
    // section silently unregisters every shortcut.
    QCOMPARE(c.hotkeys.value("reload"), QString("Ctrl+Alt+R"));
    QCOMPARE(c.hotkeys.value("toggleBar"), QString("Ctrl+Alt+B"));
}

void TestConfig::hotkeysOverrideDefaults() {
    const QString path = writeConfig(QJsonObject{
        {"hotkeys", QJsonObject{
            {"reload", "Ctrl+Shift+F5"},
            {"clearNotifications", "Win+Shift+N"},
        }}
    });
    Config c;
    QVERIFY(c.load(path));

    QCOMPARE(c.hotkeys.value("reload"), QString("Ctrl+Shift+F5"));
    QCOMPARE(c.hotkeys.value("clearNotifications"), QString("Win+Shift+N"));
    // Not overridden, so the default has to survive a partial section.
    QCOMPARE(c.hotkeys.value("toggleBar"), QString("Ctrl+Alt+B"));
}

void TestConfig::hotkeysSurviveSaveRoundTrip() {
    const QString path = writeConfig(QJsonObject{
        {"hotkeys", QJsonObject{{"reload", "Ctrl+Alt+R"}}}
    });

    Config first;
    QVERIFY(first.load(path));

    const QString out = m_dir.filePath(QStringLiteral("out.json"));
    QVERIFY(first.save(out));

    Config second;
    QVERIFY(second.load(out));
    QCOMPARE(second.hotkeys.value("reload"), first.hotkeys.value("reload"));
    QCOMPARE(second.hotkeys.size(), first.hotkeys.size());
}

void TestConfig::missingSectionsFallBackToDefaults() {
    const QString path = writeConfig(QJsonObject{});
    Config c;
    QVERIFY(c.load(path));

    // Values the app depends on rather than merely prefers.
    QCOMPARE(c.bar.height, 40);
    QCOMPARE(c.theme.font, QString("Segoe UI Variable"));
    QCOMPARE(c.theme.lightThreshold, 165);
    QVERIFY(c.bar.shadowEnabled);
}

void TestConfig::malformedModuleEntryIsIgnored() {
    const QString path = writeConfig(QJsonObject{
        {"modules", QJsonObject{
            {"clock", "not-an-object"},   // wrong type entirely
            {"graph", QJsonObject{}},      // no enabled key
        }}
    });

    Config c;
    QVERIFY(c.load(path));

    // A string where an object was expected must not crash the load.
    // moduleConfig on it yields an empty object, which reads as "default".
    QVERIFY(c.moduleConfig("clock").isEmpty());
    // No "enabled" key means enabled, so the module stays visible.
    QVERIFY(c.moduleEnabled("graph"));
    // Unknown ids default to enabled rather than silently vanishing.
    QVERIFY(c.moduleEnabled("somethingNeverHeardOf"));
}

void TestConfig::chordParsesSingleCharacter() {
    UINT mods = 0, vk = 0;
    QVERIFY(parseHotkeyChord("Ctrl+Alt+B", mods, vk));
    QCOMPARE(UINT(mods & MOD_CONTROL), UINT(MOD_CONTROL));
    QCOMPARE(UINT(mods & MOD_ALT), UINT(MOD_ALT));
    QCOMPARE(vk, WORD('B'));
}

void TestConfig::chordParsesNamedKey() {
    UINT mods = 0, vk = 0;
    QVERIFY(parseHotkeyChord("Ctrl+Shift+Escape", mods, vk));
    QCOMPARE(vk, WORD(VK_ESCAPE));

    QVERIFY(parseHotkeyChord("Win+PageUp", mods, vk));
    QCOMPARE(vk, WORD(VK_PRIOR));
    QCOMPARE(UINT(mods & MOD_WIN), UINT(MOD_WIN));
}

void TestConfig::chordParsesFunctionKey() {
    UINT mods = 0, vk = 0;
    QVERIFY(parseHotkeyChord("Alt+F5", mods, vk));
    QCOMPARE(vk, WORD(VK_F5));

    QVERIFY(parseHotkeyChord("f1", mods, vk));
    QCOMPARE(vk, WORD(VK_F1));
    // A chord with no modifier is accepted here; RegisterHotKey is what would
    // reject it, and that needs a real hotkey table.
}

void TestConfig::chordAcceptsModifierOrder() {
    UINT modsA = 0, vkA = 0, modsB = 0, vkB = 0;
    QVERIFY(parseHotkeyChord("Ctrl+Alt+R", modsA, vkA));
    QVERIFY(parseHotkeyChord("alt+ctrl+r", modsB, vkB));
    QCOMPARE(modsA, modsB);
    QCOMPARE(vkA, vkB);
}

void TestConfig::chordRejectsGarbage() {
    UINT mods = 0, vk = 0;
    QVERIFY(!parseHotkeyChord("", mods, vk));
    QVERIFY(!parseHotkeyChord("Ctrl+Alt", mods, vk));      // no key
    QVERIFY(!parseHotkeyChord("Hyper+R", mods, vk));      // unknown modifier
    QVERIFY(!parseHotkeyChord("F13", mods, vk));          // beyond F12
    QVERIFY(!parseHotkeyChord("F0", mods, vk));
    QVERIFY(!parseHotkeyChord("Ctrl+Fn+R", mods, vk));    // Fn is not a modifier
}

void TestConfig::chordAlwaysSetsNoRepeat() {
    UINT mods = 0, vk = 0;
    QVERIFY(parseHotkeyChord("Ctrl+Alt+R", mods, vk));
    // Without this, holding the key down fires reload on every autorepeat.
    QCOMPARE(UINT(mods & MOD_NOREPEAT), UINT(MOD_NOREPEAT));
}

QTEST_GUILESS_MAIN(TestConfig)
#include "test_config.moc"
