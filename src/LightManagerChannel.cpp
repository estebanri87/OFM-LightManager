#include "LightManagerChannel.h"
#include "knxprod.h"

namespace
{

bool isDateInSummerRange(uint8_t month, uint8_t day,
                         uint8_t startMonth, uint8_t startDay,
                         uint8_t endMonth, uint8_t endDay)
{
    if (startMonth == endMonth && startDay == endDay)
        return false;
    const uint16_t current = static_cast<uint16_t>(month) * 32u + day;
    const uint16_t start   = static_cast<uint16_t>(startMonth) * 32u + startDay;
    const uint16_t end     = static_cast<uint16_t>(endMonth) * 32u + endDay;
    if (end > start)
        return current >= start && current <= end;
    return current >= start || current <= end;
}

} // namespace

LightManagerChannel::LightManagerChannel(uint8_t channelIndex)
{
    _channelIndex = channelIndex;
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void LightManagerChannel::setupHcl(float latitudeDeg, float longitudeDeg, int16_t timezoneOffsetMin)
{
    loadSetpoints();

    if (ParamLMG_CHSeasonMode != 0)
        loadSummerSetpoints();

    applyAdvanced(latitudeDeg, longitudeDeg, timezoneOffsetMin);
    loadAdaptive();
    loadLockFallbackParams();
}

void LightManagerChannel::loadSetpoints()
{
    HCL::Master* m = master();
    if (!m) return;

    struct Sp { uint16_t timeMinutes; uint16_t kelvin; uint8_t brightness; bool active; };
    const Sp sps[10] = {
        {ParamLMG_CHSP0Time, ParamLMG_CHSP0Kelvin, ParamLMG_CHSP0Brightness, ParamLMG_CHSP0Active != 0},
        {ParamLMG_CHSP1Time, ParamLMG_CHSP1Kelvin, ParamLMG_CHSP1Brightness, ParamLMG_CHSP1Active != 0},
        {ParamLMG_CHSP2Time, ParamLMG_CHSP2Kelvin, ParamLMG_CHSP2Brightness, ParamLMG_CHSP2Active != 0},
        {ParamLMG_CHSP3Time, ParamLMG_CHSP3Kelvin, ParamLMG_CHSP3Brightness, ParamLMG_CHSP3Active != 0},
        {ParamLMG_CHSP4Time, ParamLMG_CHSP4Kelvin, ParamLMG_CHSP4Brightness, ParamLMG_CHSP4Active != 0},
        {ParamLMG_CHSP5Time, ParamLMG_CHSP5Kelvin, ParamLMG_CHSP5Brightness, ParamLMG_CHSP5Active != 0},
        {ParamLMG_CHSP6Time, ParamLMG_CHSP6Kelvin, ParamLMG_CHSP6Brightness, ParamLMG_CHSP6Active != 0},
        {ParamLMG_CHSP7Time, ParamLMG_CHSP7Kelvin, ParamLMG_CHSP7Brightness, ParamLMG_CHSP7Active != 0},
        {ParamLMG_CHSP8Time, ParamLMG_CHSP8Kelvin, ParamLMG_CHSP8Brightness, ParamLMG_CHSP8Active != 0},
        {ParamLMG_CHSP9Time, ParamLMG_CHSP9Kelvin, ParamLMG_CHSP9Brightness, ParamLMG_CHSP9Active != 0},
    };

    for (int i = 0; i < 10; i++)
        m->setSetpoint(i, HCL::Setpoint(0xFFFF, 4000, 100));

    for (int i = 0; i < 10; i++)
    {
        if (!sps[i].active) continue;
        m->setSetpoint(i, HCL::Setpoint(sps[i].timeMinutes, sps[i].kelvin, sps[i].brightness));
    }

    m->sortSetpoints();
}

void LightManagerChannel::loadSummerSetpoints()
{
    HCL::Master* m = master();
    if (!m) return;

    struct Sp { uint16_t timeMinutes; uint16_t kelvin; uint8_t brightness; bool active; };
    const Sp sps[10] = {
        {ParamLMG_CHSP0Time, ParamLMG_CHSP0SummerKelvin, ParamLMG_CHSP0SummerBrightness, ParamLMG_CHSP0Active != 0},
        {ParamLMG_CHSP1Time, ParamLMG_CHSP1SummerKelvin, ParamLMG_CHSP1SummerBrightness, ParamLMG_CHSP1Active != 0},
        {ParamLMG_CHSP2Time, ParamLMG_CHSP2SummerKelvin, ParamLMG_CHSP2SummerBrightness, ParamLMG_CHSP2Active != 0},
        {ParamLMG_CHSP3Time, ParamLMG_CHSP3SummerKelvin, ParamLMG_CHSP3SummerBrightness, ParamLMG_CHSP3Active != 0},
        {ParamLMG_CHSP4Time, ParamLMG_CHSP4SummerKelvin, ParamLMG_CHSP4SummerBrightness, ParamLMG_CHSP4Active != 0},
        {ParamLMG_CHSP5Time, ParamLMG_CHSP5SummerKelvin, ParamLMG_CHSP5SummerBrightness, ParamLMG_CHSP5Active != 0},
        {ParamLMG_CHSP6Time, ParamLMG_CHSP6SummerKelvin, ParamLMG_CHSP6SummerBrightness, ParamLMG_CHSP6Active != 0},
        {ParamLMG_CHSP7Time, ParamLMG_CHSP7SummerKelvin, ParamLMG_CHSP7SummerBrightness, ParamLMG_CHSP7Active != 0},
        {ParamLMG_CHSP8Time, ParamLMG_CHSP8SummerKelvin, ParamLMG_CHSP8SummerBrightness, ParamLMG_CHSP8Active != 0},
        {ParamLMG_CHSP9Time, ParamLMG_CHSP9SummerKelvin, ParamLMG_CHSP9SummerBrightness, ParamLMG_CHSP9Active != 0},
    };

    for (int i = 0; i < 10; i++)
    {
        if (!sps[i].active)
        {
            m->setSummerSetpoint(i, HCL::Setpoint(0xFFFF, 4000, 100));
            continue;
        }
        m->setSummerSetpoint(i, HCL::Setpoint(sps[i].timeMinutes, sps[i].kelvin, sps[i].brightness));
    }
    m->sortSummerSetpoints();
}

void LightManagerChannel::applyAdvanced(float latitudeDeg, float longitudeDeg, int16_t timezoneOffsetMin)
{
    HCL::Master* m = master();
    if (!m) return;

    uint8_t curveType = ParamLMG_CHCurveType;
    if (curveType > static_cast<uint8_t>(HCL::CurveType::Astronomical))
        curveType = static_cast<uint8_t>(HCL::CurveType::FixedTime);

    m->setCurveType(static_cast<HCL::CurveType>(curveType));
    m->setSlewRateKelvinPerMinute(ParamLMG_CHSlewRate);
    m->setManualKelvin(ParamLMG_CHManualKelvin);
    m->setLocation(latitudeDeg, longitudeDeg);
    m->setTimezoneOffsetMinutes(timezoneOffsetMin);
    m->setAstronomicalProfile(ParamLMG_CHAstroMinKelvin, ParamLMG_CHAstroMaxKelvin,
                              ParamLMG_CHAstroMinBrightness, ParamLMG_CHAstroMaxBrightness);

    m->setSunTimes(ParamLMG_CHSunrise, ParamLMG_CHSunset);

    m->setSunOffsets(static_cast<int16_t>(ParamLMG_CHSunriseOffset),
                     static_cast<int16_t>(ParamLMG_CHSunsetOffset));
}

void LightManagerChannel::loadAdaptive()
{
    HCL::Master* m = master();
    if (!m) return;

    HCL::AdaptiveConfig cfg;
    cfg.mode                = static_cast<HCL::AdaptiveMode>(ParamLMG_CHAdaptiveMode);
    cfg.activeMode          = static_cast<HCL::AdaptiveActiveMode>(ParamLMG_CHAdaptiveActiveMode);
    cfg.ceilToHCL           = (ParamLMG_CHAdaptiveCeilToHCL != 0);
    cfg.maxLux              = ParamLMG_CHAdaptiveMaxLux;
    cfg.minBrightness       = ParamLMG_CHAdaptiveMinBrightness;
    cfg.sensorTimeoutMinutes= ParamLMG_CHAdaptiveSensorTimeout;
    cfg.minChangePercent    = ParamLMG_CHAdaptiveMinChange;
    cfg.strength            = ParamLMG_CHAdaptiveStrength;

    static const float kpValues[] = {0.5f, 1.0f, 1.5f, 2.0f};
    const uint8_t kpEnum = ParamLMG_CHAdaptiveKp;
    cfg.kp = (kpEnum < 4) ? kpValues[kpEnum] : 1.0f;

    cfg.deadbandLux = ParamLMG_CHAdaptiveDeadband;

    cfg.activeStartMinutes = ParamLMG_CHAdaptiveStartTime;
    cfg.activeEndMinutes   = ParamLMG_CHAdaptiveEndTime;

    cfg.dayNightPolarity = (ParamLMG_CHAdaptiveDayNightPolarity != 0);
    m->setAdaptiveConfig(cfg);
}

void LightManagerChannel::loadLockFallbackParams()
{
    _lockFallbackMode   = ParamLMG_CHLockFallback;
    _lockFallbackPolicy = ParamLMG_CHFallbackPolicy;
    if (_lockFallbackPolicy > static_cast<uint8_t>(LockFallbackPolicy::Disabled))
        _lockFallbackPolicy = static_cast<uint8_t>(LockFallbackPolicy::Legacy);

    const uint64_t durMs = static_cast<uint64_t>(ParamLMG_CHFallbackDurationSec) * 1000ULL;
    _lockFallbackDurationMs = (durMs > 0xFFFFFFFFULL) ? 0xFFFFFFFFUL : static_cast<uint32_t>(durMs);

    _lockFallbackReleaseMinuteOfDay = ParamLMG_CHFallbackReleaseTime;
}

// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------

void LightManagerChannel::loopHcl(const tm* timeinfo, bool hasTime)
{
    if (hasTime && timeinfo != nullptr)
        updateSeason(timeinfo);

    evaluateLockFallback(timeinfo, hasTime);
}

void LightManagerChannel::updateSeason(const tm* timeinfo)
{
    HCL::Master* m = master();
    if (!m) return;

    const uint8_t seasonMode = ParamLMG_CHSeasonMode;
    if (seasonMode == 0)
    {
        m->setIsSummer(false);
    }
    else if (seasonMode == 1)
    {
        const int8_t dstOffsetDays = static_cast<int8_t>(ParamLMG_CHDSTOffsetDays);
        if (dstOffsetDays != 0)
        {
            struct tm timeCopy = *timeinfo;
            const time_t shifted = mktime(&timeCopy) - (static_cast<int64_t>(dstOffsetDays) * 86400LL);
            struct tm shiftedTm;
            localtime_r(&shifted, &shiftedTm);
            m->setIsSummer(shiftedTm.tm_isdst == 1);
        }
        else
        {
            m->setIsSummer(timeinfo->tm_isdst == 1);
        }
    }
    else if (seasonMode == 2)
    {
        m->setIsSummer(isDateInSummerRange(
            static_cast<uint8_t>(timeinfo->tm_mon + 1), static_cast<uint8_t>(timeinfo->tm_mday),
            ParamLMG_CHSummerStartMonth, ParamLMG_CHSummerStartDay,
            ParamLMG_CHSummerEndMonth, ParamLMG_CHSummerEndDay));
    }
    // seasonMode == 3 → per KO, set externally
}

void LightManagerChannel::pushIfChanged()
{
    const bool blocked = HCL::masterManager.isApplyBlocked() || _applyBlocked;

    if (blocked)
    {
        _lastBlocked = true;
        return;
    }

    const bool wasBlocked = _lastBlocked;
    _lastBlocked = false;

    const HCL::InterpolatedValue val = _currentValue;
    if (!wasBlocked &&
        val.kelvin == _lastPushedValue.kelvin &&
        val.brightness == _lastPushedValue.brightness)
    {
        return;
    }
    _lastPushedValue = val;

    knx.getGroupObject(LMG_KoCalcNumber(LMG_KoCHStatusBrightness)).value(val.brightness, Dpt(5, 1));
    knx.getGroupObject(LMG_KoCalcNumber(LMG_KoCHStatusColorTemp)).value(static_cast<uint16_t>(val.kelvin), Dpt(7, 600));
}

// ---------------------------------------------------------------------------
// KO routing
// ---------------------------------------------------------------------------

bool LightManagerChannel::processChannelKo(GroupObject& ko, uint16_t channelKoIndex)
{
    switch (channelKoIndex)
    {
        case LMG_KoCHLock:
            setLock(ko.value(Dpt(1, 1)), "KO");
            return true;

        case LMG_KoCHSummerActive:
        {
            HCL::Master* m = master();
            if (m)
            {
                m->setIsSummer(ko.value(Dpt(1, 1)));
            }
            return true;
        }

        case LMG_KoCHAmbientLux:
        {
            const float lux = ko.value(Dpt(9, 4));
            _master.setAmbientLux(lux);
            HCL::masterManager.forceUpdate();
            publishAdaptiveActive();
            return true;
        }

        case LMG_KoCHDayNight:
            _master.setDaytime(ko.value(Dpt(1, 1)));
            return true;

        default:
            return false;
    }
}

// ---------------------------------------------------------------------------
// Lock state machine
// ---------------------------------------------------------------------------

void LightManagerChannel::setLock(bool active, const char* reason)
{
    if (active && static_cast<LockFallbackPolicy>(_lockFallbackPolicy) == LockFallbackPolicy::Disabled)
        return;

    const bool changed = (_lockActive != active);
    _lockActive = active;
    _applyBlocked = active;

    if (active)
    {
        _lockActivatedMs = millis();
        _lockAutoReleaseMs = 0;
        _lockActivationDayOfYear = -1;
        _lockActivationMinuteOfDay = -1;

        const LockFallbackPolicy policy = static_cast<LockFallbackPolicy>(_lockFallbackPolicy);
        if (policy == LockFallbackPolicy::Legacy)
        {
            const uint32_t d = fallbackDurationMs(static_cast<LockFallbackMode>(_lockFallbackMode));
            if (d > 0) _lockAutoReleaseMs = _lockActivatedMs + d;
        }
        else if (policy == LockFallbackPolicy::Duration || policy == LockFallbackPolicy::DurationOrTime)
        {
            if (_lockFallbackDurationMs > 0)
                _lockAutoReleaseMs = _lockActivatedMs + _lockFallbackDurationMs;
        }

        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 0))
        {
            _lockActivationDayOfYear   = static_cast<int16_t>(timeinfo.tm_yday);
            _lockActivationMinuteOfDay = static_cast<int16_t>(timeinfo.tm_hour * 60 + timeinfo.tm_min);
        }

        if (changed)
            Serial.printf("[LMG ch%u] lock enabled (%s)\n", (unsigned)masterNumber(), reason ? reason : "n/a");
    }
    else
    {
        _lockActivatedMs = 0;
        _lockAutoReleaseMs = 0;
        _lockActivationDayOfYear = -1;
        _lockActivationMinuteOfDay = -1;
        if (changed)
            Serial.printf("[LMG ch%u] lock disabled (%s)\n", (unsigned)masterNumber(), reason ? reason : "n/a");
    }

    publishLockStatus();
}

