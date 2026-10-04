#pragma once
#include <QObject>
#include <QTimer>
#include <QJsonObject>

class IndicatorsService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QJsonObject state READ state NOTIFY stateChanged)

public:
    explicit IndicatorsService(QObject* parent = nullptr);

    QJsonObject state() const { return m_state; }

public slots:
    void poll();

signals:
    void stateChanged();

private:
    void pollBattery();
    // Economy mode, via WinRT PowerManager::EnergySaverStatus.
    static bool batterySaverOn();
    // Adds secondsLeft / secondsToFull, or -1 for each, to a battery state object.
    static void addTimeEstimates(QJsonObject& battery);
    void pollNetwork();
    void pollVolume();

    QTimer* m_timer = nullptr;
    QJsonObject m_state;
    int m_intervalMs = 2000;
};
