#include "HCLMasterManager.h"

namespace HCL {

// Global facade instance (stateless w.r.t. masters)
MasterManager masterManager;

MasterManager::MasterManager()
    : _enabled(false)
    , _applyBlocked(false)
    , _lastTimeMinutes(0xFFFF)
    , _lastDayOfYear(-1)
{
}

void MasterManager::setup() {
    _lastTimeMinutes = 0xFFFF;
    _lastDayOfYear = -1;
}

void MasterManager::loop(uint16_t currentTimeMinutes, int16_t dayOfYear) {
    if (!_enabled) return;
    if (_provider == nullptr) return;
    // Sentinel: caller signals "no valid time" via dayOfYear=-1. Recalculating
    // with currentTimeMinutes=0 would falsely apply the SP1 setpoint.
    if (dayOfYear < 0) return;

    if (_lastTimeMinutes != currentTimeMinutes
        || _lastDayOfYear   != dayOfYear)
    {
        updateCurrentValues(currentTimeMinutes, dayOfYear);
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
    _lastTimeMinutes = 0xFFFF;
}

void MasterManager::updateCurrentValues(uint16_t currentTimeMinutes, int16_t dayOfYear) {
    // Phase 2.J.c: legacy master-level curve evaluation (isValid / calculateValue /
    // providerSetCurrentValue) retired. ProfileV2 resolves per-channel in
    // LightManagerChannel::loopHcl() instead. This stub is kept so existing
    // forceUpdate()/loop() bookkeeping stays intact until Phase 2.K wires
    // adaptive-brightness back into the ProfileV2 pipeline.
    (void)currentTimeMinutes;
    (void)dayOfYear;
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
