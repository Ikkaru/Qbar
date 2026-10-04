#include "graph_service.h"
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <QDebug>

#pragma comment(lib, "pdh.lib")

GraphService::GraphService(QObject* parent) : QObject(parent) {
    m_cpuCores = QThread::idealThreadCount();
    m_timer = new QTimer(this);
    m_timer->setInterval(m_intervalMs);
    connect(m_timer, &QTimer::timeout, this, &GraphService::poll);
    poll();
    m_timer->start();
}

void GraphService::poll() {
    sampleCpu();
    sampleRam();
    sampleNet();
    emit updated();
}

void GraphService::sampleCpu() {
    FILETIME idle, kernel, user;
    if (!GetSystemTimes(&idle, &kernel, &user)) return;

    const quint64 idleTicks = (quint64)idle.dwLowDateTime | ((quint64)idle.dwHighDateTime << 32);
    const quint64 kernelTicks = (quint64)kernel.dwLowDateTime | ((quint64)kernel.dwHighDateTime << 32);
    const quint64 userTicks = (quint64)user.dwLowDateTime | ((quint64)user.dwHighDateTime << 32);

    const quint64 total = kernelTicks + userTicks;

    if (m_havePrev) {
        const quint64 totalDelta = total - m_prevTotal;
        const quint64 idleDelta = idleTicks - m_prevIdle;
        if (totalDelta > 0) {
            m_cpu = 100.0 * (1.0 - static_cast<double>(idleDelta) / totalDelta);
        }
    }

    m_prevIdle = idleTicks;
    m_prevTotal = total;
    m_havePrev = true;

    m_cpuHistory.append(m_cpu);
    if (m_cpuHistory.size() > m_maxSamples) m_cpuHistory.removeFirst();
}

void GraphService::sampleRam() {
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    if (!GlobalMemoryStatusEx(&mem)) return;

    m_ram = 100.0 * (1.0 - static_cast<double>(mem.ullAvailPhys) / mem.ullTotalPhys);
    m_ramUsed = formatBytes(mem.ullTotalPhys - mem.ullAvailPhys);
    m_ramTotal = formatBytes(mem.ullTotalPhys);

    m_ramHistory.append(m_ram);
    if (m_ramHistory.size() > m_maxSamples) m_ramHistory.removeFirst();
}

void GraphService::sampleNet() {
    // PDH: total bytes/sec across all network interfaces, normalised against a
    // 1 Gbps link so the ring reads as a percentage of practical bandwidth.
    static PDH_HQUERY query = nullptr;
    static PDH_HCOUNTER counter = nullptr;
    static quint64 prevBytes = 0;
    static bool havePrev = false;

    if (!query) {
        PdhOpenQueryW(nullptr, 0, &query);
        PdhAddEnglishCounterW(query,
            L"\\Network Interface(*)\\Bytes Total/sec", 0, &counter);
    }

    PdhCollectQueryData(query);
    PDH_FMT_COUNTERVALUE val;
    if (PdhGetFormattedCounterValue(counter, PDH_FMT_LARGE, nullptr, &val) != ERROR_SUCCESS)
        return;

    const quint64 bytes = val.largeValue;
    if (havePrev && bytes >= prevBytes) {
        const double bps = static_cast<double>(bytes - prevBytes) * 8.0;
        m_net = qBound(0.0, bps / 1.0e9 * 100.0, 100.0);
        m_netUp = formatRate(bps / 2.0);
        m_netDown = formatRate(bps / 2.0);
    }
    prevBytes = bytes;
    havePrev = true;
}

QString GraphService::formatBytes(quint64 bytes) {
    const QStringList units = {"B", "KB", "MB", "GB", "TB"};
    double v = static_cast<double>(bytes);
    int i = 0;
    while (v >= 1024.0 && i < units.size() - 1) { v /= 1024.0; ++i; }
    return QString("%1 %2").arg(v, 0, 'f', i == 0 ? 0 : 1).arg(units[i]);
}

QString GraphService::formatRate(double bps) {
    const QStringList units = {"bps", "Kbps", "Mbps", "Gbps"};
    double v = bps;
    int i = 0;
    while (v >= 1000.0 && i < units.size() - 1) { v /= 1000.0; ++i; }
    return QString("%1 %2/s").arg(v, 0, 'f', i < 2 ? 0 : 1).arg(units[i]);
}
