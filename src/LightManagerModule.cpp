#include "LightManagerModule.h"

LightManagerModule::LightManagerModule()
{
    memset(_lastBlockedState, 0, sizeof(_lastBlockedState));
    memset(_lastPushedValues, 0, sizeof(_lastPushedValues));
}

const std::string LightManagerModule::name()
{
    return "LightManagerModule";
}

const std::string LightManagerModule::version()
{
    return "0.1.0";
}

void LightManagerModule::setup()
{
    setupHclFromParams();
    HCL::masterManager.setup();
}

void LightManagerModule::loop()
{
    // Provide current time to HCL master manager
    const uint32_t nowMs = millis();
    const uint16_t timeMinutes = (uint16_t)((nowMs / 60000UL) % 1440);
    HCL::masterManager.loop(timeMinutes);

    // Push updated values to registered outputs
    for (uint8_t m = 1; m <= HCL::MasterManager::MAX_MASTERS; m++)
    {
        const uint8_t idx = m - 1;
        const bool blocked = HCL::masterManager.isApplyBlocked() ||
                             HCL::masterManager.isMasterApplyBlocked(m);

        if (blocked)
        {
            _lastBlockedState[idx] = true;
            continue;
        }

        const bool wasBlocked = _lastBlockedState[idx];
        _lastBlockedState[idx] = false;

        const HCL::InterpolatedValue val = HCL::masterManager.getCurrentValue(m);

        // Push if value changed or just unblocked
        if (wasBlocked ||
            val.kelvin != _lastPushedValues[idx].kelvin ||
            val.brightness != _lastPushedValues[idx].brightness)
        {
            _lastPushedValues[idx] = val;
            pushToOutputs(m, val);
        }
    }
}

void LightManagerModule::processInputKo(GroupObject& ko)
{
    // KNX KO processing will be added in Phase 3 (KO migration from HueGatewayModule)
}

bool LightManagerModule::processCommand(const std::string cmd, bool diagnoseKo)
{
    return false;
}

void LightManagerModule::registerOutput(uint8_t masterNum, ILightManagerOutput* target)
{
    if (masterNum < 1 || masterNum > HCL::MasterManager::MAX_MASTERS || target == nullptr)
        return;

    _outputs.push_back({masterNum, target});
}

void LightManagerModule::notifyOutputActive(uint8_t masterNum, bool active)
{
    if (masterNum < 1 || masterNum > HCL::MasterManager::MAX_MASTERS)
        return;

    // When a light turns off, release any per-master lock so HCL can resume
    // when the light is switched back on.
    if (!active)
    {
        HCL::masterManager.setMasterApplyBlocked(masterNum, false);
    }
}

void LightManagerModule::pushToOutputs(uint8_t masterNum, const HCL::InterpolatedValue& value)
{
    const uint8_t fadeDuration = HCL::masterManager.getFadeDuration();

    for (auto& reg : _outputs)
    {
        if (reg.masterNum == masterNum && reg.target != nullptr)
        {
            reg.target->onLightManagerValue(masterNum, value.kelvin, value.brightness, fadeDuration);
        }
    }
}

void LightManagerModule::setupHclFromParams()
{
    // HCL master configuration from ETS parameters will be added in Phase 3.
    // For now, set sensible defaults so the module compiles and runs.
    HCL::masterManager.setUpdateInterval(60);   // 60 second update interval
    HCL::masterManager.setFadeDuration(10);      // 10 second fade
}
