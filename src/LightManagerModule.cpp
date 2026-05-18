#include "LightManagerModule.h"
#include "knxprod.h"

namespace
{

int16_t decodeBaseTimezoneOffsetMinutes(uint8_t timezoneRaw)
{
    if (timezoneRaw == 31) return 60;
    if (timezoneRaw >= 17 && timezoneRaw <= 27)
        return static_cast<int16_t>((static_cast<int16_t>(timezoneRaw) - 28) * 60);
    if (timezoneRaw == 28) return 60;
    if (timezoneRaw <= 12) return static_cast<int16_t>(timezoneRaw * 60);
    return 60;
}

String readFixedTimeParam(const uint8_t* rawTime)
{
    if (rawTime == nullptr || rawTime[0] == '\0')
        return String("");
    char buffer[6] = {0, 0, 0, 0, 0, 0};
    memcpy(buffer, rawTime, 5);
    return String(buffer);
}

} // namespace

LightManagerModule::LightManagerModule() = default;

const std::string LightManagerModule::name()    { return "LightManagerModule"; }
const std::string LightManagerModule::version() { return "0.2.0"; }

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void LightManagerModule::setup()
{
    setHclLock(false, "setup");

    const bool enabled = (ParamLMG_LMGHCLEnable != 0);
    const uint8_t requested = enabled ? ParamLMG_LMGHCLMasterCount : 0;
    const uint8_t count = (requested > HCL::MasterManager::MAX_MASTERS)
                            ? HCL::MasterManager::MAX_MASTERS : requested;

    // Register ourselves as IMasterProvider so the facade can look up channel-hosted masters.
    HCL::masterManager.setProvider(this);
    HCL::masterManager.setEnabled(enabled);
    HCL::masterManager.setUpdateInterval(ParamLMG_LMGHCLUpdateInterval);
    HCL::masterManager.setFadeDuration(ParamLMG_LMGHCLFadeDuration);

    // Global lock fallback parameters
    _hclLockFallbackMode = ParamLMG_LMGHCLLockFallback;
    _hclFallbackPolicy   = ParamLMG_LMGHCLFallbackPolicy;
    if (_hclFallbackPolicy > static_cast<uint8_t>(HclLockFallbackPolicy::ExternalOnly))
        _hclFallbackPolicy = static_cast<uint8_t>(HclLockFallbackPolicy::Legacy);
    {
        const uint64_t durMs = static_cast<uint64_t>(ParamLMG_LMGHCLFallbackDurationSec) * 1000ULL;
        _hclFallbackDurationMs = (durMs > 0xFFFFFFFFULL) ? 0xFFFFFFFFUL : static_cast<uint32_t>(durMs);
    }
    {
        const String relStr = readFixedTimeParam(ParamLMG_LMGHCLFallbackReleaseTime);
        _hclFallbackReleaseMinuteOfDay = HCL::Setpoint::parseTime(relStr.c_str());
    }

    // Resolve global location/timezone (BASE common params)
    float latitude  = 50.115377f;
    float longitude = 8.684170f;
    int16_t timezoneOffsetMinutes = 60;
#ifdef ParamBASE_Latitude
    latitude  = ParamBASE_Latitude;
#endif
#ifdef ParamBASE_Longitude
    longitude = ParamBASE_Longitude;
#endif
#ifdef ParamBASE_Timezone
    timezoneOffsetMinutes = decodeBaseTimezoneOffsetMinutes(static_cast<uint8_t>(ParamBASE_Timezone));
#endif

    // Create channels and configure their HCL master
    _channels.clear();
    _channels.reserve(count);
    for (uint8_t i = 0; i < count; i++)
    {
        auto ch = std::unique_ptr<LightManagerChannel>(new LightManagerChannel(i));
        ch->setupHcl(latitude, longitude, timezoneOffsetMinutes);
        _channels.push_back(std::move(ch));
    }

    HCL::masterManager.setup();
}

// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------

