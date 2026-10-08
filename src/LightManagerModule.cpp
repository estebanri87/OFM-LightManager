#include "LightManagerModule.h"
#include "LightManagerUtil.h"
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

} // namespace

LightManagerModule::LightManagerModule() = default;

const std::string LightManagerModule::name()    { return "LightManager"; }
const std::string LightManagerModule::version()
{
    return MODULE_LightManagerModule_Version;
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void LightManagerModule::setup()
{
    setHclLock(false, "setup");

    const bool enabled = (ParamLMG_LMGHCLEnable != 0);

    // Register ourselves as IMasterProvider so the facade can look up channel-hosted masters.
    HCL::masterManager.setProvider(this);
    HCL::masterManager.setEnabled(enabled);
    // (UpdateInterval / FadeDuration are now per-channel — see LightManagerChannel::setupHcl.)

    // Global lock fallback parameters
    _hclLockFallbackMode = ParamLMG_LMGHCLLockFallback;
    _hclFallbackPolicy   = ParamLMG_LMGHCLFallbackPolicy;
    if (_hclFallbackPolicy > static_cast<uint8_t>(HclLockFallbackPolicy::ExternalOnly))
        _hclFallbackPolicy = static_cast<uint8_t>(HclLockFallbackPolicy::Legacy);
    {
        const uint64_t durMs = static_cast<uint64_t>(ParamLMG_LMGHCLFallbackDurationSec) * 1000ULL;
        _hclFallbackDurationMs = (durMs > 0xFFFFFFFFULL) ? 0xFFFFFFFFUL : static_cast<uint32_t>(durMs);
    }
    _hclFallbackReleaseMinuteOfDay = ParamLMG_LMGHCLFallbackReleaseTime;

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

    // Create only the activated, not suspended channels and configure their HCL master
    for (uint8_t i = 0; i < HCL::MasterManager::MAX_MASTERS; i++)
    {
        _channels[i].reset();
        if (!enabled || !channelConfigured(i)) continue;
        _channels[i].reset(new LightManagerChannel(i));
        _channels[i]->setupHcl(latitude, longitude, timezoneOffsetMinutes);
    }

    HCL::masterManager.setup();
}

// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------

void LightManagerModule::loop()
{
    struct tm timeinfo;
    const bool hasTime = LightManagerUtil::tryGetLocalTime(timeinfo);
    uint16_t timeMinutes = 0;
    int16_t dayOfYear = -1;
    if (hasTime)
    {
        timeMinutes = static_cast<uint16_t>(timeinfo.tm_hour * 60 + timeinfo.tm_min);
        dayOfYear   = static_cast<int16_t>(timeinfo.tm_yday + 1);
    }

    for (auto& ch : _channels)
        if (ch) ch->loopHcl(hasTime ? &timeinfo : nullptr, hasTime);

    // Skip HCL value recalculation when no valid time is available; otherwise
    // a momentary tryGetLocalTime()==false would feed timeMinutes=0/dayOfYear=-1
    // into the master and cause the published value to jump to the SP1 setpoint.
    if (hasTime)
        HCL::masterManager.loop(timeMinutes, dayOfYear);

    evaluateHclLockFallback(hasTime ? &timeinfo : nullptr, hasTime);

    for (auto& ch : _channels)
        if (ch) ch->pushIfChanged();
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
                if (ch) ch->setLock(false, "KO release trigger");
        }
        return;
    }

    const int16_t chIdx = LMG_KoCalcChannel(koNumber);
    if (chIdx < 0) return;
    if (chIdx >= HCL::MasterManager::MAX_MASTERS || !_channels[chIdx]) return;

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

uint16_t LightManagerModule::channelUpdateIntervalSec(uint8_t masterNum) const
{
    const LightManagerChannel* ch = channel(masterNum);
    return ch ? ch->updateIntervalSec() : 0;
}

uint8_t LightManagerModule::channelFadeDurationSec(uint8_t masterNum) const
{
    const LightManagerChannel* ch = channel(masterNum);
    return ch ? ch->fadeDurationSec() : 0;
}

bool LightManagerModule::channelConfigured(uint8_t channelIndex) const
{
    if (channelIndex >= HCL::MasterManager::MAX_MASTERS) return false;
    uint8_t _channelIndex = channelIndex;
    return ParamLMG_CHActive && !ParamLMG_CHDisabled;
}

