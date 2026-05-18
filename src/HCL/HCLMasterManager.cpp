#include "HCLMasterManager.h"

namespace HCL {

// Global facade instance (stateless w.r.t. masters)
MasterManager masterManager;

MasterManager::MasterManager()
    : _enabled(false)
    , _applyBlocked(false)
    , _updateIntervalMs(60000)
    , _fadeDurationSec(5)
    , _lastUpdateMs(0)
    , _lastTimeMinutes(0xFFFF)
    , _lastDayOfYear(-1)
{
}

void MasterManager::setup() {
    _lastUpdateMs = millis();
    _lastTimeMinutes = 0xFFFF;
    _lastDayOfYear = -1;
}

void MasterManager::loop(uint16_t currentTimeMinutes, int16_t dayOfYear) {
    if (!_enabled) return;
    if (_provider == nullptr) return;
    // Sentinel: caller signals "no valid time" via dayOfYear=-1. Recalculating
    // with currentTimeMinutes=0 would falsely apply the SP1 setpoint.
    if (dayOfYear < 0) return;

    uint32_t now = millis();
    if ((now - _lastUpdateMs) >= _updateIntervalMs
        || _lastTimeMinutes != currentTimeMinutes
        || _lastDayOfYear   != dayOfYear)
    {
        updateCurrentValues(currentTimeMinutes, dayOfYear);
        _lastUpdateMs    = now;
        _lastTimeMinutes = currentTimeMinutes;
        _lastDayOfYear   = dayOfYear;
    }
}

Master* MasterManager::getMaster(uint8_t masterNum) {
    return _provider ? _provider->providerGetMaster(masterNum) : nullptr;
}

InterpolatedValue MasterManager::getCurrentValue(uint8_t masterNum) const {
    if (_provider == nullptr) return InterpolatedValue();
    return _provider->providerGetCurrentValue(masterNum);
}

void MasterManager::setMasterApplyBlocked(uint8_t masterNum, bool blocked) {
    if (_provider) _provider->providerSetMasterApplyBlocked(masterNum, blocked);
}

bool MasterManager::isMasterApplyBlocked(uint8_t masterNum) const {
    return _provider ? _provider->providerIsMasterApplyBlocked(masterNum) : false;
}

void MasterManager::forceUpdate() {
    _lastUpdateMs = 0;
}

uint32_t MasterManager::getTimeUntilNextUpdate() const {
    uint32_t now = millis();
    uint32_t elapsed = now - _lastUpdateMs;
    return (elapsed >= _updateIntervalMs) ? 0 : (_updateIntervalMs - elapsed);
}

void MasterManager::updateCurrentValues(uint16_t currentTimeMinutes, int16_t dayOfYear) {
    if (_provider == nullptr) return;
    const uint8_t count = _provider->providerMasterCount();
    uint32_t now = millis();
    for (uint8_t i = 1; i <= count; i++) {
        Master* m = _provider->providerGetMaster(i);
        if (m == nullptr || !m->isValid()) continue;
        InterpolatedValue val = m->calculateValue(currentTimeMinutes, now, dayOfYear);
        _provider->providerSetCurrentValue(i, val);
        #ifdef DEBUG_HCL
        Serial.printf("[HCL] Master %d: %dK, %d%%\n", i, val.kelvin, val.brightness);
        #endif
    }
}

void MasterManager::setMasterAmbientLux(uint8_t masterNum, float lux) {
    Master* m = getMaster(masterNum);
    if (m == nullptr) return;
    m->setAmbientLux(lux);
    forceUpdate();
}

void MasterManager::setMasterDaytime(uint8_t masterNum, bool isDaytime) {
    Master* m = getMaster(masterNum);
    if (m == nullptr) return;
    m->setDaytime(isDaytime);
}

bool MasterManager::isMasterAdaptiveActive(uint8_t masterNum) const {
    if (_provider == nullptr) return false;
    return _provider->providerIsMasterAdaptiveActive(masterNum, _lastTimeMinutes, millis());
}

} // namespace HCL