void LightManagerModule::loop()
{
    struct tm timeinfo;
    const bool hasTime = getLocalTime(&timeinfo, 0);
    uint16_t timeMinutes = 0;
    int16_t dayOfYear = -1;
    if (hasTime)
    {
        timeMinutes = static_cast<uint16_t>(timeinfo.tm_hour * 60 + timeinfo.tm_min);
        dayOfYear   = static_cast<int16_t>(timeinfo.tm_yday + 1);
    }

    for (auto& ch : _channels)
        ch->loopHcl(hasTime ? &timeinfo : nullptr, hasTime);

    // Skip HCL value recalculation when no valid time is available; otherwise
    // a momentary getLocalTime()==false would feed timeMinutes=0/dayOfYear=-1
    // into the master and cause the published value to jump to the SP1 setpoint.
    if (hasTime)
        HCL::masterManager.loop(timeMinutes, dayOfYear);

    evaluateHclLockFallback(hasTime ? &timeinfo : nullptr, hasTime);

    for (auto& ch : _channels)
    {
        ch->pushIfChanged();

        // Also push to registered ILightManagerOutput sinks (Hue etc.)
        const uint8_t mn = ch->masterNumber();
        const bool blocked = HCL::masterManager.isApplyBlocked() || ch->applyBlocked();
        if (blocked) continue;
        const HCL::InterpolatedValue val = ch->currentValue();
        const uint8_t fade = HCL::masterManager.getFadeDuration();
        for (auto& reg : _outputs)
        {
            if (reg.masterNum == mn && reg.target != nullptr)
                reg.target->onLightManagerValue(mn, val.kelvin, val.brightness, fade);
        }
    }
}

// ---------------------------------------------------------------------------
// KO handling
// ---------------------------------------------------------------------------

void LightManagerModule::processInputKo(GroupObject& ko)
{
    const uint16_t koNumber = ko.asap();

    if (koNumber == LMG_KoLMGHCLLock)
    {
        setHclLock(ko.value(Dpt(1, 1)), "KO");
        return;
    }

    if (koNumber == LMG_KoLMGHCLReleaseTrigger)
    {
        if (ko.value(Dpt(1, 1)))
        {
            setHclLock(false, "KO release trigger");
            for (auto& ch : _channels)
                ch->setLock(false, "KO release trigger");
        }
        return;
    }

    const int16_t chIdx = LMG_KoCalcChannel(koNumber);
    if (chIdx < 0) return;
    if (chIdx >= static_cast<int16_t>(_channels.size())) return;

    // LMG_KoCalcIndex() expands to _channelIndex (channel-scope macro) — compute manually here.
    const uint16_t koIdx = static_cast<uint16_t>((koNumber - LMG_KoBlockOffset) % LMG_KoBlockSize);
    _channels[chIdx]->processChannelKo(ko, koIdx);
}

bool LightManagerModule::processCommand(const std::string /*cmd*/, bool /*diagnoseKo*/)
{
    return false;
}

// ---------------------------------------------------------------------------
// Output registration
// ---------------------------------------------------------------------------

void LightManagerModule::registerOutput(uint8_t masterNum, ILightManagerOutput* target)
{
    if (masterNum < 1 || masterNum > HCL::MasterManager::MAX_MASTERS || target == nullptr)
        return;

    for (auto it = _outputs.begin(); it != _outputs.end();)
    {
        if (it->target == target) it = _outputs.erase(it);
        else ++it;
    }

    _outputs.push_back({masterNum, target});

    if (masterNum > _channels.size()) return;
    LightManagerChannel* ch = _channels[masterNum - 1].get();
    const bool blocked = HCL::masterManager.isApplyBlocked() || ch->applyBlocked();
    if (!blocked)
    {
        const HCL::InterpolatedValue val = ch->currentValue();
        const uint8_t fade = HCL::masterManager.getFadeDuration();
        target->onLightManagerValue(masterNum, val.kelvin, val.brightness, fade);
    }
}

void LightManagerModule::unregisterOutput(ILightManagerOutput* target)
{
    if (target == nullptr) return;
    for (auto it = _outputs.begin(); it != _outputs.end();)
    {
        if (it->target == target) it = _outputs.erase(it);
        else ++it;
    }
}

void LightManagerModule::notifyOutputActive(uint8_t masterNum, bool active)
{
    if (masterNum < 1 || masterNum > _channels.size()) return;
    if (!active)
        _channels[masterNum - 1]->setApplyBlocked(false);
}

// ---------------------------------------------------------------------------
// Lock state machine (global)
// ---------------------------------------------------------------------------

