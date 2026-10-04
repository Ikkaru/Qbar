#pragma once
#include <QObject>
#include <QStringList>
#include <QJsonArray>
#include <QTimer>

class VolumeService : public QObject {
    Q_OBJECT
    Q_PROPERTY(double masterLevel READ masterLevel NOTIFY masterChanged)
    Q_PROPERTY(bool masterMuted READ masterMuted NOTIFY masterChanged)
    Q_PROPERTY(QJsonArray sessions READ sessions NOTIFY sessionsChanged)

public:
    explicit VolumeService(QObject* parent = nullptr);

    double masterLevel() const { return m_masterLevel; }
    bool masterMuted() const { return m_masterMuted; }
    QJsonArray sessions() const { return m_sessions; }

    // While a slider is being dragged the model must not be rebuilt, or the
    // Repeater destroys the delegate that owns it and the mouse grab dies.
    Q_INVOKABLE void setPaused(bool paused);

    Q_INVOKABLE void setMasterLevel(double level);
    Q_INVOKABLE void setMasterMuted(bool muted);
    Q_INVOKABLE void setSessionLevel(const QString& pid, double level);
    Q_INVOKABLE void setSessionMuted(const QString& pid, bool muted);
    Q_INVOKABLE void refresh();

signals:
    void masterChanged();
    void sessionsChanged();

private:
    void pollMaster();
    void pollSessions();

    QTimer* m_timer = nullptr;
    bool m_paused = false;
    double m_masterLevel = 0.0;
    bool m_masterMuted = false;
    QJsonArray m_sessions;
};