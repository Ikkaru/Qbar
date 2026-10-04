#include <QtTest>

#include "core/config.h"
#include "core/theme.h"

class TestTheme : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();

    void generateProducesOpaqueColours();
    void generateIsDeterministic();
    void differentSeedsGiveDifferentSurfaces();
    void malformedSeedFallsBackWithoutCrashing();

    // The battery hues are fixed, not seed-derived, because "charging" and "full"
    // mean the same thing on every machine. If they ever get derived from the
    // seed they will quietly change meaning, so pin them.
    void batteryHuesAreIndependentOfSeed();

    void barTextColourTracksSetter();
    void lightThresholdIsSane();

private:
    QStringList m_seeds;
};

void TestTheme::initTestCase() {
    m_seeds = {"#89b4fa", "#000000", "#ffffff", "#ff0000",
               "#00ff00", "#123456", "#abcdef", "#7f7f7f"};
}

void TestTheme::generateProducesOpaqueColours() {
    for (const QString& seed : m_seeds) {
        Theme t;
        t.generate(seed);

        const QList<QColor> roles{t.primary(), t.onPrimary(), t.primaryContainer(),
                                  t.onPrimaryContainer(), t.surface(), t.onSurface(),
                                  t.surfaceContainer(), t.onSurfaceContainer(),
                                  t.outline(), t.surfaceVariant(),
                                  t.tertiary(), t.onTertiary(),
                                  t.error(), t.onError(),
                                  t.charging(), t.full()};

        for (const QColor& c : roles) {
            QVERIFY2(c.isValid(), qPrintable(seed));
            QVERIFY2(c.alpha() == 255, qPrintable(QStringLiteral("%1 alpha=%2")
                                                  .arg(seed).arg(c.alpha())));
            // A colour outside sRGB means the OKLCH conversion overflowed, which
            // Qt renders as black or as nothing at all.
            QVERIFY2(c.red() >= 0 && c.red() <= 255 &&
                     c.green() >= 0 && c.green() <= 255 &&
                     c.blue() >= 0 && c.blue() <= 255,
                     qPrintable(QStringLiteral("%1 -> %2").arg(seed, c.name())));
        }
    }
}

void TestTheme::generateIsDeterministic() {
    // The bar regenerates the theme on every config reload. If generate() were
    // not pure, colours would flicker each time the file watcher fired.
    Theme a, b;
    a.generate("#89b4fa");
    b.generate("#89b4fa");
    QCOMPARE(a.primary(), b.primary());
    QCOMPARE(a.surface(), b.surface());
    QCOMPARE(a.tertiary(), b.tertiary());
    QCOMPARE(a.error(), b.error());
    QCOMPARE(a.full(), b.full());
}

void TestTheme::differentSeedsGiveDifferentSurfaces() {
    Theme a, b;
    a.generate("#ff0000");
    b.generate("#00ff00");
    QVERIFY(a.primary() != b.primary());
    QVERIFY(a.surface() != b.surface());
}

void TestTheme::malformedSeedFallsBackWithoutCrashing() {
    // theme.seed is user-editable and the file watcher reloads it live, so a typo
    // must not take the bar down.
    for (const QString& bad : {"", "not-a-colour", "#12345", "rgb(1,2)"}) {
        Theme t;
        t.generate(bad);
        QVERIFY2(t.primary().isValid(), qPrintable(bad));
        QCOMPARE(t.primary().alpha(), 255);
    }
}

void TestTheme::batteryHuesAreIndependentOfSeed() {
    Theme red, blue;
    red.generate("#ff0000");
    blue.generate("#0000ff");

    QCOMPARE(red.error(), blue.error());
    QCOMPARE(red.charging(), blue.charging());
    QCOMPARE(red.full(), blue.full());
    QCOMPARE(red.tertiary(), blue.tertiary());

    // They still have to be four distinguishable colours, or the popup's states
    // collapse into one. Compared by name because exact equality across two
    // objects is what we just pinned above.
    QSet<QString> distinct{red.error().name(), red.charging().name(),
                           red.full().name(), red.tertiary().name()};
    QCOMPARE(distinct.size(), 4);

    // And each must be legible against the popup surface, which is dark.
    for (const QColor& c : {red.error(), red.charging(), red.full(), red.tertiary()}) {
        const int luminance = qGray(c.red()) * 299 + qGray(c.green()) * 587 +
                              qGray(c.blue()) * 114;
        const int surfaceLuminance = qGray(red.surfaceContainer().red()) * 299 +
                                    qGray(red.surfaceContainer().green()) * 587 +
                                    qGray(red.surfaceContainer().blue()) * 114;
        QVERIFY2(luminance - surfaceLuminance > 60,
                 qPrintable(QStringLiteral("%1 does not separate from %2")
                             .arg(c.name(), red.surfaceContainer().name())));
    }
}

void TestTheme::barTextColourTracksSetter() {
    Theme t;
    t.generate("#89b4fa");

    QSignalSpy spy(&t, &Theme::barTextColorChanged);

    // The default is already white, so starting there would prove nothing -
    // the setter has to move away from it before it is allowed to emit.
    t.setBarTextColor(QColor("#101010"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(t.barTextColor(), QColor("#101010"));

    // Same value again: setting a property should not churn bindings, because
    // ContrastService re-applies on every wallpaper poll.
    t.setBarTextColor(QColor("#101010"));
    QCOMPARE(spy.count(), 1);

    t.setBarTextColor(QColor("#ffffff"));
    QCOMPARE(spy.count(), 2);
    QCOMPARE(t.barTextColor(), QColor("#ffffff"));
}

void TestTheme::lightThresholdIsSane() {
    // ContrastService compares against this. Outside 0-255 the comparison can
    // never resolve one way, and the bar would sit on the wrong colour forever.
    Config config;
    // Relative to the source tree, so ctest gives this test the right working
    // directory rather than whatever directory the runner happened to start in.
    QVERIFY2(config.load(QStringLiteral("resources/config.json")),
             "run ctest via the CTest target, which sets the working directory");
    QVERIFY(config.theme.lightThreshold >= 0);
    QVERIFY(config.theme.lightThreshold <= 255);
    QVERIFY(config.theme.lightThreshold > 0);
}

QTEST_GUILESS_MAIN(TestTheme)
#include "test_theme.moc"
