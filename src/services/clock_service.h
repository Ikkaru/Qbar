#pragma once
#include <QObject>
#include <QTimer>
#include <QDateTime>

class ClockService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString timeText READ timeText NOTIFY timeChanged)
    Q_PROPERTY(QString dateText READ dateText NOTIFY timeChanged)

public:
    explicit ClockService(QObject* parent = nullptr);
    void setFormat(const QString& fmt) { m_format = fmt; tick(); }

    QString timeText() const { return m_timeText; }
    QString dateText() const { return m_dateText; }

public slots:
    void tick();

signals:
    void timeChanged();

private:
    QTimer* m_timer = nullptr;
    QString m_format = "HH:mm";
    QString m_timeText;
    QString m_dateText;
};