void LightManagerChannel::evaluateLockFallback(const tm* timeinfo, bool hasTime)
{
    if (!_lockActive) return;

    const LockFallbackPolicy policy = static_cast<LockFallbackPolicy>(_lockFallbackPolicy);
    if (policy == LockFallbackPolicy::ExternalOnly) return;

    if (policy != LockFallbackPolicy::Legacy)
    {
        const bool byDuration = (_lockAutoReleaseMs != 0) &&
                                (static_cast<long>(millis() - _lockAutoReleaseMs) >= 0);
        const bool byTime = (policy == LockFallbackPolicy::TimeOfDay || policy == LockFallbackPolicy::DurationOrTime) &&
                            shouldReleaseByPolicyTime(timeinfo, hasTime);
        if (byDuration || byTime)
            setLock(false, byTime ? "fallback release time" : "fallback duration elapsed");
        return;
    }

    // Legacy
    const LockFallbackMode mode = static_cast<LockFallbackMode>(_lockFallbackMode);
    if (mode == LockFallbackMode::None) return;

    if (_lockAutoReleaseMs != 0)
    {
        if (static_cast<long>(millis() - _lockAutoReleaseMs) >= 0)
            setLock(false, "fallback duration elapsed");
        return;
    }

    if (mode == LockFallbackMode::NextDay && hasTime && timeinfo != nullptr)
    {
        if (_lockActivationDayOfYear >= 0 && timeinfo->tm_yday != _lockActivationDayOfYear)
            setLock(false, "fallback day change");
    }
}

