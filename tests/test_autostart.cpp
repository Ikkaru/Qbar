#include <QtTest>

#include "platform/autostart.h"

// This deliberately tests only approvalAllowsStartup, the one piece of Autostart
// with interesting logic. Everything else in the class is a thin registry call,
// and exercising it for real would mean writing to the user's actual HKCU Run key
// - a test suite that silently reconfigures the machine it runs on is worse than
// no test at all.
class TestAutostart : public QObject {
    Q_OBJECT

private slots:
    // The bug this exists for: Task Manager disabling an entry does not delete the
    // Run value, it writes a consent blob next to it. Reading only the Run key
    // reported the entry as enabled, so a config saying autostart: true looked
    // satisfied while the shell was still skipping the entry. The bar then never
    // started at logon and nothing said why.
    void disabledBlobBlocksStartup();
    void enabledBlobAllowsStartup();

    // A fresh entry has no blob at all. Treating "absent" as disabled would mean
    // the very first registration never takes effect.
    void absentBlobAllowsStartup();

    // Empty blob, and states Windows is not documented to use. Both must allow, so
    // an encoding we do not recognise cannot leave the bar permanently unable to
    // start itself.
    void emptyBlobAllowsStartup();
    void unknownStateAllowsStartup();
};

void TestAutostart::disabledBlobBlocksStartup() {
    // Exactly the 12 bytes Task Manager writes: state byte 03 then padding.
    const QByteArray blob = QByteArray::fromHex("030000000000000000000000");
    QCOMPARE(blob.size(), 12);
    QVERIFY(!Autostart::approvalAllowsStartup(blob));
}

void TestAutostart::enabledBlobAllowsStartup() {
    const QByteArray blob = QByteArray::fromHex("020000000000000000000000");
    QCOMPARE(blob.size(), 12);
    QVERIFY(Autostart::approvalAllowsStartup(blob));
}

void TestAutostart::absentBlobAllowsStartup() {
    QVERIFY(Autostart::approvalAllowsStartup({}));
}

void TestAutostart::emptyBlobAllowsStartup() {
    QVERIFY(Autostart::approvalAllowsStartup(QByteArray("")));
}

void TestAutostart::unknownStateAllowsStartup() {
    // Anything that is not an explicit 03 is treated as permitted. Windows has
    // changed this encoding before; an unrecognised value should not silently
    // disable the bar forever.
    for (unsigned char state : {0x00, 0x01, 0x04, 0x06, 0xff}) {
        QByteArray blob(12, '\0');
        blob[0] = static_cast<char>(state);
        QVERIFY2(Autostart::approvalAllowsStartup(blob),
                 qPrintable(QStringLiteral("state %1 should not block startup")
                                .arg(state)));
    }
}

QTEST_GUILESS_MAIN(TestAutostart)
#include "test_autostart.moc"