uint8_t LightManagerModule::activeChannelCount() const
{
    uint8_t count = 0;
    for (const auto& ch : _channels)
        if (ch) count++;
    return count;
}

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

    LightManagerChannel* ch = channel(masterNum);
    if (!ch) return;
    const bool blocked = HCL::masterManager.isApplyBlocked() || ch->applyBlocked();
    if (!blocked && ch->internalOutputEnabled())
    {
        const HCL::InterpolatedValue val = ch->currentValue();
        const uint8_t fade = ch->fadeDurationSec();
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
    LightManagerChannel* ch = channel(masterNum);
    if (!ch) return;
    if (!active)
        ch->setApplyBlocked(false);
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
        if (LightManagerUtil::tryGetLocalTime(timeinfo))
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
    LightManagerChannel* ch = channel(managerNumber);
    if (!ch) return;
    ch->setLock(active, reason);
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
// 0x03: Punkt 5 (Plan Phase 2 Step 4a, NVS-Magic 0x03). SavePower-Gate beim
//       Schreiben/Lesen; bei deaktivierter SavePower wird der Slot ignoriert
//       und SummerActiveInit angewandt.
static constexpr uint8_t LMG_SUMMER_FLASH_VERSION = 0x03;

uint16_t LightManagerModule::flashSize() { return 3; }

void LightManagerModule::writeFlash()
{
    // Punkt 5: nur wenn Master-Param SavePower=Ja persistiert wird.
    if (!ParamLMG_LMGSummerActiveSavePower)
    {
        // Magic ungueltig schreiben, damit naechster Boot Init-Pfad nimmt.
        openknx.flash.writeByte(0x00);
        openknx.flash.writeByte(0x00);
        openknx.flash.writeByte(0x00);
        return;
    }
    uint16_t mask = 0;
    for (auto& ch : _channels)
    {
        if (ch && ch->isSummer())
            mask |= static_cast<uint16_t>(1u << (ch->masterNumber() - 1));
    }
    openknx.flash.writeByte(LMG_SUMMER_FLASH_VERSION);
    openknx.flash.writeByte(static_cast<uint8_t>(mask >> 8));
    openknx.flash.writeByte(static_cast<uint8_t>(mask & 0xFF));
}

void LightManagerModule::readFlash(const uint8_t* /*data*/, const uint16_t size)
{
    // Punkt 5: SavePower=Nein → Slot ignorieren, SummerActiveInit anwenden.
    if (!ParamLMG_LMGSummerActiveSavePower)
    {
        applySummerActiveInit();
        return;
    }
    if (size < 3) { applySummerActiveInit(); return; }
    const uint8_t version = openknx.flash.readByte();
    if (version != LMG_SUMMER_FLASH_VERSION)
    {
        // Magic-Mismatch → Init-Pfad. Slot wird beim naechsten writeFlash neu geschrieben.
        applySummerActiveInit();
        return;
    }

    const uint8_t hi = openknx.flash.readByte();
    const uint8_t lo = openknx.flash.readByte();
    const uint16_t mask = (static_cast<uint16_t>(hi) << 8) | lo;

    for (auto& ch : _channels)
        if (ch) ch->restoreSummerFromMask(mask);
}

void LightManagerModule::applySummerActiveInit()
{
    // Plan Phase 2 Step 4a: Init beim Boot wenn kein NVS-Restore.
    const uint8_t init = ParamLMG_LMGSummerActiveInit;
    for (auto& ch : _channels)
    {
        if (!ch) continue;
        HCL::Master* m = ch->masterPtr();
        if (!m) continue;
        switch (static_cast<HCL::SummerActiveInit>(init))
        {
            case HCL::SummerActiveInit::Winter:      m->setIsSummer(false); break;
            case HCL::SummerActiveInit::Summer:      m->setIsSummer(true);  break;
            case HCL::SummerActiveInit::ReadFromBus: /* TODO: send read request */ break;
            case HCL::SummerActiveInit::Nothing:
            default:                                 /* unchanged (Default false) */ break;
        }
    }
}

// ---------------------------------------------------------------------------
// HCL::IMasterProvider implementation
// ---------------------------------------------------------------------------

HCL::Master* LightManagerModule::providerGetMaster(uint8_t masterNum)
{
    LightManagerChannel* ch = channel(masterNum);
    return ch ? ch->masterPtr() : nullptr;
}

HCL::InterpolatedValue LightManagerModule::providerGetCurrentValue(uint8_t masterNum) const
{
    const LightManagerChannel* ch = channel(masterNum);
    return ch ? ch->currentValue() : HCL::InterpolatedValue{};
}

void LightManagerModule::providerSetCurrentValue(uint8_t masterNum, const HCL::InterpolatedValue& value)
{
    LightManagerChannel* ch = channel(masterNum);
    if (ch) ch->setCurrentValue(value);
}

bool LightManagerModule::providerIsMasterApplyBlocked(uint8_t masterNum) const
{
    const LightManagerChannel* ch = channel(masterNum);
    return ch ? ch->applyBlocked() : false;
}

void LightManagerModule::providerSetMasterApplyBlocked(uint8_t masterNum, bool blocked)
{
    LightManagerChannel* ch = channel(masterNum);
    if (ch) ch->setApplyBlocked(blocked);
}

bool LightManagerModule::providerIsMasterAdaptiveActive(uint8_t masterNum, uint16_t currentTimeMinutes, uint32_t nowMs) const
{
    const LightManagerChannel* ch = channel(masterNum);
    return ch ? ch->isAdaptiveActive(currentTimeMinutes, nowMs) : false;
}