void LightManagerModule::setHclLock(bool active, const char* reason)
{
    const bool changed = (_hclLockActive != active);
    _hclLockActive = active;
    HCL::masterManager.setApplyBlocked(active);

    if (active)
    {
        _hclLockActivatedMs = millis();
        _hclLockAutoReleaseMs = 0;
        _hclLockActivationDayOfYear = -1;
        _hclLockActivationMinuteOfDay = -1;

        const HclLockFallbackPolicy policy = static_cast<HclLockFallbackPolicy>(_hclFallbackPolicy);
        if (policy == HclLockFallbackPolicy::Legacy)
        {
            const uint32_t d = getHclFallbackDurationMs(static_cast<HclLockFallbackMode>(_hclLockFallbackMode));
            if (d > 0) _hclLockAutoReleaseMs = _hclLockActivatedMs + d;
        }
        else if (policy == HclLockFallbackPolicy::Duration || policy == HclLockFallbackPolicy::DurationOrTime)
        {
            if (_hclFallbackDurationMs > 0)
                _hclLockAutoReleaseMs = _hclLockActivatedMs + _hclFallbackDurationMs;
        }

        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 0))
        {
            _hclLockActivationDayOfYear = static_cast<int16_t>(timeinfo.tm_yday);
            _hclLockActivationMinuteOfDay = static_cast<int16_t>(timeinfo.tm_hour * 60 + timeinfo.tm_min);
        }

        if (changed)
            Serial.printf("[LightManagerModule] HCL lock enabled (%s)\n", reason ? reason : "n/a");
    }
    else
    {
        _hclLockActivatedMs = 0;
        _hclLockAutoReleaseMs = 0;
        _hclLockActivationDayOfYear = -1;
        _hclLockActivationMinuteOfDay = -1;
        if (changed)
            Serial.printf("[LightManagerModule] HCL lock disabled (%s)\n", reason ? reason : "n/a");
    }
    publishHclLockStatus();
}

void LightManagerModule::setHclManagerLock(uint8_t managerNumber, bool active, const char* reason)
{
    if (managerNumber < 1 || managerNumber > _channels.size()) return;
    _channels[managerNumber - 1]->setLock(active, reason);
}

void LightManagerModule::publishHclLockStatus()
{
    knx.getGroupObject(LMG_KoLMGHCLLockStatus).value(_hclLockActive, Dpt(1, 1));
}

void LightManagerModule::evaluateHclLockFallback(const tm* timeinfo, bool hasTime)
{
    if (!_hclLockActive) return;

    const HclLockFallbackPolicy policy = static_cast<HclLockFallbackPolicy>(_hclFallbackPolicy);
    if (policy == HclLockFallbackPolicy::ExternalOnly) return;

    if (policy != HclLockFallbackPolicy::Legacy)
    {
        const bool byDuration = (_hclLockAutoReleaseMs != 0) &&
                                (static_cast<long>(millis() - _hclLockAutoReleaseMs) >= 0);
        const bool byTime = (policy == HclLockFallbackPolicy::TimeOfDay || policy == HclLockFallbackPolicy::DurationOrTime) &&
                            shouldReleaseByPolicyTime(_hclLockActivationDayOfYear, _hclLockActivationMinuteOfDay,
                                                      _hclFallbackReleaseMinuteOfDay, timeinfo, hasTime);
        if (byDuration || byTime)
            setHclLock(false, byTime ? "fallback release time" : "fallback duration elapsed");
        return;
    }

    const HclLockFallbackMode mode = static_cast<HclLockFallbackMode>(_hclLockFallbackMode);
    if (mode == HclLockFallbackMode::None) return;

    if (_hclLockAutoReleaseMs != 0)
    {
        if (static_cast<long>(millis() - _hclLockAutoReleaseMs) >= 0)
            setHclLock(false, "fallback duration elapsed");
        return;
    }

    if (mode == HclLockFallbackMode::NextDay && hasTime && timeinfo != nullptr)
    {
        if (_hclLockActivationDayOfYear >= 0 && timeinfo->tm_yday != _hclLockActivationDayOfYear)
            setHclLock(false, "fallback day change");
    }
}

uint32_t LightManagerModule::getHclFallbackDurationMs(HclLockFallbackMode mode) const
{
    switch (mode)
    {
        case HclLockFallbackMode::Min1:  return   1UL * 60UL * 1000UL;
        case HclLockFallbackMode::Min2:  return   2UL * 60UL * 1000UL;
        case HclLockFallbackMode::Min5:  return   5UL * 60UL * 1000UL;
        case HclLockFallbackMode::Min10: return  10UL * 60UL * 1000UL;
        case HclLockFallbackMode::Min20: return  20UL * 60UL * 1000UL;
        case HclLockFallbackMode::Min30: return  30UL * 60UL * 1000UL;
        case HclLockFallbackMode::Hour1: return   1UL * 60UL * 60UL * 1000UL;
        case HclLockFallbackMode::Hour2: return   2UL * 60UL * 60UL * 1000UL;
        case HclLockFallbackMode::Hour5: return   5UL * 60UL * 60UL * 1000UL;
        case HclLockFallbackMode::Hour8: return   8UL * 60UL * 60UL * 1000UL;
        case HclLockFallbackMode::Hour12: return 12UL * 60UL * 60UL * 1000UL;
        default: return 0;
    }
}

