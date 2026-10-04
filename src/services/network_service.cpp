#include "network_service.h"
#include <winrt/Windows.Networking.Connectivity.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <string>

using namespace winrt::Windows::Networking::Connectivity;

// Network state comes from Windows.Networking.Connectivity, the same API the
// shell tray icon uses.
//
// The native Wifi API was the obvious source for the SSID, but
// WlanQueryInterface(wlan_intf_opcode_current_connection) answers
// ERROR_ACCESS_DENIED on current Windows 11 builds - "requires elevation" -
// so an unelevated bar can never read it. netsh reports the same thing.
// ConnectionProfile.ProfileName is the SSID for a wireless profile, so one
// object covers the name, the connectivity level and the transport, none of
// which need elevation.
namespace {

QString labelFor(NetworkConnectivityLevel level) {
    switch (level) {
    case NetworkConnectivityLevel::InternetAccess:
    case NetworkConnectivityLevel::ConstrainedInternetAccess:
        return QStringLiteral("Internet Access");
    case NetworkConnectivityLevel::LocalAccess:
        return QStringLiteral("LAN Access");
    default:
        return QStringLiteral("No Internet");
    }
}

// Higher is better. ConnectivityLevel cannot be compared directly across
// versions, so rank it by hand.
int rankFor(NetworkConnectivityLevel level) {
    switch (level) {
    case NetworkConnectivityLevel::InternetAccess:
        return 3;
    case NetworkConnectivityLevel::ConstrainedInternetAccess:
        return 2;
    case NetworkConnectivityLevel::LocalAccess:
        return 1;
    default:
        return 0;
    }
}

} // namespace

NetworkService::NetworkService(QObject* parent) : QObject(parent) {
    refresh();
}

void NetworkService::refresh() {
    // Several profiles can be listed at once (a VPN, a docked Ethernet NIC, a
    // hotspot). Pick the one actually carrying traffic, preferring Wi-Fi when
    // two rank equal, because that is the icon the user recognises and the one
    // whose name is an SSID.
    int bestRank = -1;
    QString bestName;
    QString bestReachability;
    QString bestKind = QStringLiteral("none");

    for (const auto& profile : NetworkInformation::GetConnectionProfiles()) {
        const NetworkConnectivityLevel level = profile.GetNetworkConnectivityLevel();

        int rank = rankFor(level);
        if (profile.IsWlanConnectionProfile() || profile.IsWwanConnectionProfile())
            rank += 0x100;
        if (rank <= bestRank)
            continue;

        bestRank = rank;
        bestName = QString::fromStdWString(std::wstring(profile.ProfileName()));
        bestReachability = labelFor(level);
        bestKind = (profile.IsWlanConnectionProfile() || profile.IsWwanConnectionProfile())
                       ? QStringLiteral("wifi")
                       : QStringLiteral("lan");
    }

    m_ssid = bestName;
    m_reachability = bestReachability;
    m_kind = bestKind;
    m_connected = bestRank >= 0;
    emit changed();
}