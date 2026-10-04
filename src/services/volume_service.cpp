#include "volume_service.h"
#include <QJsonObject>
#include <QDebug>
#include <QHash>
#include <QSet>
#include <QIcon>
#include <QPixmap>
#include <QBuffer>
#include <QFileIconProvider>
#include <QFileInfo>
#include <windows.h>
#include <shellapi.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audiopolicy.h>
#include <psapi.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "mmdevapi.lib")
#pragma comment(lib, "psapi.lib")

namespace {

// Cache extracted app icons per PID. Extracting on every 1.5s poll would be
// wasteful, and the same handful of apps keep making noise.
QHash<DWORD, QString> g_iconCache;

QString appIconDataUri(DWORD pid) {
    auto it = g_iconCache.constFind(pid);
    if (it != g_iconCache.constEnd())
        return it.value();

    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return QString();
    wchar_t path[MAX_PATH];
    DWORD size = MAX_PATH;
    if (!QueryFullProcessImageNameW(h, 0, path, &size)) {
        CloseHandle(h);
        return QString();
    }
    CloseHandle(h);

    // QFileIconProvider is the API that actually extracts the embedded resource
    // icon from an executable; QIcon(path) does not.
    static QFileIconProvider iconProvider;
    const QIcon exeIcon = iconProvider.icon(QFileInfo(QString::fromWCharArray(path)));
    const QPixmap pm = exeIcon.pixmap(32, 32);
    if (pm.isNull())
        return QString();

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    pm.toImage().save(&buffer, "PNG");
    const QString uri = QStringLiteral("data:image/png;base64,")
                        + QString::fromLatin1(bytes.toBase64());
    g_iconCache.insert(pid, uri);
    return uri;
}

IMMDeviceEnumerator* createEnumerator() {
    IMMDeviceEnumerator* enumerator = nullptr;
    CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                     IID_PPV_ARGS(&enumerator));
    return enumerator;
}

IAudioEndpointVolume* getDefaultRenderVolume(IMMDeviceEnumerator* enumerator) {
    if (!enumerator) return nullptr;
    IMMDevice* device = nullptr;
    IAudioEndpointVolume* vol = nullptr;
    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device))) {
        device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                         reinterpret_cast<void**>(&vol));
        device->Release();
    }
    return vol;
}

QString processNameFromPid(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return QStringLiteral("unknown");
    wchar_t path[MAX_PATH];
    DWORD size = MAX_PATH;
    QString name;
    if (QueryFullProcessImageNameW(h, 0, path, &size)) {
        const QString full = QString::fromWCharArray(path);
        name = full.mid(full.lastIndexOf('\\') + 1);
        name = name.mid(0, name.lastIndexOf('.'));
    }
    CloseHandle(h);
    return name;
}

} // namespace

VolumeService::VolumeService(QObject* parent) : QObject(parent) {
    m_timer = new QTimer(this);
    m_timer->setInterval(1500);
    connect(m_timer, &QTimer::timeout, this, &VolumeService::refresh);
    refresh();
    m_timer->start();
}

void VolumeService::setPaused(bool paused) {
    if (m_paused == paused) return;
    m_paused = paused;
    if (paused) {
        m_timer->stop();
    } else {
        refresh();
        m_timer->start();
    }
}

void VolumeService::setMasterLevel(double level) {
    IMMDeviceEnumerator* enumerator = createEnumerator();
    IAudioEndpointVolume* vol = getDefaultRenderVolume(enumerator);
    if (vol) {
        vol->SetMasterVolumeLevelScalar(static_cast<float>(qBound(0.0, level, 1.0)), nullptr);
        vol->Release();
    }
    if (enumerator) enumerator->Release();
    pollMaster();
    emit masterChanged();
}

void VolumeService::setMasterMuted(bool muted) {
    IMMDeviceEnumerator* enumerator = createEnumerator();
    IAudioEndpointVolume* vol = getDefaultRenderVolume(enumerator);
    if (vol) {
        vol->SetMute(muted ? TRUE : FALSE, nullptr);
        vol->Release();
    }
    if (enumerator) enumerator->Release();
    pollMaster();
    emit masterChanged();
}

void VolumeService::setSessionLevel(const QString& pid, double level) {
    bool ok = false;
    const DWORD target = pid.toULong(&ok);
    if (!ok) return;

    IMMDeviceEnumerator* enumerator = createEnumerator();
    IMMDevice* device = nullptr;
    IAudioSessionManager2* mgr = nullptr;
    if (enumerator &&
        SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device)) &&
        SUCCEEDED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr,
                                   reinterpret_cast<void**>(&mgr)))) {
        IAudioSessionEnumerator* enumerator = nullptr;
        if (SUCCEEDED(mgr->GetSessionEnumerator(&enumerator))) {
            int count = 0;
            enumerator->GetCount(&count);
            for (int i = 0; i < count; ++i) {
                IAudioSessionControl* control = nullptr;
                if (FAILED(enumerator->GetSession(i, &control))) continue;
                IAudioSessionControl2* c2 = nullptr;
                if (SUCCEEDED(control->QueryInterface(__uuidof(IAudioSessionControl2),
                                                      reinterpret_cast<void**>(&c2)))) {
                    DWORD p = 0;
                    c2->GetProcessId(&p);
                    if (p == target) {
                        ISimpleAudioVolume* sav = nullptr;
                        if (SUCCEEDED(c2->QueryInterface(__uuidof(ISimpleAudioVolume),
                                                         reinterpret_cast<void**>(&sav)))) {
                            sav->SetMasterVolume(
                                static_cast<float>(qBound(0.0, level, 1.0)), nullptr);
                            sav->Release();
                        }
                    }
                    c2->Release();
                }
                control->Release();
            }
            enumerator->Release();
        }
        mgr->Release();
    }
    if (device) device->Release();
    if (enumerator) enumerator->Release();
    // Deliberately no pollSessions()/sessionsChanged(): see setSessionMuted.
}