bool LightManagerModule::shouldReleaseByPolicyTime(int16_t activationDayOfYear, int16_t activationMinuteOfDay,
                                                    uint16_t releaseMinuteOfDay,
                                                    const tm* timeinfo, bool hasTime) const
{
    if (!hasTime || timeinfo == nullptr || releaseMinuteOfDay == 0xFFFF) return false;
    if (activationDayOfYear < 0 || activationMinuteOfDay < 0) return false;

    const int16_t currentDay = static_cast<int16_t>(timeinfo->tm_yday);
    const int16_t currentMin = static_cast<int16_t>(timeinfo->tm_hour * 60 + timeinfo->tm_min);

    if (currentDay < activationDayOfYear) return false;
    if (currentDay == activationDayOfYear)
    {
        if (releaseMinuteOfDay <= static_cast<uint16_t>(activationMinuteOfDay)) return false;
        return currentMin >= static_cast<int16_t>(releaseMinuteOfDay);
    }
    return currentMin >= static_cast<int16_t>(releaseMinuteOfDay);
}

// ---------------------------------------------------------------------------
// Flash persistence (per-master "is summer" for season mode 3)
// ---------------------------------------------------------------------------

// 0x01: pre-channel-owner layout (state lived in HCL::MasterManager arrays).
// 0x02: channel-owner layout (state lives in LightManagerChannel; bit i =
//       channel i = master i+1). Identical byte layout but bumped so older
//       firmwares' flash is cleanly discarded on first read.
static constexpr uint8_t LMG_SUMMER_FLASH_VERSION = 0x02;

uint16_t LightManagerModule::flashSize() { return 3; }

void LightManagerModule::writeFlash()
{
    uint16_t mask = 0;
    for (auto& ch : _channels)
    {
        if (ch->isSummer())
            mask |= static_cast<uint16_t>(1u << (ch->masterNumber() - 1));
    }
    openknx.flash.writeByte(LMG_SUMMER_FLASH_VERSION);
    openknx.flash.writeByte(static_cast<uint8_t>(mask >> 8));
    openknx.flash.writeByte(static_cast<uint8_t>(mask & 0xFF));
}

void LightManagerModule::readFlash(const uint8_t* /*data*/, const uint16_t size)
{
    if (size < 3) return;
    const uint8_t version = openknx.flash.readByte();
    if (version != LMG_SUMMER_FLASH_VERSION) return;

    const uint8_t hi = openknx.flash.readByte();
    const uint8_t lo = openknx.flash.readByte();
    const uint16_t mask = (static_cast<uint16_t>(hi) << 8) | lo;

    for (auto& ch : _channels)
        ch->restoreSummerFromMask(mask);
}

// ---------------------------------------------------------------------------
// HCL::IMasterProvider implementation
// ---------------------------------------------------------------------------

HCL::Master* LightManagerModule::providerGetMaster(uint8_t masterNum)
{
    if (masterNum < 1 || masterNum > _channels.size()) return nullptr;
    return _channels[masterNum - 1]->masterPtr();
}

HCL::InterpolatedValue LightManagerModule::providerGetCurrentValue(uint8_t masterNum) const
{
    if (masterNum < 1 || masterNum > _channels.size()) return HCL::InterpolatedValue{};
    return _channels[masterNum - 1]->currentValue();
}

void LightManagerModule::providerSetCurrentValue(uint8_t masterNum, const HCL::InterpolatedValue& value)
{
    if (masterNum < 1 || masterNum > _channels.size()) return;
    _channels[masterNum - 1]->setCurrentValue(value);
}

bool LightManagerModule::providerIsMasterApplyBlocked(uint8_t masterNum) const
{
    if (masterNum < 1 || masterNum > _channels.size()) return false;
    return _channels[masterNum - 1]->applyBlocked();
}

void LightManagerModule::providerSetMasterApplyBlocked(uint8_t masterNum, bool blocked)
{
    if (masterNum < 1 || masterNum > _channels.size()) return;
    _channels[masterNum - 1]->setApplyBlocked(blocked);
}

bool LightManagerModule::providerIsMasterAdaptiveActive(uint8_t masterNum, uint16_t currentTimeMinutes, uint32_t nowMs) const
{
    if (masterNum < 1 || masterNum > _channels.size()) return false;
    return _channels[masterNum - 1]->isAdaptiveActive(currentTimeMinutes, nowMs);
}
