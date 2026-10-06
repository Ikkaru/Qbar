#pragma once
#include <QMap>
#include <QObject>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QQuickItem>
#include <QRect>
#include <QTimer>
#include <windows.h>

class PopupHost : public QObject {
    Q_OBJECT
    Q_PROPERTY(QQuickItem* anchor READ anchor WRITE setAnchor NOTIFY anchorChanged)
    Q_PROPERTY(bool isOpen READ isOpenValue NOTIFY isOpenChanged)
    // URL of the popup currently being shown, empty when none. QML compares this
    // instead of isOpen to decide whether a hover should open its tooltip: "a
    // popup is showing" is not the same question as "my popup is showing", and
    // only the second one prevents a tooltip springing back up after its own
    // popup closed.
    Q_PROPERTY(QString currentUrl READ currentUrl NOTIFY isOpenChanged)
    // Whether the open popup is transient or interactive.
    //
    // A transient popup is a readout: it goes away as soon as the pointer leaves
    // the anchor. An interactive one contains controls the user has to reach -
    // the volume mixer needs the pointer to travel from the icon down into the
    // popup to grab a slider, and that journey crosses the icon's hover area, so
    // treating it like a readout closes it mid-gesture.
    Q_PROPERTY(bool interactive READ interactiveValue NOTIFY isOpenChanged)

public:
    explicit PopupHost(QObject* parent = nullptr);

    QQuickItem* anchor() const { return m_anchor; }
    void setAnchor(QQuickItem* item);
    QString currentUrl() const { return m_popupUrl; }
    bool interactiveValue() const { return m_interactive; }

    // interactive marks the popup as one the pointer has to be able to travel
    // into, so the anchor's onExited must not close it. Only the low-level press
    // hook can dismiss an interactive popup.
    Q_INVOKABLE void open(const QString& url, bool interactive = false);
    Q_INVOKABLE void close();
    // Close only if the popup is transient. QML's onExited calls this rather
    // than close(), so a single handler works for both kinds of popup.
    Q_INVOKABLE void closeIfTransient();
    // Destroys the popup without animating. Called by open() when replacing a
    // popup, and by the close timer once the close animation has had time to
    // finish. Keeping the timer here rather than in QML means a popup that is
    // replaced mid-close cannot have its stale timer destroy the new one.
    Q_INVOKABLE void finishClose();
    // Exposed through the isOpen property rather than as an invokable of the
    // same name: the property shadows it, and QML then resolves `isOpen()` to
    // the boolean and throws "not a function" at the call site.
    bool isOpenValue() const { return m_popup != nullptr; }

signals:
    void anchorChanged();
    void isOpenChanged();
    // Emitted instead of destroying immediately, so a popup can play its close
    // animation first. The popup calls finishClose() when it is done.
    void closeRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QRect rect() const;
    QRect anchorRect() const;
    // True when the point is over the popup or its anchor, i.e. a press there
    // should not dismiss the popup.
    bool containsGlobal(const QPoint& p) const;

    // A press on another application's window never reaches the qApp event
    // filter, so a popup only dismissed when the bar itself was clicked. A
    // low-level hook sees every press on the screen.
    void installHook();
    void removeHook();
    static LRESULT CALLBACK mouseProc(int nCode, WPARAM wParam, LPARAM lParam);

    QQuickItem* m_anchor = nullptr;
    QQuickWindow* m_popup = nullptr;
    QTimer m_closeTimer;
    QString m_popupUrl;
    bool m_closing = false;
    bool m_hook = false;
    bool m_interactive = false;
    // One component per popup URL, kept for the process lifetime. There are only
    // a handful of popups and each is small, so the flat cost is worth it against
    // rebuilding one on every open - see open() for why that leaked.
    //
    // Raw QObject* rather than a smart pointer or the component itself: the
    // PopupHost owns these and outlives every popup, and QQmlComponent is
    // neither copyable nor movable so it cannot be a container value type.
    QMap<QString, QQmlComponent*> m_components;
    static HHOOK s_hook;
    static PopupHost* s_instance;
};