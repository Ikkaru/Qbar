# Adding a module

A module is a pair: a **C++ service** that talks to Windows, and a **QML file**
that draws it. The service is a plain `QObject` with no GUI dependency; the QML
is a `Rectangle` that reads properties off the service.

The split is the whole point. Anything that can be tested without a window lives
in the service, and the QML left over is presentation.

A worked example follows, for a module that reports CPU temperature.

---

## 1. The service

`src/services/tempservice.h`

```cpp
#pragma once
#include <QObject>
#include <QTimer>

class TempService : public QObject {
    Q_OBJECT
    // Every property needs NOTIFY. Without it, a QML binding reads the value
    // once and then silently keeps the stale one forever.
    Q_PROPERTY(double celsius READ celsius NOTIFY readingChanged)

public:
    explicit TempService(QObject* parent = nullptr);

    double celsius() const { return m_celsius; }

signals:
    void readingChanged();

private slots:
    void poll();

private:
    QTimer* m_timer = nullptr;
    double m_celsius = 0.0;
};
```

`src/services/tempservice.cpp`

```cpp
#include "tempservice.h"

TempService::TempService(QObject* parent) : QObject(parent) {
    m_timer = new QTimer(this);
    m_timer->setInterval(2000);
    connect(m_timer, &QTimer::timeout, this, &TempService::poll);
    poll();              // prime it, so the first paint is not 0
    m_timer->start();
}

void TempService::poll() {
    m_celsius = readFromWhateverWindowsProvides();
    emit readingChanged();
}
```

Two rules that are not negotiable:

**Prime the first value in the constructor.** Calling `poll()` before starting
the timer means the bar never shows a placeholder. This bit the `theme` wiring in
`main.cpp`, where a setter ran before its signal was connected and the first
value was lost.

**Choose the interval from the source, not from taste.** A sensor that changes
over minutes polled every 500 ms is pure CPU for a number nobody can perceive
change in. Put it in `config.json` if the user should be able to change it.

---

## 2. Register the service in `main.cpp`

Three places, and missing any one of them fails quietly:

```cpp
#include "services/tempservice.h"

int main(int argc, char* argv[]) {
    ...
    TempService temps;                       // 1. construct

    engine.rootContext()->setContextProperty(
        "tempService", &temps);              // 2. expose to QML

    barManager.createWindow();
    engine.rootContext()->setContextProperty("barWindow", barManager.window());
    barManager.setupWindow(engine.rootContext());   // 3. AFTER the context property

    return app.exec();
}
```

**Step 3 has an ordering trap worth knowing about.** `barWindow` must be
registered *before* `setupWindow()` parses the QML. A binding evaluated while the
context property is still `undefined` latches that `undefined` permanently, and
`setContextProperty` afterwards does not notify bindings that already ran. The
symptom is a property that is wired up correctly and silently does nothing. This
cost a long debugging session once; the fix was splitting `BarManager::createWindow()`
out of `setupWindow()` so the window exists before the property is registered.

---

## 3. The QML

`src/qml/modules/Temp.qml`

```qml
import QtQuick

Rectangle {
    id: temp
    width: 60
    height: parent.height
    color: "transparent"

    property color textColor: theme.barTextColor

    Text {
        anchors.centerIn: parent
        text: Math.round(tempService.celsius) + "°C"
        color: temp.textColor
        Behavior on color { ColorAnimation { duration: 300 } }
        font.family: theme.font
        font.pixelSize: 13
    }
}
```

**The module root's width must match its content.** `Bar.qml` drops each module
into a centring `Row`, so a root wider than its content does not show as extra
content, it shows as an oversized gap on *both* sides. If the content is
dynamic, bind the width to it.

**Never hardcode icon metrics.** Use the shared numbers every other module uses,
so glyphs from the same font land on the same baseline:

| Property | Value | Why |
|---|---|---|
| `iconCell` | 22 | Cell height. Glyphs sit on the font baseline, and a baseline only lands at a predictable y when cell height, pixel size and vertical alignment all agree. |
| `iconSize` | 19 | Pixel size for icons. |

Declare them as `readonly property int` on the module root, matching
`Network.qml`.

---

## 4. Register the QML in **two** places

This is where mistakes are most expensive, because nothing fails at build time.

**`resources/resources.qrc`**

```xml
<file alias="qml/modules/Temp.qml">../src/qml/modules/Temp.qml</file>
```

**`src/core/module_registry.cpp`**

```cpp
static const QMap<QString, QString> s_moduleQml = {
    ...
    {"temp", "qrc:/qml/modules/Temp.qml"},
};
```

Miss the first and the Loader gets an empty source. Miss the second and the id
is filtered out of the layout as if it did not exist.

**The registry is the only thing between `config.json` and a Loader pointed at
a URL that resolves to nothing.** Four ids shipped in the registry with no QML
behind them at one point — `media`, `tray`, `notifications` and `volume` — which
meant a crash the moment anyone added one to `layout`. `test_module_registry`
now checks that every registered id exists inside the qrc bundle.

---

## 5. Enable it

`resources/config.json`

```json
{
  "modules": {
    "temp": { "enabled": true, "intervalMs": 2000 }
  },
  "layout": {
    "left": ["clock", "temp"],
    "center": [],
    "right": ["indicators", "network", "clock"]
  }
}
```

`layout.left` / `center` / `right` decide both membership and order. A module
does not need to be in the layout to be enabled, and one that is enabled but not
listed never appears. There is no separate `visible` flag, on purpose.

---

## 6. Test the service

Anything in the service that does not touch a window belongs in a test. See
`tests/README.md` for the pattern.

---

## Adding a popup instead

If the module opens a surface rather than sitting in the bar:

1. New `Window` root in `src/qml/popups/TempPopup.qml`, registered in
   `resources.qrc`.
2. Same `flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint` and
   `color: "transparent"` as the other popups. Without `FramelessWindowHint` it
   comes up as an ordinary decorated window with a title bar.
3. Anchor it to the item that opened it:

```qml
popupHost.anchor = someItem;
popupHost.open("qrc:/qml/popups/TempPopup.qml");
```

**The popup cannot be drawn in the bar's own QML tree.** The bar window is
exactly `bar.height` tall, so anything below the icon lands outside the window
rect and is clipped away. This is why the edge shadow is drawn inside the bar
too. A tooltip needs its own window.

If the popup animates, use the `closeRequested` signal and let `PopupHost`'s
timer own the destruction:

```qml
Connections {
    target: popupHost
    function onCloseRequested() {
        fadeOut.start();
        scaleOut.start();
    }
}
```

Do not destroy the popup from QML. The timer in `PopupHost` is what stops a
popup that was replaced mid-close from destroying its replacement.

---

## Traps worth knowing about

- **`Rectangle` `gradient` replaces `color` entirely.** A near-transparent
  gradient silently erases the fill. Use `Qt.rgba(...)` alpha instead.
- **A QML property on a C++ object that does not exist** fails as a blank label,
  not a crash. If a `Text` is empty, check the C++ side first.
- **Do not let a model refresh destroy a delegate mid-drag.** A popup whose
  session list reloads under an active slider drag drops the mouse grab. Pause
  the refresh while a handle is pressed.
- **A popover that hides itself must hide itself everywhere.** `PopupHost`
  dismisses on a press anywhere on screen, including outside the bar, via a
  low-level mouse hook. An app-level event filter only ever sees presses that
  land on our own windows, which is why clicks elsewhere used to leave popups
  open.