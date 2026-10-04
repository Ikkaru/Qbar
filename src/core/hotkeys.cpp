#include "hotkeys.h"
// QCoreApplication, not QApplication: the only thing needed from the app object
// is installNativeEventFilter, which both provide, and using the lighter one
// keeps the tests off QtWidgets.
#include <QCoreApplication>
#include <QMap>

namespace {

// Named keys that are not single characters. Values are the lowercase spellings
// accepted in a chord.
const QMap<WORD, QString> namedKeys = {
    {VK_ESCAPE, "escape"}, {VK_SPACE, "space"}, {VK_TAB, "tab"},
    {VK_RETURN, "return"}, {VK_BACK, "backspace"}, {VK_DELETE, "delete"},
    {VK_INSERT, "insert"}, {VK_HOME, "home"}, {VK_END, "end"},
    {VK_PRIOR, "pageup"}, {VK_NEXT, "pagedown"},
    {VK_UP, "up"}, {VK_DOWN, "down"}, {VK_LEFT, "left"}, {VK_RIGHT, "right"},
    {VK_PRINT, "print"}, {VK_PAUSE, "pause"}, {VK_CAPITAL, "capslock"},
    {VK_NUMLOCK, "numlock"}, {VK_SCROLL, "scrolllock"},
    {VK_SNAPSHOT, "printscreen"}, {VK_APPS, "apps"},
};

} // namespace

// "Ctrl+Alt+R" -> MOD_CONTROL|MOD_ALT|MOD_NOREPEAT, 'R'. The key part is
// either a single character or a named key; modifiers may appear in any order.
bool parseHotkeyChord(const QString& chord, UINT& mods, UINT& vk) {
    const QStringList parts = chord.split('+', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return false;

    mods = MOD_NOREPEAT;
    for (int i = 0; i < parts.size() - 1; ++i) {
        const QString m = parts[i].trimmed().toLower();
        if (m == "ctrl" || m == "control") mods |= MOD_CONTROL;
        else if (m == "alt") mods |= MOD_ALT;
        else if (m == "shift") mods |= MOD_SHIFT;
        else if (m == "win" || m == "meta") mods |= MOD_WIN;
        else return false;
    }

    const QString key = parts.last().trimmed();
    if (key.size() == 1) {
        const SHORT v = VkKeyScanA(key.at(0).toLatin1());
        if (v == -1) return false;
        vk = v & 0xFF;
        return true;
    }

    const QString lower = key.toLower();
    for (auto it = namedKeys.constBegin(); it != namedKeys.constEnd(); ++it) {
        if (it.value() == lower) { vk = it.key(); return true; }
    }

    if (lower.startsWith('f')) {
        bool ok = false;
        const int n = lower.mid(1).toInt(&ok);
        if (ok && n >= 1 && n <= 12) { vk = VK_F1 + n - 1; return true; }
    }
    return false;
}

Hotkeys::Hotkeys(QObject* parent) : QObject(parent) {
    qApp->installNativeEventFilter(this);
}

Hotkeys::~Hotkeys() {
    for (const auto& e : m_entries)
        UnregisterHotKey(nullptr, e.id);
    qApp->removeNativeEventFilter(this);
}

bool Hotkeys::registerHotkey(const QString& action, const QString& chord) {
    UINT mods = 0, vk = 0;
    if (!parseHotkeyChord(chord, mods, vk)) return false;

    const int id = m_nextId++;
    if (!RegisterHotKey(nullptr, id, mods, vk)) return false;

    m_entries.append({id, action});
    return true;
}

bool Hotkeys::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) {
    if (eventType == "windows_generic_MSG") {
        auto* msg = static_cast<MSG*>(message);
        if (msg->message == WM_HOTKEY) {
            for (const auto& e : m_entries) {
                if (e.id == static_cast<int>(msg->wParam)) {
                    emit triggered(e.action);
                    return true;
                }
            }
        }
    }
    return false;
}