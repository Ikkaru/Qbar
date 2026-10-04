#include "clock_service.h"
#include <QDebug>

ClockService::ClockService(QObject* parent) : QObject(parent) {
    m_timer = new QTimer(this);
    m_timer->setInterval(200);
    connect(m_timer, &QTimer::timeout, this, &ClockService::tick);

    // Align to next second boundary
    auto now = QDateTime::currentDateTime();
    int delay = 1000 - now.time().msec();
    QTimer::singleShot(delay, this, [this]() {
        tick();
        m_timer->start();
    });
}

void ClockService::tick() {
    auto now = QDateTime::currentDateTime();
    QString t = now.toString(m_format);
    QString d = now.toString("ddd MMM d");
    if (t != m_timeText) {
        m_timeText = t;
        m_dateText = d;
        emit timeChanged();
    }
}
