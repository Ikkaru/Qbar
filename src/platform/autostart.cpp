#include "autostart.h"
#include <windows.h>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>

namespace {

// The per-user Run key. HKCU rather than HKLM on purpose: writing to HKLM needs
// elevation, and this app has no business asking for that just to start itself.
constexpr wchar_t kRunKey[] = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kApprovalKey[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
constexpr wchar_t kValueName[] = L"qbar";

// First byte of the StartupApproved blob: 02 = enabled, 03 = disabled.
constexpr unsigned char kApprovedEnabled = 0x02;
constexpr unsigned char kApprovedDisabled = 0x03;

QString registryPath(const wchar_t* key) {
    return QStringLiteral("HKEY_CURRENT_USER\\") + QString::fromWCharArray(key);
}

// Reads a REG_SZ or REG_BINARY value. Returns nullopt-equivalent (empty QByteArray)
// when the value is absent, which is the normal state for a fresh entry.
QByteArray readValue(HKEY root, const wchar_t* subkey, const wchar_t* name) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, subkey, 0, KEY_READ, &key) != ERROR_SUCCESS)
        return {};

    DWORD type = 0, size = 0;
    QByteArray out;
    if (RegQueryValueExW(key, name, nullptr, &type, nullptr, &size) == ERROR_SUCCESS
        && size > 0) {
        QByteArray buf(static_cast<qsizetype>(size), Qt::Uninitialized);
        if (RegQueryValueExW(key, name, nullptr, &type,
                             reinterpret_cast<LPBYTE>(buf.data()), &size) == ERROR_SUCCESS) {
            buf.truncate(static_cast<qsizetype>(size));
            // A REG_SZ read back as bytes carries a UTF-16 NUL terminator; drop it
            // so callers compare against a plain command line.
            if (type == REG_SZ)
                buf.chop(2);
            out = buf;
        }
    }
    RegCloseKey(key);
    return out;
}

bool deleteValue(HKEY root, const wchar_t* subkey, const wchar_t* name) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, subkey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
        return false;
    const LONG rc = RegDeleteValueW(key, name);
    RegCloseKey(key);
    return rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND;
}

} // namespace

bool Autostart::approvalAllowsStartup(const QByteArray& approval) {
    // No blob means Windows has recorded no opinion, and an entry nobody has
    // objected to runs. Also covers the single-entry case: a zero-length value
    // carries no state byte.
    if (approval.isEmpty())
        return true;

    // Only an explicit "disabled" blocks. Windows has used a few encodings for
    // this blob over the years, and an unrecognised state should leave the bar
    // able to start rather than permanently stuck.
    return static_cast<unsigned char>(approval.at(0)) != kApprovedDisabled;
}

Autostart::Autostart(QObject* parent) : QObject(parent) {}

QByteArray Autostart::readApproval() {
    return readValue(HKEY_CURRENT_USER, kApprovalKey, kValueName);
}

bool Autostart::clearApproval() {
    // Deleting rather than writing {02}. Absence means "no opinion recorded",
    // which is the honest state for an entry this app just created, and it is
    // what makes Task Manager show Enabled.
    return deleteValue(HKEY_CURRENT_USER, kApprovalKey, kValueName);
}

bool Autostart::registered() const {
    return !readValue(HKEY_CURRENT_USER, kRunKey, kValueName).isEmpty();
}

bool Autostart::enabled() const {
    if (!registered())
        return false;
    return approvalAllowsStartup(readApproval());
}

QString Autostart::command() const {
    const QByteArray raw = readValue(HKEY_CURRENT_USER, kRunKey, kValueName);
    if (raw.isEmpty())
        return {};
    return QString::fromUtf16(reinterpret_cast<const char16_t*>(raw.constData()),
                              raw.size() / 2);
}

bool Autostart::writeRunValue(const QString& command) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    const std::wstring w = command.toStdWString();
    const LONG rc = RegSetValueExW(
        key, kValueName, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(w.c_str()),
        static_cast<DWORD>((w.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return rc == ERROR_SUCCESS;
}

bool Autostart::clearRunValue() {
    // Delete, not blank. A blank value would still appear in Task Manager as a
    // disabled entry, which is noise rather than a clean removal.
    return deleteValue(HKEY_CURRENT_USER, kRunKey, kValueName);
}

void Autostart::setEnabled(bool on) {
    const bool before = enabled();

    if (on) {
        // Quote the path: autostart runs with the system directory as CWD, and an
        // unquoted path breaks outright the moment the install location contains a
        // space. "C:\Program Files\..." is the normal case.
        const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        writeRunValue(QStringLiteral("\"%1\"").arg(exe));

        // Must clear the consent blob as well. Writing the command is not enough:
        // once Task Manager has recorded the entry as disabled, the shell keeps
        // skipping it, so without this the caller would get a registry that looks
        // right and a bar that still never starts at logon.
        clearApproval();
    } else {
        clearRunValue();
        clearApproval();
    }

    if (enabled() != before)
        emit enabledChanged();
}

void Autostart::sync(bool want) {
    const bool present = registered();

    if (want && !present) {
        setEnabled(true);
    } else if (!want && present) {
        setEnabled(false);
    } else {
        // Nothing to reconcile. Deliberately includes `want && present && !approved`:
        // the user disabled this in Task Manager, and the next bar launch is not
        // the place to quietly take that back. Task Manager's Enable button is the
        // way back, which is the control surface the user asked for.
        return;
    }

    if (enabled() != want) {
        // Worth a log line: this fails on a locked-down machine where HKCU is
        // redirected by policy, and the user would otherwise have no idea why the
        // bar is not starting.
        qWarning() << "Autostart: could not" << (want ? "register" : "remove")
                   << "HKCU Run value under" << registryPath(kRunKey);
    }
}
