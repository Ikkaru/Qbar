#include "indicators_service.h"
#include <windows.h>
#include <chrono>
#include <winrt/Windows.System.Power.h>
#include <winrt/Windows.Devices.Power.h>
// IReference<int32_t>::Value() is only declared here; without it the
// BatteryReport capacity figures have no definition to call.
#include <winrt/Windows.Foundation.h>
#include <QDebug>

// Battery saver is read through WinRT's Windows.System.Power.PowerManager.
// The classic alternative, CallPowerSetting with GUID_POWER_SAVING_STATUS, is
// simply not present in this SDK: it is undeclared in powrprof.h and absent from
// powrprof.dll, and the registry mirror of the setting does not exist until the
// user toggles it. EnergySaverStatus is the supported replacement and reports
// the same thing. The C++/WinRT dependency is already in the build for the
// network module's ConnectionProfile.
using namespace winrt::Windows::System::Power;

// Time estimates come from Windows.Devices.Power, a different namespace:
// Windows.System.Power.PowerManager exposes RemainingDischargeTime (and nothing
// for charging), while Windows.Devices.Power.Battery.GetReport() returns the
// charge rate and the capacity figures that time-to-full has to be derived from.
using namespace winrt::Windows::Devices::Power;

IndicatorsService::IndicatorsService(QObject* parent) : QObject(parent) {
    m_timer = new QTimer(this);
    m_timer->setInterval(m_intervalMs);
    connect(m_timer, &QTimer::timeout, this, &IndicatorsService::poll);
    poll();
    m_timer->start();
}

void IndicatorsService::poll() {
    pollBattery();
    pollNetwork();
    pollVolume();
    emit stateChanged();
}

void IndicatorsService::pollBattery() {
    SYSTEM_POWER_STATUS s;
    if (!GetSystemPowerStatus(&s)) return;

    // ACLineStatus only answers "is the cable in". It cannot distinguish a
    // battery that is actively filling from one that is full and deliberately
    // held there, which look identical to it. WinRT's BatteryStatus does:
    // Charging means current is flowing, Idle means plugged in and paused.
    // That is the signal for a charge limit / smart-charge feature, so the bar
    // can show a shield instead of a bolt while the cable is still connected.
    const auto status = PowerManager::BatteryStatus();
    const bool plugged = s.ACLineStatus == 1;
    const bool holding = plugged && status == BatteryStatus::Idle;

    m_state["battery"] = QJsonObject{
        {"present", s.BatteryLifePercent != 255},
        {"percent", s.BatteryLifePercent},
        {"charging", plugged && !holding},
        {"plugged", plugged},
        {"holding", holding},
        {"saver", batterySaverOn()},
    };

    if (s.BatteryLifePercent != 255) {
        QJsonObject battery = m_state["battery"].toObject();
        addTimeEstimates(battery);
        m_state["battery"] = battery;
    }
}

// Two separate estimates, because the two APIs do not overlap at all.
//
// Discharging: RemainingDischargeTime is reported directly by the OS.
//   The classic GetSystemPowerStatus route is useless here - it returned
//   0xFFFFFFFF for both BatteryLifeTime and BatteryFullLifeTime on every
//   machine probed, i.e. always "unknown".
//
// Charging: no API reports time-to-full. It has to be derived, from the charge
//   rate the OS reports and the capacity still to be added.
void IndicatorsService::addTimeEstimates(QJsonObject& battery) {
    // -1 means "the OS would not say", which the popup renders as nothing at all
    // rather than as a wrong number.
    int secondsLeft = -1;
    int secondsToFull = -1;

    try {
        // Throws on a machine with no battery, so it cannot be read
        // unconditionally. The C++/WinRT projection hands back a std::chrono
        // duration, not the raw TimeSpan ticks, so it converts with count()
        // rather than a 100ns division.
        const auto span = PowerManager::RemainingDischargeTime();
        const auto count = span.count();
        if (count > 0)
            secondsLeft = int(std::chrono::duration_cast<std::chrono::seconds>(span).count());
    } catch (const winrt::hresult_error&) {
        // No battery, or the OS declined to estimate.
    }

    try {
        // AggregateBattery, not FromIdAsync: a laptop with two cells exposes one
        // aggregate report, and FromIdAsync needs a device GUID that this code
        // has no way to discover.
        const auto report = Battery::AggregateBattery().GetReport();
        // Each figure comes back as IReference<int32_t>: nullable, and null is
        // how the OS says "this cell does not report that". So every one has to
        // be unwrapped and checked, not just tested for sign.
        const auto rateRef = report.ChargeRateInMilliwatts();
        const auto fullRef = report.FullChargeCapacityInMilliwattHours();
        const auto remainingRef = report.RemainingCapacityInMilliwattHours();

        // ChargeRateInMilliwatts is negative while discharging and zero while
        // paused, so a positive rate is the only case where an estimate means
        // anything.
        if (rateRef && fullRef && remainingRef) {
            const int rate = rateRef.Value();
            const int fullCapacity = fullRef.Value();
            // Not "remaining": windows.h defines min/max macros that this name
            // collides with.
            const int chargeLeft = remainingRef.Value();

            if (rate > 0 && fullCapacity > 0 && chargeLeft >= 0 &&
                chargeLeft < fullCapacity) {
                // milliwatt-hours divided by milliwatts is hours, so only the
                // hour conversion is left. Nothing reports time-to-full
                // directly; this is a rate average, so it drifts as the charger
                // tapers off near full.
                const double hours = double(fullCapacity - chargeLeft) / double(rate);
                secondsToFull = int(hours * 3600.0);
            }
        }
    } catch (const winrt::hresult_error&) {
        // No battery device.
    }

    battery["secondsLeft"] = secondsLeft;
    battery["secondsToFull"] = secondsToFull;
}

// Disabled means the machine has no battery at all, which the bar already
// covers by hiding the battery icon; On is what the user wants to see.
bool IndicatorsService::batterySaverOn() {
    return PowerManager::EnergySaverStatus() == EnergySaverStatus::On;
}

void IndicatorsService::pollNetwork() {
    // Wifi detail now lives in NetworkService (SSID, signal, reachability).
    // The indicator row only needs a connected flag for its icon.
    m_state["network"] = QJsonObject{
        {"connected", false},
    };
}

void IndicatorsService::pollVolume() {
    // Master volume comes with the WASAPI per-app mixer (Phase 3 item 4).
    // Until then the icon renders the neutral full-volume glyph.
    m_state["volume"] = QJsonObject{
        {"muted", false},
        {"level", 100},
    };
}
