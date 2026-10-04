#pragma once
#include <QObject>
#include <QTimer>
#include <QVector>
#include <QString>
#include <QThread>

class GraphService : public QObject {
    Q_OBJECT
    Q_PROPERTY(double cpu READ cpu NOTIFY updated)
    Q_PROPERTY(double ram READ ram NOTIFY updated)
    Q_PROPERTY(double net READ net NOTIFY updated)
    Q_PROPERTY(int cpuCores READ cpuCores NOTIFY updated)
    Q_PROPERTY(QString ramUsed READ ramUsed NOTIFY updated)
    Q_PROPERTY(QString ramTotal READ ramTotal NOTIFY updated)
    Q_PROPERTY(QString netUp READ netUp NOTIFY updated)
    Q_PROPERTY(QString netDown READ netDown NOTIFY updated)
    Q_PROPERTY(QVector<double> cpuHistory READ cpuHistory NOTIFY updated)
    Q_PROPERTY(QVector<double> ramHistory READ ramHistory NOTIFY updated)

public:
    explicit GraphService(QObject* parent = nullptr);

    double cpu() const { return m_cpu; }
    double ram() const { return m_ram; }
    double net() const { return m_net; }
    int cpuCores() const { return m_cpuCores; }
    QString ramUsed() const { return m_ramUsed; }
    QString ramTotal() const { return m_ramTotal; }
    QString netUp() const { return m_netUp; }
    QString netDown() const { return m_netDown; }
    QVector<double> cpuHistory() const { return m_cpuHistory; }
    QVector<double> ramHistory() const { return m_ramHistory; }

public slots:
    void poll();

signals:
    void updated();

private:
    void sampleCpu();
    void sampleRam();
    void sampleNet();

    QTimer* m_timer = nullptr;
    double m_cpu = 0.0;
    double m_ram = 0.0;
    double m_net = 0.0;
    int m_cpuCores = 0;
    QString m_ramUsed;
    QString m_ramTotal;
    QString m_netUp;
    QString m_netDown;
    QVector<double> m_cpuHistory;
    QVector<double> m_ramHistory;
    int m_intervalMs = 1000;
    int m_maxSamples = 60;

    // CPU deltas
    quint64 m_prevIdle = 0;
    quint64 m_prevTotal = 0;
    bool m_havePrev = false;

    static QString formatBytes(quint64 bytes);
    static QString formatRate(double bps);
};
