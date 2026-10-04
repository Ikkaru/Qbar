#pragma once
#include <QObject>
#include <QQuickWindow>
#include <QQuickItem>
#include <QRect>
#include <QTimer>
#include <windows.h>

class PopupHost : public QObject {
    Q_OBJECT
    Q_PROPERTY(QQuickItem* anchor READ anchor WRITE setAnchor NOTIFY anchorChanged)
    Q_PROPERTY(bool isOpen READ isOpenValue NOTIFY isOpenChanged)

public:
    explicit PopupHost(QObject* parent = nullptr);

    QQuickItem* anchor() const { return m_anchor; }
    void setAnchor(QQuickItem* item);

    Q_INVOKABLE void open(const QString& url);
    Q_INVOKABLE void close();
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
    static HHOOK s_hook;
    static PopupHost* s_instance;
};