uint32_t LightManagerChannel::fallbackDurationMs(LockFallbackMode mode) const
{
    switch (mode)
    {
        case LockFallbackMode::Min1:   return   1UL * 60UL * 1000UL;
        case LockFallbackMode::Min2:   return   2UL * 60UL * 1000UL;
        case LockFallbackMode::Min5:   return   5UL * 60UL * 1000UL;
        case LockFallbackMode::Min10:  return  10UL * 60UL * 1000UL;
        case LockFallbackMode::Min20:  return  20UL * 60UL * 1000UL;
        case LockFallbackMode::Min30:  return  30UL * 60UL * 1000UL;
        case LockFallbackMode::Hour1:  return   1UL * 60UL * 60UL * 1000UL;
        case LockFallbackMode::Hour2:  return   2UL * 60UL * 60UL * 1000UL;
        case LockFallbackMode::Hour5:  return   5UL * 60UL * 60UL * 1000UL;
        case LockFallbackMode::Hour8:  return   8UL * 60UL * 60UL * 1000UL;
        case LockFallbackMode::Hour12: return  12UL * 60UL * 60UL * 1000UL;
        default: return 0;
    }
}

bool LightManagerChannel::shouldReleaseByPolicyTime(const tm* timeinfo, bool hasTime) const
{
    if (!hasTime || timeinfo == nullptr || _lockFallbackReleaseMinuteOfDay == 0xFFFF) return false;
    if (_lockActivationDayOfYear < 0 || _lockActivationMinuteOfDay < 0) return false;

    const int16_t currentDay = static_cast<int16_t>(timeinfo->tm_yday);
    const int16_t currentMin = static_cast<int16_t>(timeinfo->tm_hour * 60 + timeinfo->tm_min);

    if (currentDay < _lockActivationDayOfYear) return false;
    if (currentDay == _lockActivationDayOfYear)
    {
        if (_lockFallbackReleaseMinuteOfDay <= static_cast<uint16_t>(_lockActivationMinuteOfDay)) return false;
        return currentMin >= static_cast<int16_t>(_lockFallbackReleaseMinuteOfDay);
    }
    return currentMin >= static_cast<int16_t>(_lockFallbackReleaseMinuteOfDay);
}

void LightManagerChannel::publishLockStatus()
{
    knx.getGroupObject(LMG_KoCalcNumber(LMG_KoCHLockStatus)).value(_lockActive, Dpt(1, 1));
}

void LightManagerChannel::publishAdaptiveActive()
{
    const bool active = HCL::masterManager.isMasterAdaptiveActive(masterNumber());
    knx.getGroupObject(LMG_KoCalcNumber(LMG_KoCHAdaptiveActive)).value(active, Dpt(1, 11));
}

// ---------------------------------------------------------------------------
// Summer persistence
// ---------------------------------------------------------------------------

bool LightManagerChannel::isSummer() const
{
    const HCL::Master* m = master();
    return m && m->isSummer();
}

void LightManagerChannel::restoreSummerFromMask(uint16_t mask)
{
    if (ParamLMG_CHSeasonMode != 3) return; // only "per KO" persists
    HCL::Master* m = master();
    if (!m) return;
    const bool wasSummer = (mask & static_cast<uint16_t>(1u << _channelIndex)) != 0;
    m->setIsSummer(wasSummer);
}
