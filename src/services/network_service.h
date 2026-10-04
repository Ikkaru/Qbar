#pragma once
#include <QObject>
#include <QString>

// Network state comes from Windows.Networking.Connectivity, the same API the
// shell tray icon uses.
//
// The native Wifi API was the obvious source for the SSID, but
// WlanQueryInterface(wlan_intf_opcode_current_connection) now answers
// ERROR_ACCESS_DENIED ("requires elevation") on current Windows 11 builds, so
// an unelevated bar can never read it - netsh reports the same thing.
// ConnectionProfile.ProfileName is the SSID for a wireless profile, so one
// object gives connectivity, transport and SSID without elevation.
class NetworkService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString ssid READ ssid NOTIFY changed)
    Q_PROPERTY(QString reachability READ reachability NOTIFY changed)
    // Which transport carries the link: "wifi", "lan" or "none". Reachability
    // alone cannot tell a wireless profile from a wired one.
    Q_PROPERTY(QString kind READ kind NOTIFY changed)
    Q_PROPERTY(bool connected READ connected NOTIFY changed)

public:
    explicit NetworkService(QObject* parent = nullptr);

    QString ssid() const { return m_ssid; }
    QString reachability() const { return m_reachability; }
    QString kind() const { return m_kind; }
    bool connected() const { return m_connected; }

public slots:
    void refresh();

signals:
    void changed();

private:
    QString m_ssid;
    QString m_reachability;
    QString m_kind = QStringLiteral("none");
    bool m_connected = false;
};