void VolumeService::setSessionMuted(const QString& pid, bool muted) {
    bool ok = false;
    const DWORD target = pid.toULong(&ok);
    if (!ok) return;

    IMMDeviceEnumerator* enumerator = createEnumerator();
    IMMDevice* device = nullptr;
    IAudioSessionManager2* mgr = nullptr;
    if (enumerator &&
        SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device)) &&
        SUCCEEDED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr,
                                   reinterpret_cast<void**>(&mgr)))) {
        IAudioSessionEnumerator* enumerator = nullptr;
        if (SUCCEEDED(mgr->GetSessionEnumerator(&enumerator))) {
            int count = 0;
            enumerator->GetCount(&count);
            for (int i = 0; i < count; ++i) {
                IAudioSessionControl* control = nullptr;
                if (FAILED(enumerator->GetSession(i, &control))) continue;
                IAudioSessionControl2* c2 = nullptr;
                if (SUCCEEDED(control->QueryInterface(__uuidof(IAudioSessionControl2),
                                                      reinterpret_cast<void**>(&c2)))) {
                    DWORD p = 0;
                    c2->GetProcessId(&p);
                    if (p == target) {
                        ISimpleAudioVolume* sav = nullptr;
                        if (SUCCEEDED(c2->QueryInterface(__uuidof(ISimpleAudioVolume),
                                                         reinterpret_cast<void**>(&sav)))) {
                            sav->SetMute(muted ? TRUE : FALSE, nullptr);
                            sav->Release();
                        }
                    }
                    c2->Release();
                }
                control->Release();
            }
            enumerator->Release();
        }
        mgr->Release();
    }
    if (device) device->Release();
    if (enumerator) enumerator->Release();
    // Deliberately no pollSessions()/sessionsChanged() here: emitting rebuilds
    // the model, which destroys the Repeater delegate that owns the slider
    // being dragged and kills the mouse grab mid-drag. The 1.5 s poll in
    // main.cpp refreshes the list soon enough.
    emit masterChanged();
}

void VolumeService::refresh() {
    pollMaster();
    pollSessions();
    emit masterChanged();
    emit sessionsChanged();
}

void VolumeService::pollMaster() {
    IMMDeviceEnumerator* enumerator = createEnumerator();
    IAudioEndpointVolume* vol = getDefaultRenderVolume(enumerator);
    if (vol) {
        float level = 0.0f;
        BOOL muted = FALSE;
        vol->GetMasterVolumeLevelScalar(&level);
        vol->GetMute(&muted);
        m_masterLevel = level;
        m_masterMuted = (muted == TRUE);
        vol->Release();
    }
    if (enumerator) enumerator->Release();
}

void VolumeService::pollSessions() {
    IAudioSessionManager2* mgr = nullptr;
    IMMDeviceEnumerator* mmDevEnumerator = createEnumerator();
    IMMDevice* device = nullptr;
    if (!mmDevEnumerator) return;

    if (FAILED(mmDevEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device)) ||
        FAILED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr,
                                reinterpret_cast<void**>(&mgr)))) {
        if (device) device->Release();
        mmDevEnumerator->Release();
        return;
    }

    QJsonArray arr;
    IAudioSessionEnumerator* sessionEnumerator = nullptr;
    if (SUCCEEDED(mgr->GetSessionEnumerator(&sessionEnumerator))) {
        int count = 0;
        sessionEnumerator->GetCount(&count);
        for (int i = 0; i < count; ++i) {
            IAudioSessionControl* control = nullptr;
            if (FAILED(sessionEnumerator->GetSession(i, &control))) continue;
            IAudioSessionControl2* c2 = nullptr;
            if (SUCCEEDED(control->QueryInterface(__uuidof(IAudioSessionControl2),
                                                  reinterpret_cast<void**>(&c2)))) {
                DWORD p = 0;
                c2->GetProcessId(&p);
                ISimpleAudioVolume* sav = nullptr;
                if (SUCCEEDED(c2->QueryInterface(__uuidof(ISimpleAudioVolume),
                                                 reinterpret_cast<void**>(&sav)))) {
                    float level = 0.0f;
                    BOOL muted = FALSE;
                    sav->GetMasterVolume(&level);
                    sav->GetMute(&muted);
                    arr.append(QJsonObject{
                        {"pid", QString::number(p)},
                        {"name", processNameFromPid(p)},
                        {"icon", appIconDataUri(p)},
                        {"level", level},
                        {"muted", muted == TRUE},
                    });
                    sav->Release();
                }
                c2->Release();
            }
            control->Release();
        }
        sessionEnumerator->Release();
    }
    mgr->Release();
    if (device) device->Release();
    mmDevEnumerator->Release();

    // Drop cached icons for processes that no longer have an audio session.
    QSet<DWORD> live;
    for (const auto& v : arr) {
        const QJsonObject obj = v.toObject();
        bool ok = false;
        const DWORD p = obj.value("pid").toString().toULong(&ok);
        if (ok) live.insert(p);
    }
    for (auto it = g_iconCache.begin(); it != g_iconCache.end();) {
        if (live.contains(it.key())) {
            ++it;
        } else {
            it = g_iconCache.erase(it);
        }
    }

    m_sessions = arr;
}
