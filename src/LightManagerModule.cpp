#include "LightManagerModule.h"

namespace {

String readFixedTimeParam(const uint8_t* rawTime)
{
    if (rawTime == nullptr || rawTime[0] == '\0')
    {
        return String("");
    }

    char buffer[6] = {0, 0, 0, 0, 0, 0};
    memcpy(buffer, rawTime, 5);
    return String(buffer);
}

bool isDateInSummerRange(uint8_t month, uint8_t day,
                         uint8_t startMonth, uint8_t startDay,
                         uint8_t endMonth, uint8_t endDay)
{
    if (startMonth == endMonth && startDay == endDay)
    {
        return false;
    }

    const uint16_t current = static_cast<uint16_t>(month) * 32u + day;
    const uint16_t start = static_cast<uint16_t>(startMonth) * 32u + startDay;
    const uint16_t end = static_cast<uint16_t>(endMonth) * 32u + endDay;

    if (end > start)
    {
        return current >= start && current <= end;
    }

    return current >= start || current <= end;
}

int16_t decodeBaseTimezoneOffsetMinutes(uint8_t timezoneRaw)
{
    if (timezoneRaw == 31)
    {
        return 60;
    }
    if (timezoneRaw >= 17 && timezoneRaw <= 27)
    {
        return static_cast<int16_t>((static_cast<int16_t>(timezoneRaw) - 28) * 60);
    }
    if (timezoneRaw == 28)
    {
        return 60;
    }
    if (timezoneRaw <= 12)
    {
        return static_cast<int16_t>(timezoneRaw * 60);
    }
    return 60;
}

} // namespace

LightManagerModule::LightManagerModule()
{
    memset(_lastBlockedState, 0, sizeof(_lastBlockedState));
    memset(_lastPushedValues, 0, sizeof(_lastPushedValues));

    _hclLockActive = false;
    _hclLockFallbackMode = static_cast<uint8_t>(HclLockFallbackMode::None);
    _hclFallbackPolicy = static_cast<uint8_t>(HclLockFallbackPolicy::Legacy);
    _hclFallbackDurationMs = 0;
    _hclFallbackReleaseMinuteOfDay = 0xFFFF;
    _hclLockActivatedMs = 0;
    _hclLockAutoReleaseMs = 0;
    _hclLockActivationDayOfYear = -1;
    _hclLockActivationMinuteOfDay = -1;

    for (uint8_t i = 0; i < HCL::MasterManager::MAX_MASTERS; i++)
    {
        _hclManagerLockActive[i] = false;
        _hclManagerLockFallbackMode[i] = static_cast<uint8_t>(HclLockFallbackMode::None);
        _hclManagerFallbackPolicy[i] = static_cast<uint8_t>(HclLockFallbackPolicy::Legacy);
        _hclManagerFallbackDurationMs[i] = 0;
        _hclManagerFallbackReleaseMinuteOfDay[i] = 0xFFFF;
        _hclManagerLockActivatedMs[i] = 0;
        _hclManagerLockAutoReleaseMs[i] = 0;
        _hclManagerLockActivationDayOfYear[i] = -1;
        _hclManagerLockActivationMinuteOfDay[i] = -1;
    }
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
    // Reset all locks on startup
    setHclLock(false, "setup");
    for (uint8_t m = 1; m <= HCL::MasterManager::MAX_MASTERS; m++)
        setHclManagerLock(m, false, "setup");

    setupHclFromParams();
    HCL::masterManager.setup();
}

void LightManagerModule::loop()
{
    // Provide current time to HCL master manager and lock fallback evaluation
    struct tm timeinfo;
    const bool hasTime = getLocalTime(&timeinfo, 0);
    uint16_t timeMinutes = 0;
    int16_t dayOfYear = -1;
    if (hasTime)
    {
        timeMinutes = static_cast<uint16_t>(timeinfo.tm_hour * 60 + timeinfo.tm_min);
        dayOfYear = static_cast<int16_t>(timeinfo.tm_yday + 1);

        auto updateMasterSeason = [&](uint8_t masterNumber, uint8_t seasonMode, int8_t dstOffsetDays,
                          uint8_t summerStartMonth, uint8_t summerStartDay,
                          uint8_t summerEndMonth, uint8_t summerEndDay) {
            HCL::Master* master = HCL::masterManager.getMaster(masterNumber);
            if (!master)
            {
            return;
            }
            if (seasonMode == 0)
            {
            master->setIsSummer(false);
            }
            else if (seasonMode == 1)
            {
            if (dstOffsetDays != 0)
            {
                struct tm timeCopy = timeinfo;
                time_t shifted = mktime(&timeCopy) - (static_cast<int64_t>(dstOffsetDays) * 86400LL);
                struct tm shiftedTm;
                localtime_r(&shifted, &shiftedTm);
                master->setIsSummer(shiftedTm.tm_isdst == 1);
            }
            else
            {
                master->setIsSummer(timeinfo.tm_isdst == 1);
            }
            }
            else if (seasonMode == 2)
            {
            master->setIsSummer(isDateInSummerRange(
                static_cast<uint8_t>(timeinfo.tm_mon + 1), static_cast<uint8_t>(timeinfo.tm_mday),
                summerStartMonth, summerStartDay, summerEndMonth, summerEndDay));
            }
        };

    #if defined(ParamLMG_HCLM1SeasonMode) && defined(ParamLMG_HCLM1SummerStartMonth)
        updateMasterSeason(1, ParamLMG_HCLM1SeasonMode,
    #ifdef ParamLMG_HCLM1DSTOffsetDays
            static_cast<int8_t>(ParamLMG_HCLM1DSTOffsetDays),
    #else
            0,
    #endif
            ParamLMG_HCLM1SummerStartMonth, ParamLMG_HCLM1SummerStartDay,
            ParamLMG_HCLM1SummerEndMonth, ParamLMG_HCLM1SummerEndDay);
    #endif
    #if defined(ParamLMG_HCLM2SeasonMode) && defined(ParamLMG_HCLM2SummerStartMonth)
        updateMasterSeason(2, ParamLMG_HCLM2SeasonMode,
    #ifdef ParamLMG_HCLM2DSTOffsetDays
            static_cast<int8_t>(ParamLMG_HCLM2DSTOffsetDays),
    #else
            0,
    #endif
            ParamLMG_HCLM2SummerStartMonth, ParamLMG_HCLM2SummerStartDay,
            ParamLMG_HCLM2SummerEndMonth, ParamLMG_HCLM2SummerEndDay);
    #endif
    #if defined(ParamLMG_HCLM3SeasonMode) && defined(ParamLMG_HCLM3SummerStartMonth)
        updateMasterSeason(3, ParamLMG_HCLM3SeasonMode,
    #ifdef ParamLMG_HCLM3DSTOffsetDays
            static_cast<int8_t>(ParamLMG_HCLM3DSTOffsetDays),
    #else
            0,
    #endif
            ParamLMG_HCLM3SummerStartMonth, ParamLMG_HCLM3SummerStartDay,
            ParamLMG_HCLM3SummerEndMonth, ParamLMG_HCLM3SummerEndDay);
    #endif
    #if defined(ParamLMG_HCLM4SeasonMode) && defined(ParamLMG_HCLM4SummerStartMonth)
        updateMasterSeason(4, ParamLMG_HCLM4SeasonMode,
    #ifdef ParamLMG_HCLM4DSTOffsetDays
            static_cast<int8_t>(ParamLMG_HCLM4DSTOffsetDays),
    #else
            0,
    #endif
            ParamLMG_HCLM4SummerStartMonth, ParamLMG_HCLM4SummerStartDay,
            ParamLMG_HCLM4SummerEndMonth, ParamLMG_HCLM4SummerEndDay);
    #endif
    #if defined(ParamLMG_HCLM5SeasonMode) && defined(ParamLMG_HCLM5SummerStartMonth)
        updateMasterSeason(5, ParamLMG_HCLM5SeasonMode,
    #ifdef ParamLMG_HCLM5DSTOffsetDays
            static_cast<int8_t>(ParamLMG_HCLM5DSTOffsetDays),
    #else
            0,
    #endif
            ParamLMG_HCLM5SummerStartMonth, ParamLMG_HCLM5SummerStartDay,
            ParamLMG_HCLM5SummerEndMonth, ParamLMG_HCLM5SummerEndDay);
    #endif
    #if defined(ParamLMG_HCLM6SeasonMode) && defined(ParamLMG_HCLM6SummerStartMonth)
        updateMasterSeason(6, ParamLMG_HCLM6SeasonMode,
    #ifdef ParamLMG_HCLM6DSTOffsetDays
            static_cast<int8_t>(ParamLMG_HCLM6DSTOffsetDays),
    #else
            0,
    #endif
            ParamLMG_HCLM6SummerStartMonth, ParamLMG_HCLM6SummerStartDay,
            ParamLMG_HCLM6SummerEndMonth, ParamLMG_HCLM6SummerEndDay);
    #endif
    #if defined(ParamLMG_HCLM7SeasonMode) && defined(ParamLMG_HCLM7SummerStartMonth)
        updateMasterSeason(7, ParamLMG_HCLM7SeasonMode,
    #ifdef ParamLMG_HCLM7DSTOffsetDays
            static_cast<int8_t>(ParamLMG_HCLM7DSTOffsetDays),
    #else
            0,
    #endif
            ParamLMG_HCLM7SummerStartMonth, ParamLMG_HCLM7SummerStartDay,
            ParamLMG_HCLM7SummerEndMonth, ParamLMG_HCLM7SummerEndDay);
    #endif
    #if defined(ParamLMG_HCLM8SeasonMode) && defined(ParamLMG_HCLM8SummerStartMonth)
        updateMasterSeason(8, ParamLMG_HCLM8SeasonMode,
    #ifdef ParamLMG_HCLM8DSTOffsetDays
            static_cast<int8_t>(ParamLMG_HCLM8DSTOffsetDays),
    #else
            0,
    #endif
            ParamLMG_HCLM8SummerStartMonth, ParamLMG_HCLM8SummerStartDay,
            ParamLMG_HCLM8SummerEndMonth, ParamLMG_HCLM8SummerEndDay);
    #endif
    #if defined(ParamLMG_HCLM9SeasonMode) && defined(ParamLMG_HCLM9SummerStartMonth)
        updateMasterSeason(9, ParamLMG_HCLM9SeasonMode,
    #ifdef ParamLMG_HCLM9DSTOffsetDays
            static_cast<int8_t>(ParamLMG_HCLM9DSTOffsetDays),
    #else
            0,
    #endif
            ParamLMG_HCLM9SummerStartMonth, ParamLMG_HCLM9SummerStartDay,
            ParamLMG_HCLM9SummerEndMonth, ParamLMG_HCLM9SummerEndDay);
    #endif
    #if defined(ParamLMG_HCLM10SeasonMode) && defined(ParamLMG_HCLM10SummerStartMonth)
        updateMasterSeason(10, ParamLMG_HCLM10SeasonMode,
    #ifdef ParamLMG_HCLM10DSTOffsetDays
            static_cast<int8_t>(ParamLMG_HCLM10DSTOffsetDays),
    #else
            0,
    #endif
            ParamLMG_HCLM10SummerStartMonth, ParamLMG_HCLM10SummerStartDay,
            ParamLMG_HCLM10SummerEndMonth, ParamLMG_HCLM10SummerEndDay);
    #endif
    #if defined(ParamLMG_HCLM11SeasonMode) && defined(ParamLMG_HCLM11SummerStartMonth)
        updateMasterSeason(11, ParamLMG_HCLM11SeasonMode,
    #ifdef ParamLMG_HCLM11DSTOffsetDays
            static_cast<int8_t>(ParamLMG_HCLM11DSTOffsetDays),
    #else
            0,
    #endif
            ParamLMG_HCLM11SummerStartMonth, ParamLMG_HCLM11SummerStartDay,
            ParamLMG_HCLM11SummerEndMonth, ParamLMG_HCLM11SummerEndDay);
    #endif
    #if defined(ParamLMG_HCLM12SeasonMode) && defined(ParamLMG_HCLM12SummerStartMonth)
        updateMasterSeason(12, ParamLMG_HCLM12SeasonMode,
    #ifdef ParamLMG_HCLM12DSTOffsetDays
            static_cast<int8_t>(ParamLMG_HCLM12DSTOffsetDays),
    #else
            0,
    #endif
            ParamLMG_HCLM12SummerStartMonth, ParamLMG_HCLM12SummerStartDay,
            ParamLMG_HCLM12SummerEndMonth, ParamLMG_HCLM12SummerEndDay);
    #endif
    #if defined(ParamLMG_HCLM13SeasonMode) && defined(ParamLMG_HCLM13SummerStartMonth)
        updateMasterSeason(13, ParamLMG_HCLM13SeasonMode,
    #ifdef ParamLMG_HCLM13DSTOffsetDays
            static_cast<int8_t>(ParamLMG_HCLM13DSTOffsetDays),
    #else
            0,
    #endif
            ParamLMG_HCLM13SummerStartMonth, ParamLMG_HCLM13SummerStartDay,
            ParamLMG_HCLM13SummerEndMonth, ParamLMG_HCLM13SummerEndDay);
    #endif
    #if defined(ParamLMG_HCLM14SeasonMode) && defined(ParamLMG_HCLM14SummerStartMonth)
        updateMasterSeason(14, ParamLMG_HCLM14SeasonMode,
    #ifdef ParamLMG_HCLM14DSTOffsetDays
            static_cast<int8_t>(ParamLMG_HCLM14DSTOffsetDays),
    #else
            0,
    #endif
            ParamLMG_HCLM14SummerStartMonth, ParamLMG_HCLM14SummerStartDay,
            ParamLMG_HCLM14SummerEndMonth, ParamLMG_HCLM14SummerEndDay);
    #endif
    #if defined(ParamLMG_HCLM15SeasonMode) && defined(ParamLMG_HCLM15SummerStartMonth)
        updateMasterSeason(15, ParamLMG_HCLM15SeasonMode,
    #ifdef ParamLMG_HCLM15DSTOffsetDays
            static_cast<int8_t>(ParamLMG_HCLM15DSTOffsetDays),
    #else
            0,
    #endif
            ParamLMG_HCLM15SummerStartMonth, ParamLMG_HCLM15SummerStartDay,
            ParamLMG_HCLM15SummerEndMonth, ParamLMG_HCLM15SummerEndDay);
    #endif
    #if defined(ParamLMG_HCLM16SeasonMode) && defined(ParamLMG_HCLM16SummerStartMonth)
        updateMasterSeason(16, ParamLMG_HCLM16SeasonMode,
    #ifdef ParamLMG_HCLM16DSTOffsetDays
            static_cast<int8_t>(ParamLMG_HCLM16DSTOffsetDays),
    #else
            0,
    #endif
            ParamLMG_HCLM16SummerStartMonth, ParamLMG_HCLM16SummerStartDay,
            ParamLMG_HCLM16SummerEndMonth, ParamLMG_HCLM16SummerEndDay);
    #endif
    }

    HCL::masterManager.loop(timeMinutes, dayOfYear);

    // Evaluate lock fallback timers
    evaluateHclLockFallback(hasTime ? &timeinfo : nullptr, hasTime);
    evaluateHclManagerLockFallback(hasTime ? &timeinfo : nullptr, hasTime);

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
    const uint16_t koNumber = ko.asap();

    // Global HCL lock
    #ifdef LMG_KoLMGHCLLock
    if (koNumber == LMG_KoLMGHCLLock)
    {
        setHclLock(ko.value(Dpt(1, 1)), "KO");
        return;
    }
    #endif

    // Global release trigger
    #ifdef LMG_KoLMGHCLReleaseTrigger
    if (koNumber == LMG_KoLMGHCLReleaseTrigger)
    {
        if (ko.value(Dpt(1, 1)))
        {
            setHclLock(false, "KO release trigger");
            for (uint8_t m = 1; m <= HCL::MasterManager::MAX_MASTERS; m++)
                setHclManagerLock(m, false, "KO release trigger");
        }
        return;
    }
    #endif

    // Per-master lock
    {
        struct Entry { uint16_t ko; uint8_t master; };
        static const Entry table[] = {
#ifdef LMG_KoLMGHCLM1Lock
            { LMG_KoLMGHCLM1Lock, 1 },
#endif
#ifdef LMG_KoLMGHCLM2Lock
            { LMG_KoLMGHCLM2Lock, 2 },
#endif
#ifdef LMG_KoLMGHCLM3Lock
            { LMG_KoLMGHCLM3Lock, 3 },
#endif
#ifdef LMG_KoLMGHCLM4Lock
            { LMG_KoLMGHCLM4Lock, 4 },
#endif
#ifdef LMG_KoLMGHCLM5Lock
            { LMG_KoLMGHCLM5Lock, 5 },
#endif
#ifdef LMG_KoLMGHCLM6Lock
            { LMG_KoLMGHCLM6Lock, 6 },
#endif
#ifdef LMG_KoLMGHCLM7Lock
            { LMG_KoLMGHCLM7Lock, 7 },
#endif
#ifdef LMG_KoLMGHCLM8Lock
            { LMG_KoLMGHCLM8Lock, 8 },
#endif
#ifdef LMG_KoLMGHCLM9Lock
            { LMG_KoLMGHCLM9Lock, 9 },
#endif
#ifdef LMG_KoLMGHCLM10Lock
            { LMG_KoLMGHCLM10Lock, 10 },
#endif
#ifdef LMG_KoLMGHCLM11Lock
            { LMG_KoLMGHCLM11Lock, 11 },
#endif
#ifdef LMG_KoLMGHCLM12Lock
            { LMG_KoLMGHCLM12Lock, 12 },
#endif
#ifdef LMG_KoLMGHCLM13Lock
            { LMG_KoLMGHCLM13Lock, 13 },
#endif
#ifdef LMG_KoLMGHCLM14Lock
            { LMG_KoLMGHCLM14Lock, 14 },
#endif
#ifdef LMG_KoLMGHCLM15Lock
            { LMG_KoLMGHCLM15Lock, 15 },
#endif
#ifdef LMG_KoLMGHCLM16Lock
            { LMG_KoLMGHCLM16Lock, 16 },
#endif
            { 0, 0 }
        };
        for (size_t i = 0; table[i].master != 0; i++)
        {
            if (koNumber == table[i].ko)
            {
                setHclManagerLock(table[i].master, ko.value(Dpt(1, 1)), "KO");
                return;
            }
        }
    }

    // Per-master summer active
    {
        struct Entry { uint16_t ko; uint8_t master; };
        static const Entry table[] = {
#ifdef LMG_KoLMGHCLM1SummerActive
            { LMG_KoLMGHCLM1SummerActive, 1 },
#endif
#ifdef LMG_KoLMGHCLM2SummerActive
            { LMG_KoLMGHCLM2SummerActive, 2 },
#endif
#ifdef LMG_KoLMGHCLM3SummerActive
            { LMG_KoLMGHCLM3SummerActive, 3 },
#endif
#ifdef LMG_KoLMGHCLM4SummerActive
            { LMG_KoLMGHCLM4SummerActive, 4 },
#endif
#ifdef LMG_KoLMGHCLM5SummerActive
            { LMG_KoLMGHCLM5SummerActive, 5 },
#endif
#ifdef LMG_KoLMGHCLM6SummerActive
            { LMG_KoLMGHCLM6SummerActive, 6 },
#endif
#ifdef LMG_KoLMGHCLM7SummerActive
            { LMG_KoLMGHCLM7SummerActive, 7 },
#endif
#ifdef LMG_KoLMGHCLM8SummerActive
            { LMG_KoLMGHCLM8SummerActive, 8 },
#endif
#ifdef LMG_KoLMGHCLM9SummerActive
            { LMG_KoLMGHCLM9SummerActive, 9 },
#endif
#ifdef LMG_KoLMGHCLM10SummerActive
            { LMG_KoLMGHCLM10SummerActive, 10 },
#endif
#ifdef LMG_KoLMGHCLM11SummerActive
            { LMG_KoLMGHCLM11SummerActive, 11 },
#endif
#ifdef LMG_KoLMGHCLM12SummerActive
            { LMG_KoLMGHCLM12SummerActive, 12 },
#endif
#ifdef LMG_KoLMGHCLM13SummerActive
            { LMG_KoLMGHCLM13SummerActive, 13 },
#endif
#ifdef LMG_KoLMGHCLM14SummerActive
            { LMG_KoLMGHCLM14SummerActive, 14 },
#endif
#ifdef LMG_KoLMGHCLM15SummerActive
            { LMG_KoLMGHCLM15SummerActive, 15 },
#endif
#ifdef LMG_KoLMGHCLM16SummerActive
            { LMG_KoLMGHCLM16SummerActive, 16 },
#endif
            { 0, 0 }
        };
        for (size_t i = 0; table[i].master != 0; i++)
        {
            if (koNumber == table[i].ko)
            {
                HCL::Master* master = HCL::masterManager.getMaster(table[i].master);
                if (master)
                {
                    master->setIsSummer(ko.value(Dpt(1, 1)));
                    // Persist new season state so it survives a reboot/reprogramming
                    openknx.flash.save();
                }
                return;
            }
        }
    }

    // Per-master ambient lux (DPT 9.004)
    {
        struct Entry { uint16_t ko; uint8_t master; };
        static const Entry table[] = {
#ifdef LMG_KoLMGHCLM1AmbientLux
            { LMG_KoLMGHCLM1AmbientLux, 1 },
#endif
#ifdef LMG_KoLMGHCLM2AmbientLux
            { LMG_KoLMGHCLM2AmbientLux, 2 },
#endif
#ifdef LMG_KoLMGHCLM3AmbientLux
            { LMG_KoLMGHCLM3AmbientLux, 3 },
#endif
#ifdef LMG_KoLMGHCLM4AmbientLux
            { LMG_KoLMGHCLM4AmbientLux, 4 },
#endif
#ifdef LMG_KoLMGHCLM5AmbientLux
            { LMG_KoLMGHCLM5AmbientLux, 5 },
#endif
#ifdef LMG_KoLMGHCLM6AmbientLux
            { LMG_KoLMGHCLM6AmbientLux, 6 },
#endif
#ifdef LMG_KoLMGHCLM7AmbientLux
            { LMG_KoLMGHCLM7AmbientLux, 7 },
#endif
#ifdef LMG_KoLMGHCLM8AmbientLux
            { LMG_KoLMGHCLM8AmbientLux, 8 },
#endif
#ifdef LMG_KoLMGHCLM9AmbientLux
            { LMG_KoLMGHCLM9AmbientLux, 9 },
#endif
#ifdef LMG_KoLMGHCLM10AmbientLux
            { LMG_KoLMGHCLM10AmbientLux, 10 },
#endif
#ifdef LMG_KoLMGHCLM11AmbientLux
            { LMG_KoLMGHCLM11AmbientLux, 11 },
#endif
#ifdef LMG_KoLMGHCLM12AmbientLux
            { LMG_KoLMGHCLM12AmbientLux, 12 },
#endif
#ifdef LMG_KoLMGHCLM13AmbientLux
            { LMG_KoLMGHCLM13AmbientLux, 13 },
#endif
#ifdef LMG_KoLMGHCLM14AmbientLux
            { LMG_KoLMGHCLM14AmbientLux, 14 },
#endif
#ifdef LMG_KoLMGHCLM15AmbientLux
            { LMG_KoLMGHCLM15AmbientLux, 15 },
#endif
#ifdef LMG_KoLMGHCLM16AmbientLux
            { LMG_KoLMGHCLM16AmbientLux, 16 },
#endif
            { 0, 0 }
        };
        for (size_t i = 0; table[i].master != 0; i++)
        {
            if (koNumber == table[i].ko)
            {
                const float lux = ko.value(Dpt(9, 4));
                HCL::masterManager.setMasterAmbientLux(table[i].master, lux);
                // Send adaptive-active status KO
                const bool active = HCL::masterManager.isMasterAdaptiveActive(table[i].master);
                switch (table[i].master)
                {
#ifdef LMG_KoLMGHCLM1AdaptiveActive
                    case 1: knx.getGroupObject(LMG_KoLMGHCLM1AdaptiveActive).value(active, Dpt(1, 11)); break;
#endif
#ifdef LMG_KoLMGHCLM2AdaptiveActive
                    case 2: knx.getGroupObject(LMG_KoLMGHCLM2AdaptiveActive).value(active, Dpt(1, 11)); break;
#endif
#ifdef LMG_KoLMGHCLM3AdaptiveActive
                    case 3: knx.getGroupObject(LMG_KoLMGHCLM3AdaptiveActive).value(active, Dpt(1, 11)); break;
#endif
#ifdef LMG_KoLMGHCLM4AdaptiveActive
                    case 4: knx.getGroupObject(LMG_KoLMGHCLM4AdaptiveActive).value(active, Dpt(1, 11)); break;
#endif
#ifdef LMG_KoLMGHCLM5AdaptiveActive
                    case 5: knx.getGroupObject(LMG_KoLMGHCLM5AdaptiveActive).value(active, Dpt(1, 11)); break;
#endif
#ifdef LMG_KoLMGHCLM6AdaptiveActive
                    case 6: knx.getGroupObject(LMG_KoLMGHCLM6AdaptiveActive).value(active, Dpt(1, 11)); break;
#endif
#ifdef LMG_KoLMGHCLM7AdaptiveActive
                    case 7: knx.getGroupObject(LMG_KoLMGHCLM7AdaptiveActive).value(active, Dpt(1, 11)); break;
#endif
#ifdef LMG_KoLMGHCLM8AdaptiveActive
                    case 8: knx.getGroupObject(LMG_KoLMGHCLM8AdaptiveActive).value(active, Dpt(1, 11)); break;
#endif
#ifdef LMG_KoLMGHCLM9AdaptiveActive
                    case 9: knx.getGroupObject(LMG_KoLMGHCLM9AdaptiveActive).value(active, Dpt(1, 11)); break;
#endif
#ifdef LMG_KoLMGHCLM10AdaptiveActive
                    case 10: knx.getGroupObject(LMG_KoLMGHCLM10AdaptiveActive).value(active, Dpt(1, 11)); break;
#endif
#ifdef LMG_KoLMGHCLM11AdaptiveActive
                    case 11: knx.getGroupObject(LMG_KoLMGHCLM11AdaptiveActive).value(active, Dpt(1, 11)); break;
#endif
#ifdef LMG_KoLMGHCLM12AdaptiveActive
                    case 12: knx.getGroupObject(LMG_KoLMGHCLM12AdaptiveActive).value(active, Dpt(1, 11)); break;
#endif
#ifdef LMG_KoLMGHCLM13AdaptiveActive
                    case 13: knx.getGroupObject(LMG_KoLMGHCLM13AdaptiveActive).value(active, Dpt(1, 11)); break;
#endif
#ifdef LMG_KoLMGHCLM14AdaptiveActive
                    case 14: knx.getGroupObject(LMG_KoLMGHCLM14AdaptiveActive).value(active, Dpt(1, 11)); break;
#endif
#ifdef LMG_KoLMGHCLM15AdaptiveActive
                    case 15: knx.getGroupObject(LMG_KoLMGHCLM15AdaptiveActive).value(active, Dpt(1, 11)); break;
#endif
#ifdef LMG_KoLMGHCLM16AdaptiveActive
                    case 16: knx.getGroupObject(LMG_KoLMGHCLM16AdaptiveActive).value(active, Dpt(1, 11)); break;
#endif
                    default: break;
                }
                return;
            }
        }
    }

    // Per-master day/night (DPT 1.001)
    {
        struct Entry { uint16_t ko; uint8_t master; };
        static const Entry table[] = {
#ifdef LMG_KoLMGHCLM1DayNight
            { LMG_KoLMGHCLM1DayNight, 1 },
#endif
#ifdef LMG_KoLMGHCLM2DayNight
            { LMG_KoLMGHCLM2DayNight, 2 },
#endif
#ifdef LMG_KoLMGHCLM3DayNight
            { LMG_KoLMGHCLM3DayNight, 3 },
#endif
#ifdef LMG_KoLMGHCLM4DayNight
            { LMG_KoLMGHCLM4DayNight, 4 },
#endif
#ifdef LMG_KoLMGHCLM5DayNight
            { LMG_KoLMGHCLM5DayNight, 5 },
#endif
#ifdef LMG_KoLMGHCLM6DayNight
            { LMG_KoLMGHCLM6DayNight, 6 },
#endif
#ifdef LMG_KoLMGHCLM7DayNight
            { LMG_KoLMGHCLM7DayNight, 7 },
#endif
#ifdef LMG_KoLMGHCLM8DayNight
            { LMG_KoLMGHCLM8DayNight, 8 },
#endif
#ifdef LMG_KoLMGHCLM9DayNight
            { LMG_KoLMGHCLM9DayNight, 9 },
#endif
#ifdef LMG_KoLMGHCLM10DayNight
            { LMG_KoLMGHCLM10DayNight, 10 },
#endif
#ifdef LMG_KoLMGHCLM11DayNight
            { LMG_KoLMGHCLM11DayNight, 11 },
#endif
#ifdef LMG_KoLMGHCLM12DayNight
            { LMG_KoLMGHCLM12DayNight, 12 },
#endif
#ifdef LMG_KoLMGHCLM13DayNight
            { LMG_KoLMGHCLM13DayNight, 13 },
#endif
#ifdef LMG_KoLMGHCLM14DayNight
            { LMG_KoLMGHCLM14DayNight, 14 },
#endif
#ifdef LMG_KoLMGHCLM15DayNight
            { LMG_KoLMGHCLM15DayNight, 15 },
#endif
#ifdef LMG_KoLMGHCLM16DayNight
            { LMG_KoLMGHCLM16DayNight, 16 },
#endif
            { 0, 0 }
        };
        for (size_t i = 0; table[i].master != 0; i++)
        {
            if (koNumber == table[i].ko)
            {
                HCL::masterManager.setMasterDaytime(table[i].master, ko.value(Dpt(1, 1)));
                return;
            }
        }
    }
}

bool LightManagerModule::processCommand(const std::string cmd, bool diagnoseKo)
{
    return false;
}

void LightManagerModule::registerOutput(uint8_t masterNum, ILightManagerOutput* target)
{
    if (masterNum < 1 || masterNum > HCL::MasterManager::MAX_MASTERS || target == nullptr)
        return;

    // Dedupe: remove any prior registration of the same target (any master) to
    // avoid duplicates when setupDevices() runs multiple times.
    for (auto it = _outputs.begin(); it != _outputs.end();)
    {
        if (it->target == target)
            it = _outputs.erase(it);
        else
            ++it;
    }

    _outputs.push_back({masterNum, target});

    // Immediately push the current value so newly registered (or re-registered)
    // outputs receive the HCL setpoint without waiting for the next change.
    const uint8_t idx = masterNum - 1;
    const bool blocked = HCL::masterManager.isApplyBlocked() ||
                         HCL::masterManager.isMasterApplyBlocked(masterNum);
    if (!blocked)
    {
        const HCL::InterpolatedValue val = HCL::masterManager.getCurrentValue(masterNum);
        const uint8_t fadeDuration = HCL::masterManager.getFadeDuration();
        target->onLightManagerValue(masterNum, val.kelvin, val.brightness, fadeDuration);
        _lastPushedValues[idx] = val;
    }
}

void LightManagerModule::unregisterOutput(ILightManagerOutput* target)
{
    if (target == nullptr)
        return;
    for (auto it = _outputs.begin(); it != _outputs.end();)
    {
        if (it->target == target)
            it = _outputs.erase(it);
        else
            ++it;
    }
}

void LightManagerModule::notifyOutputActive(uint8_t masterNum, bool active)
{
    if (masterNum < 1 || masterNum > HCL::MasterManager::MAX_MASTERS)
        return;

    if (!active)
    {
        // Light turned off: release per-master lock so HCL can resume when light comes back on
        HCL::masterManager.setMasterApplyBlocked(masterNum, false);
    }
}

void LightManagerModule::pushToOutputs(uint8_t masterNum, const HCL::InterpolatedValue& value)
{
    const uint8_t fadeDuration = HCL::masterManager.getFadeDuration();

    for (auto& reg : _outputs)
    {
        if (reg.masterNum == masterNum && reg.target != nullptr)
            reg.target->onLightManagerValue(masterNum, value.kelvin, value.brightness, fadeDuration);
    }

    // Send status KOs
    switch (masterNum)
    {
#ifdef LMG_KoLMGHCLM1StatusBrightness
        case 1:
            knx.getGroupObject(LMG_KoLMGHCLM1StatusBrightness).value(value.brightness, Dpt(5, 1));
            knx.getGroupObject(LMG_KoLMGHCLM1StatusColorTemp).value((uint16_t)value.kelvin, Dpt(7, 600));
            break;
#endif
#ifdef LMG_KoLMGHCLM2StatusBrightness
        case 2:
            knx.getGroupObject(LMG_KoLMGHCLM2StatusBrightness).value(value.brightness, Dpt(5, 1));
            knx.getGroupObject(LMG_KoLMGHCLM2StatusColorTemp).value((uint16_t)value.kelvin, Dpt(7, 600));
            break;
#endif
#ifdef LMG_KoLMGHCLM3StatusBrightness
        case 3:
            knx.getGroupObject(LMG_KoLMGHCLM3StatusBrightness).value(value.brightness, Dpt(5, 1));
            knx.getGroupObject(LMG_KoLMGHCLM3StatusColorTemp).value((uint16_t)value.kelvin, Dpt(7, 600));
            break;
#endif
#ifdef LMG_KoLMGHCLM4StatusBrightness
        case 4:
            knx.getGroupObject(LMG_KoLMGHCLM4StatusBrightness).value(value.brightness, Dpt(5, 1));
            knx.getGroupObject(LMG_KoLMGHCLM4StatusColorTemp).value((uint16_t)value.kelvin, Dpt(7, 600));
            break;
#endif
#ifdef LMG_KoLMGHCLM5StatusBrightness
        case 5:
            knx.getGroupObject(LMG_KoLMGHCLM5StatusBrightness).value(value.brightness, Dpt(5, 1));
            knx.getGroupObject(LMG_KoLMGHCLM5StatusColorTemp).value((uint16_t)value.kelvin, Dpt(7, 600));
            break;
#endif
#ifdef LMG_KoLMGHCLM6StatusBrightness
        case 6:
            knx.getGroupObject(LMG_KoLMGHCLM6StatusBrightness).value(value.brightness, Dpt(5, 1));
            knx.getGroupObject(LMG_KoLMGHCLM6StatusColorTemp).value((uint16_t)value.kelvin, Dpt(7, 600));
            break;
#endif
#ifdef LMG_KoLMGHCLM7StatusBrightness
        case 7:
            knx.getGroupObject(LMG_KoLMGHCLM7StatusBrightness).value(value.brightness, Dpt(5, 1));
            knx.getGroupObject(LMG_KoLMGHCLM7StatusColorTemp).value((uint16_t)value.kelvin, Dpt(7, 600));
            break;
#endif
#ifdef LMG_KoLMGHCLM8StatusBrightness
        case 8:
            knx.getGroupObject(LMG_KoLMGHCLM8StatusBrightness).value(value.brightness, Dpt(5, 1));
            knx.getGroupObject(LMG_KoLMGHCLM8StatusColorTemp).value((uint16_t)value.kelvin, Dpt(7, 600));
            break;
#endif
#ifdef LMG_KoLMGHCLM9StatusBrightness
        case 9:
            knx.getGroupObject(LMG_KoLMGHCLM9StatusBrightness).value(value.brightness, Dpt(5, 1));
            knx.getGroupObject(LMG_KoLMGHCLM9StatusColorTemp).value((uint16_t)value.kelvin, Dpt(7, 600));
            break;
#endif
#ifdef LMG_KoLMGHCLM10StatusBrightness
        case 10:
            knx.getGroupObject(LMG_KoLMGHCLM10StatusBrightness).value(value.brightness, Dpt(5, 1));
            knx.getGroupObject(LMG_KoLMGHCLM10StatusColorTemp).value((uint16_t)value.kelvin, Dpt(7, 600));
            break;
#endif
#ifdef LMG_KoLMGHCLM11StatusBrightness
        case 11:
            knx.getGroupObject(LMG_KoLMGHCLM11StatusBrightness).value(value.brightness, Dpt(5, 1));
            knx.getGroupObject(LMG_KoLMGHCLM11StatusColorTemp).value((uint16_t)value.kelvin, Dpt(7, 600));
            break;
#endif
#ifdef LMG_KoLMGHCLM12StatusBrightness
        case 12:
            knx.getGroupObject(LMG_KoLMGHCLM12StatusBrightness).value(value.brightness, Dpt(5, 1));
            knx.getGroupObject(LMG_KoLMGHCLM12StatusColorTemp).value((uint16_t)value.kelvin, Dpt(7, 600));
            break;
#endif
#ifdef LMG_KoLMGHCLM13StatusBrightness
        case 13:
            knx.getGroupObject(LMG_KoLMGHCLM13StatusBrightness).value(value.brightness, Dpt(5, 1));
            knx.getGroupObject(LMG_KoLMGHCLM13StatusColorTemp).value((uint16_t)value.kelvin, Dpt(7, 600));
            break;
#endif
#ifdef LMG_KoLMGHCLM14StatusBrightness
        case 14:
            knx.getGroupObject(LMG_KoLMGHCLM14StatusBrightness).value(value.brightness, Dpt(5, 1));
            knx.getGroupObject(LMG_KoLMGHCLM14StatusColorTemp).value((uint16_t)value.kelvin, Dpt(7, 600));
            break;
#endif
#ifdef LMG_KoLMGHCLM15StatusBrightness
        case 15:
            knx.getGroupObject(LMG_KoLMGHCLM15StatusBrightness).value(value.brightness, Dpt(5, 1));
            knx.getGroupObject(LMG_KoLMGHCLM15StatusColorTemp).value((uint16_t)value.kelvin, Dpt(7, 600));
            break;
#endif
#ifdef LMG_KoLMGHCLM16StatusBrightness
        case 16:
            knx.getGroupObject(LMG_KoLMGHCLM16StatusBrightness).value(value.brightness, Dpt(5, 1));
            knx.getGroupObject(LMG_KoLMGHCLM16StatusColorTemp).value((uint16_t)value.kelvin, Dpt(7, 600));
            break;
#endif
        default: break;
    }
}

void LightManagerModule::setupHclFromParams()
{
    #ifdef ParamLMG_LMGHCLEnable
    const bool enabled = ParamLMG_LMGHCLEnable != 0;
    #else
    const bool enabled = true;
    #endif
    HCL::masterManager.setEnabled(enabled);
    if (!enabled)
    {
        Serial.println("[LightManagerModule] HCL disabled");
        return;
    }

    #ifdef ParamLMG_LMGHCLUpdateInterval
    HCL::masterManager.setUpdateInterval(ParamLMG_LMGHCLUpdateInterval);
    #else
    HCL::masterManager.setUpdateInterval(60);
    #endif

    #ifdef ParamLMG_LMGHCLFadeDuration
    HCL::masterManager.setFadeDuration(ParamLMG_LMGHCLFadeDuration);
    #else
    HCL::masterManager.setFadeDuration(10);
    #endif

    #ifdef ParamLMG_LMGHCLLockFallback
    _hclLockFallbackMode = ParamLMG_LMGHCLLockFallback;
    #else
    _hclLockFallbackMode = static_cast<uint8_t>(HclLockFallbackMode::None);
    #endif

    #ifdef ParamLMG_LMGHCLFallbackPolicy
    _hclFallbackPolicy = ParamLMG_LMGHCLFallbackPolicy;
    if (_hclFallbackPolicy > static_cast<uint8_t>(HclLockFallbackPolicy::ExternalOnly))
    {
        _hclFallbackPolicy = static_cast<uint8_t>(HclLockFallbackPolicy::Legacy);
    }
    #else
    _hclFallbackPolicy = static_cast<uint8_t>(HclLockFallbackPolicy::Legacy);
    #endif

    _hclFallbackDurationMs = 0;
    #ifdef ParamLMG_LMGHCLFallbackDurationSec
    {
        const uint64_t durationMs = static_cast<uint64_t>(ParamLMG_LMGHCLFallbackDurationSec) * 1000ULL;
        _hclFallbackDurationMs = (durationMs > 0xFFFFFFFFULL) ? 0xFFFFFFFFUL : static_cast<uint32_t>(durationMs);
    }
    #endif

    _hclFallbackReleaseMinuteOfDay = 0xFFFF;
    #ifdef ParamLMG_LMGHCLFallbackReleaseTime
    {
        const String releaseTime = readFixedTimeParam(ParamLMG_LMGHCLFallbackReleaseTime);
        _hclFallbackReleaseMinuteOfDay = HCL::Setpoint::parseTime(releaseTime.c_str());
    }
    #endif

    for (uint8_t i = 0; i < HCL::MasterManager::MAX_MASTERS; i++)
    {
        _hclManagerLockFallbackMode[i] = static_cast<uint8_t>(HclLockFallbackMode::None);
        _hclManagerFallbackPolicy[i] = static_cast<uint8_t>(HclLockFallbackPolicy::Legacy);
        _hclManagerFallbackDurationMs[i] = 0;
        _hclManagerFallbackReleaseMinuteOfDay[i] = 0xFFFF;
    }

    auto readManagerPolicy = [this](uint8_t idx, uint8_t policyVal) {
        _hclManagerFallbackPolicy[idx] = policyVal;
        if (_hclManagerFallbackPolicy[idx] > static_cast<uint8_t>(HclLockFallbackPolicy::ExternalOnly))
        {
            _hclManagerFallbackPolicy[idx] = static_cast<uint8_t>(HclLockFallbackPolicy::Legacy);
        }
    };

#ifdef ParamLMG_LMGHCLM1LockFallback
    _hclManagerLockFallbackMode[0] = ParamLMG_LMGHCLM1LockFallback;
#endif
#ifdef ParamLMG_LMGHCLM2LockFallback
    _hclManagerLockFallbackMode[1] = ParamLMG_LMGHCLM2LockFallback;
#endif
#ifdef ParamLMG_LMGHCLM3LockFallback
    _hclManagerLockFallbackMode[2] = ParamLMG_LMGHCLM3LockFallback;
#endif
#ifdef ParamLMG_LMGHCLM4LockFallback
    _hclManagerLockFallbackMode[3] = ParamLMG_LMGHCLM4LockFallback;
#endif
#ifdef ParamLMG_LMGHCLM5LockFallback
    _hclManagerLockFallbackMode[4] = ParamLMG_LMGHCLM5LockFallback;
#endif
#ifdef ParamLMG_LMGHCLM6LockFallback
    _hclManagerLockFallbackMode[5] = ParamLMG_LMGHCLM6LockFallback;
#endif
#ifdef ParamLMG_LMGHCLM7LockFallback
    _hclManagerLockFallbackMode[6] = ParamLMG_LMGHCLM7LockFallback;
#endif
#ifdef ParamLMG_LMGHCLM8LockFallback
    _hclManagerLockFallbackMode[7] = ParamLMG_LMGHCLM8LockFallback;
#endif
#ifdef ParamLMG_LMGHCLM9LockFallback
    _hclManagerLockFallbackMode[8] = ParamLMG_LMGHCLM9LockFallback;
#endif
#ifdef ParamLMG_LMGHCLM10LockFallback
    _hclManagerLockFallbackMode[9] = ParamLMG_LMGHCLM10LockFallback;
#endif
#ifdef ParamLMG_LMGHCLM11LockFallback
    _hclManagerLockFallbackMode[10] = ParamLMG_LMGHCLM11LockFallback;
#endif
#ifdef ParamLMG_LMGHCLM12LockFallback
    _hclManagerLockFallbackMode[11] = ParamLMG_LMGHCLM12LockFallback;
#endif
#ifdef ParamLMG_LMGHCLM13LockFallback
    _hclManagerLockFallbackMode[12] = ParamLMG_LMGHCLM13LockFallback;
#endif
#ifdef ParamLMG_LMGHCLM14LockFallback
    _hclManagerLockFallbackMode[13] = ParamLMG_LMGHCLM14LockFallback;
#endif
#ifdef ParamLMG_LMGHCLM15LockFallback
    _hclManagerLockFallbackMode[14] = ParamLMG_LMGHCLM15LockFallback;
#endif
#ifdef ParamLMG_LMGHCLM16LockFallback
    _hclManagerLockFallbackMode[15] = ParamLMG_LMGHCLM16LockFallback;
#endif

#ifdef ParamLMG_LMGHCLM1FallbackPolicy
    readManagerPolicy(0, ParamLMG_LMGHCLM1FallbackPolicy);
#endif
#ifdef ParamLMG_LMGHCLM2FallbackPolicy
    readManagerPolicy(1, ParamLMG_LMGHCLM2FallbackPolicy);
#endif
#ifdef ParamLMG_LMGHCLM3FallbackPolicy
    readManagerPolicy(2, ParamLMG_LMGHCLM3FallbackPolicy);
#endif
#ifdef ParamLMG_LMGHCLM4FallbackPolicy
    readManagerPolicy(3, ParamLMG_LMGHCLM4FallbackPolicy);
#endif
#ifdef ParamLMG_LMGHCLM5FallbackPolicy
    readManagerPolicy(4, ParamLMG_LMGHCLM5FallbackPolicy);
#endif
#ifdef ParamLMG_LMGHCLM6FallbackPolicy
    readManagerPolicy(5, ParamLMG_LMGHCLM6FallbackPolicy);
#endif
#ifdef ParamLMG_LMGHCLM7FallbackPolicy
    readManagerPolicy(6, ParamLMG_LMGHCLM7FallbackPolicy);
#endif
#ifdef ParamLMG_LMGHCLM8FallbackPolicy
    readManagerPolicy(7, ParamLMG_LMGHCLM8FallbackPolicy);
#endif
#ifdef ParamLMG_LMGHCLM9FallbackPolicy
    readManagerPolicy(8, ParamLMG_LMGHCLM9FallbackPolicy);
#endif
#ifdef ParamLMG_LMGHCLM10FallbackPolicy
    readManagerPolicy(9, ParamLMG_LMGHCLM10FallbackPolicy);
#endif
#ifdef ParamLMG_LMGHCLM11FallbackPolicy
    readManagerPolicy(10, ParamLMG_LMGHCLM11FallbackPolicy);
#endif
#ifdef ParamLMG_LMGHCLM12FallbackPolicy
    readManagerPolicy(11, ParamLMG_LMGHCLM12FallbackPolicy);
#endif
#ifdef ParamLMG_LMGHCLM13FallbackPolicy
    readManagerPolicy(12, ParamLMG_LMGHCLM13FallbackPolicy);
#endif
#ifdef ParamLMG_LMGHCLM14FallbackPolicy
    readManagerPolicy(13, ParamLMG_LMGHCLM14FallbackPolicy);
#endif
#ifdef ParamLMG_LMGHCLM15FallbackPolicy
    readManagerPolicy(14, ParamLMG_LMGHCLM15FallbackPolicy);
#endif
#ifdef ParamLMG_LMGHCLM16FallbackPolicy
    readManagerPolicy(15, ParamLMG_LMGHCLM16FallbackPolicy);
#endif

    auto readManagerDuration = [this](uint8_t idx, uint32_t seconds) {
        const uint64_t durationMs = static_cast<uint64_t>(seconds) * 1000ULL;
        _hclManagerFallbackDurationMs[idx] = (durationMs > 0xFFFFFFFFULL) ? 0xFFFFFFFFUL : static_cast<uint32_t>(durationMs);
    };

#ifdef ParamLMG_LMGHCLM1FallbackDurationSec
    readManagerDuration(0, ParamLMG_LMGHCLM1FallbackDurationSec);
#endif
#ifdef ParamLMG_LMGHCLM2FallbackDurationSec
    readManagerDuration(1, ParamLMG_LMGHCLM2FallbackDurationSec);
#endif
#ifdef ParamLMG_LMGHCLM3FallbackDurationSec
    readManagerDuration(2, ParamLMG_LMGHCLM3FallbackDurationSec);
#endif
#ifdef ParamLMG_LMGHCLM4FallbackDurationSec
    readManagerDuration(3, ParamLMG_LMGHCLM4FallbackDurationSec);
#endif
#ifdef ParamLMG_LMGHCLM5FallbackDurationSec
    readManagerDuration(4, ParamLMG_LMGHCLM5FallbackDurationSec);
#endif
#ifdef ParamLMG_LMGHCLM6FallbackDurationSec
    readManagerDuration(5, ParamLMG_LMGHCLM6FallbackDurationSec);
#endif
#ifdef ParamLMG_LMGHCLM7FallbackDurationSec
    readManagerDuration(6, ParamLMG_LMGHCLM7FallbackDurationSec);
#endif
#ifdef ParamLMG_LMGHCLM8FallbackDurationSec
    readManagerDuration(7, ParamLMG_LMGHCLM8FallbackDurationSec);
#endif
#ifdef ParamLMG_LMGHCLM9FallbackDurationSec
    readManagerDuration(8, ParamLMG_LMGHCLM9FallbackDurationSec);
#endif
#ifdef ParamLMG_LMGHCLM10FallbackDurationSec
    readManagerDuration(9, ParamLMG_LMGHCLM10FallbackDurationSec);
#endif
#ifdef ParamLMG_LMGHCLM11FallbackDurationSec
    readManagerDuration(10, ParamLMG_LMGHCLM11FallbackDurationSec);
#endif
#ifdef ParamLMG_LMGHCLM12FallbackDurationSec
    readManagerDuration(11, ParamLMG_LMGHCLM12FallbackDurationSec);
#endif
#ifdef ParamLMG_LMGHCLM13FallbackDurationSec
    readManagerDuration(12, ParamLMG_LMGHCLM13FallbackDurationSec);
#endif
#ifdef ParamLMG_LMGHCLM14FallbackDurationSec
    readManagerDuration(13, ParamLMG_LMGHCLM14FallbackDurationSec);
#endif
#ifdef ParamLMG_LMGHCLM15FallbackDurationSec
    readManagerDuration(14, ParamLMG_LMGHCLM15FallbackDurationSec);
#endif
#ifdef ParamLMG_LMGHCLM16FallbackDurationSec
    readManagerDuration(15, ParamLMG_LMGHCLM16FallbackDurationSec);
#endif

    auto readManagerReleaseTime = [this](uint8_t idx, const uint8_t* rawTime) {
        const String releaseTime = readFixedTimeParam(rawTime);
        _hclManagerFallbackReleaseMinuteOfDay[idx] = HCL::Setpoint::parseTime(releaseTime.c_str());
    };

#ifdef ParamLMG_LMGHCLM1FallbackReleaseTime
    readManagerReleaseTime(0, ParamLMG_LMGHCLM1FallbackReleaseTime);
#endif
#ifdef ParamLMG_LMGHCLM2FallbackReleaseTime
    readManagerReleaseTime(1, ParamLMG_LMGHCLM2FallbackReleaseTime);
#endif
#ifdef ParamLMG_LMGHCLM3FallbackReleaseTime
    readManagerReleaseTime(2, ParamLMG_LMGHCLM3FallbackReleaseTime);
#endif
#ifdef ParamLMG_LMGHCLM4FallbackReleaseTime
    readManagerReleaseTime(3, ParamLMG_LMGHCLM4FallbackReleaseTime);
#endif
#ifdef ParamLMG_LMGHCLM5FallbackReleaseTime
    readManagerReleaseTime(4, ParamLMG_LMGHCLM5FallbackReleaseTime);
#endif
#ifdef ParamLMG_LMGHCLM6FallbackReleaseTime
    readManagerReleaseTime(5, ParamLMG_LMGHCLM6FallbackReleaseTime);
#endif
#ifdef ParamLMG_LMGHCLM7FallbackReleaseTime
    readManagerReleaseTime(6, ParamLMG_LMGHCLM7FallbackReleaseTime);
#endif
#ifdef ParamLMG_LMGHCLM8FallbackReleaseTime
    readManagerReleaseTime(7, ParamLMG_LMGHCLM8FallbackReleaseTime);
#endif
#ifdef ParamLMG_LMGHCLM9FallbackReleaseTime
    readManagerReleaseTime(8, ParamLMG_LMGHCLM9FallbackReleaseTime);
#endif
#ifdef ParamLMG_LMGHCLM10FallbackReleaseTime
    readManagerReleaseTime(9, ParamLMG_LMGHCLM10FallbackReleaseTime);
#endif
#ifdef ParamLMG_LMGHCLM11FallbackReleaseTime
    readManagerReleaseTime(10, ParamLMG_LMGHCLM11FallbackReleaseTime);
#endif
#ifdef ParamLMG_LMGHCLM12FallbackReleaseTime
    readManagerReleaseTime(11, ParamLMG_LMGHCLM12FallbackReleaseTime);
#endif
#ifdef ParamLMG_LMGHCLM13FallbackReleaseTime
    readManagerReleaseTime(12, ParamLMG_LMGHCLM13FallbackReleaseTime);
#endif
#ifdef ParamLMG_LMGHCLM14FallbackReleaseTime
    readManagerReleaseTime(13, ParamLMG_LMGHCLM14FallbackReleaseTime);
#endif
#ifdef ParamLMG_LMGHCLM15FallbackReleaseTime
    readManagerReleaseTime(14, ParamLMG_LMGHCLM15FallbackReleaseTime);
#endif
#ifdef ParamLMG_LMGHCLM16FallbackReleaseTime
    readManagerReleaseTime(15, ParamLMG_LMGHCLM16FallbackReleaseTime);
#endif

    float hclLatitude = 50.115377f;
    float hclLongitude = 8.684170f;
    int16_t hclTimezoneOffsetMinutes = 60;

#ifdef ParamBASE_Latitude
    hclLatitude = ParamBASE_Latitude;
#endif
#ifdef ParamBASE_Longitude
    hclLongitude = ParamBASE_Longitude;
#endif
#ifdef ParamBASE_Timezone
    hclTimezoneOffsetMinutes = decodeBaseTimezoneOffsetMinutes(static_cast<uint8_t>(ParamBASE_Timezone));
#endif

    auto loadMasterSetpoints = [](uint8_t masterNumber,
                                  const char* const (&times)[10],
                                  const uint16_t (&kelvins)[10],
                                  const uint8_t (&brightnesses)[10],
                                  const bool (&active)[10]) {
        HCL::Master* master = HCL::masterManager.getMaster(masterNumber);
        if (!master)
        {
            return;
        }

        for (int i = 0; i < 10; i++)
        {
            master->setSetpoint(i, HCL::Setpoint(0xFFFF, 4000, 100));
        }

        for (int i = 0; i < 10; i++)
        {
            if (!active[i])
            {
                continue;
            }

            const uint16_t minutes = HCL::Setpoint::parseTime(times[i]);
            if (minutes == 0xFFFF)
            {
                continue;
            }

            master->setSetpoint(i, HCL::Setpoint(minutes, kelvins[i], brightnesses[i]));
        }

        master->sortSetpoints();
    };

    auto loadMasterSummerSetpoints = [](uint8_t masterNumber,
                                        const char* const (&times)[10],
                                        const uint16_t (&summerKelvins)[10],
                                        const uint8_t (&summerBrightnesses)[10],
                                        const bool (&active)[10]) {
        HCL::Master* master = HCL::masterManager.getMaster(masterNumber);
        if (!master)
        {
            return;
        }

        for (int i = 0; i < 10; i++)
        {
            if (!active[i])
            {
                master->setSummerSetpoint(i, HCL::Setpoint(0xFFFF, 4000, 100));
                continue;
            }

            const uint16_t minutes = HCL::Setpoint::parseTime(times[i]);
            if (minutes == 0xFFFF)
            {
                master->setSummerSetpoint(i, HCL::Setpoint(0xFFFF, 4000, 100));
                continue;
            }

            master->setSummerSetpoint(i, HCL::Setpoint(minutes, summerKelvins[i], summerBrightnesses[i]));
        }

        master->sortSummerSetpoints();
    };

    auto applyMasterAdvanced = [](uint8_t masterNumber,
                                  uint8_t curveType,
                                  uint16_t slewRateKelvinPerMinute,
                                  uint16_t manualKelvin,
                                  const String& sunriseTime,
                                  const String& sunsetTime,
                                  int16_t sunriseOffset,
                                  int16_t sunsetOffset,
                                  float latitude,
                                  float longitude,
                                  int16_t timezoneOffsetMinutes,
                                  uint16_t astroMinKelvin,
                                  uint16_t astroMaxKelvin,
                                  uint8_t astroMinBrightness,
                                  uint8_t astroMaxBrightness) {
        HCL::Master* master = HCL::masterManager.getMaster(masterNumber);
        if (!master)
        {
            return;
        }

        if (curveType > static_cast<uint8_t>(HCL::CurveType::Astronomical))
        {
            curveType = static_cast<uint8_t>(HCL::CurveType::FixedTime);
        }

        master->setCurveType(static_cast<HCL::CurveType>(curveType));
        master->setSlewRateKelvinPerMinute(slewRateKelvinPerMinute);
        master->setManualKelvin(manualKelvin);
        master->setLocation(latitude, longitude);
        master->setTimezoneOffsetMinutes(timezoneOffsetMinutes);
        master->setAstronomicalProfile(astroMinKelvin, astroMaxKelvin, astroMinBrightness, astroMaxBrightness);

        const uint16_t sunriseMinutes = HCL::Setpoint::parseTime(sunriseTime.c_str());
        const uint16_t sunsetMinutes = HCL::Setpoint::parseTime(sunsetTime.c_str());
        if (sunriseMinutes != 0xFFFF && sunsetMinutes != 0xFFFF)
        {
            master->setSunTimes(sunriseMinutes, sunsetMinutes);
        }
        else
        {
            master->clearSunTimes();
        }

        master->setSunOffsets(sunriseOffset, sunsetOffset);
    };

    auto loadMasterAdaptiveConfig = [](uint8_t masterNumber,
                                       uint8_t mode,
                                       uint8_t activeMode,
                                       uint8_t ceilToHCL,
                                       uint16_t maxLux,
                                       uint8_t minBrightness,
                                       uint8_t sensorTimeout,
                                       uint8_t minChange,
                                       uint8_t strength,
                                       uint8_t kpEnum,
                                       uint16_t deadband,
                                       const uint8_t* startTimeRaw,
                                       const uint8_t* endTimeRaw,
                                       uint8_t dayNightPolarity) {
        HCL::Master* master = HCL::masterManager.getMaster(masterNumber);
        if (!master)
        {
            return;
        }

        HCL::AdaptiveConfig cfg;
        cfg.mode = static_cast<HCL::AdaptiveMode>(mode);
        cfg.activeMode = static_cast<HCL::AdaptiveActiveMode>(activeMode);
        cfg.ceilToHCL = (ceilToHCL != 0);
        cfg.maxLux = maxLux;
        cfg.minBrightness = minBrightness;
        cfg.sensorTimeoutMinutes = sensorTimeout;
        cfg.minChangePercent = minChange;
        cfg.strength = strength;
        static const float kpValues[] = {0.5f, 1.0f, 1.5f, 2.0f};
        cfg.kp = (kpEnum < 4) ? kpValues[kpEnum] : 1.0f;
        cfg.deadbandLux = deadband;
        const String startStr = readFixedTimeParam(startTimeRaw);
        const String endStr = readFixedTimeParam(endTimeRaw);
        cfg.activeStartMinutes = HCL::Setpoint::parseTime(startStr.c_str());
        cfg.activeEndMinutes = HCL::Setpoint::parseTime(endStr.c_str());
        if (cfg.activeStartMinutes == 0xFFFF)
        {
            cfg.activeStartMinutes = 360;
        }
        if (cfg.activeEndMinutes == 0xFFFF)
        {
            cfg.activeEndMinutes = 1320;
        }
        cfg.dayNightPolarity = (dayNightPolarity != 0);
        master->setAdaptiveConfig(cfg);
    };

    auto getAstroMinKelvin = [](uint8_t masterNumber) -> uint16_t {
        switch (masterNumber)
        {
            case 1:
#ifdef ParamLMG_HCLM1AstroMinKelvin
                return ParamLMG_HCLM1AstroMinKelvin;
#endif
                break;
            case 2:
#ifdef ParamLMG_HCLM2AstroMinKelvin
                return ParamLMG_HCLM2AstroMinKelvin;
#endif
                break;
            case 3:
#ifdef ParamLMG_HCLM3AstroMinKelvin
                return ParamLMG_HCLM3AstroMinKelvin;
#endif
                break;
            case 4:
#ifdef ParamLMG_HCLM4AstroMinKelvin
                return ParamLMG_HCLM4AstroMinKelvin;
#endif
                break;
            case 5:
#ifdef ParamLMG_HCLM5AstroMinKelvin
                return ParamLMG_HCLM5AstroMinKelvin;
#endif
                break;
            case 6:
#ifdef ParamLMG_HCLM6AstroMinKelvin
                return ParamLMG_HCLM6AstroMinKelvin;
#endif
                break;
            case 7:
#ifdef ParamLMG_HCLM7AstroMinKelvin
                return ParamLMG_HCLM7AstroMinKelvin;
#endif
                break;
            case 8:
#ifdef ParamLMG_HCLM8AstroMinKelvin
                return ParamLMG_HCLM8AstroMinKelvin;
#endif
                break;
            default:
                break;
        }
        return 2400;
    };

    auto getAstroMaxKelvin = [](uint8_t masterNumber) -> uint16_t {
        switch (masterNumber)
        {
            case 1:
#ifdef ParamLMG_HCLM1AstroMaxKelvin
                return ParamLMG_HCLM1AstroMaxKelvin;
#endif
                break;
            case 2:
#ifdef ParamLMG_HCLM2AstroMaxKelvin
                return ParamLMG_HCLM2AstroMaxKelvin;
#endif
                break;
            case 3:
#ifdef ParamLMG_HCLM3AstroMaxKelvin
                return ParamLMG_HCLM3AstroMaxKelvin;
#endif
                break;
            case 4:
#ifdef ParamLMG_HCLM4AstroMaxKelvin
                return ParamLMG_HCLM4AstroMaxKelvin;
#endif
                break;
            case 5:
#ifdef ParamLMG_HCLM5AstroMaxKelvin
                return ParamLMG_HCLM5AstroMaxKelvin;
#endif
                break;
            case 6:
#ifdef ParamLMG_HCLM6AstroMaxKelvin
                return ParamLMG_HCLM6AstroMaxKelvin;
#endif
                break;
            case 7:
#ifdef ParamLMG_HCLM7AstroMaxKelvin
                return ParamLMG_HCLM7AstroMaxKelvin;
#endif
                break;
            case 8:
#ifdef ParamLMG_HCLM8AstroMaxKelvin
                return ParamLMG_HCLM8AstroMaxKelvin;
#endif
                break;
            default:
                break;
        }
        return 5000;
    };

    auto getAstroMinBrightness = [](uint8_t masterNumber) -> uint8_t {
        switch (masterNumber)
        {
            case 1:
#ifdef ParamLMG_HCLM1AstroMinBrightness
                return ParamLMG_HCLM1AstroMinBrightness;
#endif
                break;
            case 2:
#ifdef ParamLMG_HCLM2AstroMinBrightness
                return ParamLMG_HCLM2AstroMinBrightness;
#endif
                break;
            case 3:
#ifdef ParamLMG_HCLM3AstroMinBrightness
                return ParamLMG_HCLM3AstroMinBrightness;
#endif
                break;
            case 4:
#ifdef ParamLMG_HCLM4AstroMinBrightness
                return ParamLMG_HCLM4AstroMinBrightness;
#endif
                break;
            case 5:
#ifdef ParamLMG_HCLM5AstroMinBrightness
                return ParamLMG_HCLM5AstroMinBrightness;
#endif
                break;
            case 6:
#ifdef ParamLMG_HCLM6AstroMinBrightness
                return ParamLMG_HCLM6AstroMinBrightness;
#endif
                break;
            case 7:
#ifdef ParamLMG_HCLM7AstroMinBrightness
                return ParamLMG_HCLM7AstroMinBrightness;
#endif
                break;
            case 8:
#ifdef ParamLMG_HCLM8AstroMinBrightness
                return ParamLMG_HCLM8AstroMinBrightness;
#endif
                break;
            default:
                break;
        }
        return 10;
    };

    auto getAstroMaxBrightness = [](uint8_t masterNumber) -> uint8_t {
        switch (masterNumber)
        {
            case 1:
#ifdef ParamLMG_HCLM1AstroMaxBrightness
                return ParamLMG_HCLM1AstroMaxBrightness;
#endif
                break;
            case 2:
#ifdef ParamLMG_HCLM2AstroMaxBrightness
                return ParamLMG_HCLM2AstroMaxBrightness;
#endif
                break;
            case 3:
#ifdef ParamLMG_HCLM3AstroMaxBrightness
                return ParamLMG_HCLM3AstroMaxBrightness;
#endif
                break;
            case 4:
#ifdef ParamLMG_HCLM4AstroMaxBrightness
                return ParamLMG_HCLM4AstroMaxBrightness;
#endif
                break;
            case 5:
#ifdef ParamLMG_HCLM5AstroMaxBrightness
                return ParamLMG_HCLM5AstroMaxBrightness;
#endif
                break;
            case 6:
#ifdef ParamLMG_HCLM6AstroMaxBrightness
                return ParamLMG_HCLM6AstroMaxBrightness;
#endif
                break;
            case 7:
#ifdef ParamLMG_HCLM7AstroMaxBrightness
                return ParamLMG_HCLM7AstroMaxBrightness;
#endif
                break;
            case 8:
#ifdef ParamLMG_HCLM8AstroMaxBrightness
                return ParamLMG_HCLM8AstroMaxBrightness;
#endif
                break;
            default:
                break;
        }
        return 80;
    };

#define LMG_LOAD_MASTER_SETPOINTS(M) \
    do { \
        const char* const times[10] = { \
            reinterpret_cast<const char*>(ParamLMG_HCLM##M##SP0Time), reinterpret_cast<const char*>(ParamLMG_HCLM##M##SP1Time), \
            reinterpret_cast<const char*>(ParamLMG_HCLM##M##SP2Time), reinterpret_cast<const char*>(ParamLMG_HCLM##M##SP3Time), \
            reinterpret_cast<const char*>(ParamLMG_HCLM##M##SP4Time), reinterpret_cast<const char*>(ParamLMG_HCLM##M##SP5Time), \
            reinterpret_cast<const char*>(ParamLMG_HCLM##M##SP6Time), reinterpret_cast<const char*>(ParamLMG_HCLM##M##SP7Time), \
            reinterpret_cast<const char*>(ParamLMG_HCLM##M##SP8Time), reinterpret_cast<const char*>(ParamLMG_HCLM##M##SP9Time) }; \
        const uint16_t kelvins[10] = { \
            ParamLMG_HCLM##M##SP0Kelvin, ParamLMG_HCLM##M##SP1Kelvin, ParamLMG_HCLM##M##SP2Kelvin, ParamLMG_HCLM##M##SP3Kelvin, \
            ParamLMG_HCLM##M##SP4Kelvin, ParamLMG_HCLM##M##SP5Kelvin, ParamLMG_HCLM##M##SP6Kelvin, ParamLMG_HCLM##M##SP7Kelvin, \
            ParamLMG_HCLM##M##SP8Kelvin, ParamLMG_HCLM##M##SP9Kelvin }; \
        const uint8_t brightnesses[10] = { \
            ParamLMG_HCLM##M##SP0Brightness, ParamLMG_HCLM##M##SP1Brightness, ParamLMG_HCLM##M##SP2Brightness, ParamLMG_HCLM##M##SP3Brightness, \
            ParamLMG_HCLM##M##SP4Brightness, ParamLMG_HCLM##M##SP5Brightness, ParamLMG_HCLM##M##SP6Brightness, ParamLMG_HCLM##M##SP7Brightness, \
            ParamLMG_HCLM##M##SP8Brightness, ParamLMG_HCLM##M##SP9Brightness }; \
        const bool active[10] = { \
            ParamLMG_HCLM##M##SP0Active != 0, ParamLMG_HCLM##M##SP1Active != 0, ParamLMG_HCLM##M##SP2Active != 0, ParamLMG_HCLM##M##SP3Active != 0, \
            ParamLMG_HCLM##M##SP4Active != 0, ParamLMG_HCLM##M##SP5Active != 0, ParamLMG_HCLM##M##SP6Active != 0, ParamLMG_HCLM##M##SP7Active != 0, \
            ParamLMG_HCLM##M##SP8Active != 0, ParamLMG_HCLM##M##SP9Active != 0 }; \
        loadMasterSetpoints(M, times, kelvins, brightnesses, active); \
    } while (false)

#define LMG_LOAD_MASTER_SUMMER_SETPOINTS(M) \
    do { \
        const char* const times[10] = { \
            reinterpret_cast<const char*>(ParamLMG_HCLM##M##SP0Time), reinterpret_cast<const char*>(ParamLMG_HCLM##M##SP1Time), \
            reinterpret_cast<const char*>(ParamLMG_HCLM##M##SP2Time), reinterpret_cast<const char*>(ParamLMG_HCLM##M##SP3Time), \
            reinterpret_cast<const char*>(ParamLMG_HCLM##M##SP4Time), reinterpret_cast<const char*>(ParamLMG_HCLM##M##SP5Time), \
            reinterpret_cast<const char*>(ParamLMG_HCLM##M##SP6Time), reinterpret_cast<const char*>(ParamLMG_HCLM##M##SP7Time), \
            reinterpret_cast<const char*>(ParamLMG_HCLM##M##SP8Time), reinterpret_cast<const char*>(ParamLMG_HCLM##M##SP9Time) }; \
        const uint16_t summerKelvins[10] = { \
            ParamLMG_HCLM##M##SP0SummerKelvin, ParamLMG_HCLM##M##SP1SummerKelvin, ParamLMG_HCLM##M##SP2SummerKelvin, ParamLMG_HCLM##M##SP3SummerKelvin, \
            ParamLMG_HCLM##M##SP4SummerKelvin, ParamLMG_HCLM##M##SP5SummerKelvin, ParamLMG_HCLM##M##SP6SummerKelvin, ParamLMG_HCLM##M##SP7SummerKelvin, \
            ParamLMG_HCLM##M##SP8SummerKelvin, ParamLMG_HCLM##M##SP9SummerKelvin }; \
        const uint8_t summerBrightnesses[10] = { \
            ParamLMG_HCLM##M##SP0SummerBrightness, ParamLMG_HCLM##M##SP1SummerBrightness, ParamLMG_HCLM##M##SP2SummerBrightness, ParamLMG_HCLM##M##SP3SummerBrightness, \
            ParamLMG_HCLM##M##SP4SummerBrightness, ParamLMG_HCLM##M##SP5SummerBrightness, ParamLMG_HCLM##M##SP6SummerBrightness, ParamLMG_HCLM##M##SP7SummerBrightness, \
            ParamLMG_HCLM##M##SP8SummerBrightness, ParamLMG_HCLM##M##SP9SummerBrightness }; \
        const bool active[10] = { \
            ParamLMG_HCLM##M##SP0Active != 0, ParamLMG_HCLM##M##SP1Active != 0, ParamLMG_HCLM##M##SP2Active != 0, ParamLMG_HCLM##M##SP3Active != 0, \
            ParamLMG_HCLM##M##SP4Active != 0, ParamLMG_HCLM##M##SP5Active != 0, ParamLMG_HCLM##M##SP6Active != 0, ParamLMG_HCLM##M##SP7Active != 0, \
            ParamLMG_HCLM##M##SP8Active != 0, ParamLMG_HCLM##M##SP9Active != 0 }; \
        loadMasterSummerSetpoints(M, times, summerKelvins, summerBrightnesses, active); \
    } while (false)

#define LMG_APPLY_MASTER_ADVANCED(M) \
    do { \
        applyMasterAdvanced(M, ParamLMG_HCLM##M##CurveType, ParamLMG_HCLM##M##SlewRate, ParamLMG_HCLM##M##ManualKelvin, \
            readFixedTimeParam(ParamLMG_HCLM##M##Sunrise), readFixedTimeParam(ParamLMG_HCLM##M##Sunset), \
            static_cast<int16_t>(ParamLMG_HCLM##M##SunriseOffset), static_cast<int16_t>(ParamLMG_HCLM##M##SunsetOffset), \
            hclLatitude, hclLongitude, hclTimezoneOffsetMinutes, getAstroMinKelvin(M), getAstroMaxKelvin(M), getAstroMinBrightness(M), getAstroMaxBrightness(M)); \
    } while (false)

#define LMG_LOAD_MASTER_ADAPTIVE(M) \
    do { \
        loadMasterAdaptiveConfig(M, ParamLMG_HCLM##M##AdaptiveMode, ParamLMG_HCLM##M##AdaptiveActiveMode, \
            ParamLMG_HCLM##M##AdaptiveCeilToHCL, ParamLMG_HCLM##M##AdaptiveMaxLux, ParamLMG_HCLM##M##AdaptiveMinBrightness, \
            ParamLMG_HCLM##M##AdaptiveSensorTimeout, ParamLMG_HCLM##M##AdaptiveMinChange, ParamLMG_HCLM##M##AdaptiveStrength, \
            ParamLMG_HCLM##M##AdaptiveKp, ParamLMG_HCLM##M##AdaptiveDeadband, ParamLMG_HCLM##M##AdaptiveStartTime, ParamLMG_HCLM##M##AdaptiveEndTime, \
            ParamLMG_HCLM##M##AdaptiveDayNightPolarity); \
    } while (false)

#ifdef ParamLMG_HCLM1SP0Time
    LMG_LOAD_MASTER_SETPOINTS(1);
    #if defined(ParamLMG_HCLM1SeasonMode) && defined(ParamLMG_HCLM1SP0SummerKelvin)
    if (ParamLMG_HCLM1SeasonMode != 0)
        LMG_LOAD_MASTER_SUMMER_SETPOINTS(1);
    #endif
    #ifdef ParamLMG_HCLM1CurveType
    LMG_APPLY_MASTER_ADVANCED(1);
    #endif
#endif
#ifdef ParamLMG_HCLM1AdaptiveMode
    LMG_LOAD_MASTER_ADAPTIVE(1);
#endif

#ifdef ParamLMG_HCLM2SP0Time
    LMG_LOAD_MASTER_SETPOINTS(2);
    #if defined(ParamLMG_HCLM2SeasonMode) && defined(ParamLMG_HCLM2SP0SummerKelvin)
    if (ParamLMG_HCLM2SeasonMode != 0)
        LMG_LOAD_MASTER_SUMMER_SETPOINTS(2);
    #endif
    #ifdef ParamLMG_HCLM2CurveType
    LMG_APPLY_MASTER_ADVANCED(2);
    #endif
#endif
#ifdef ParamLMG_HCLM2AdaptiveMode
    LMG_LOAD_MASTER_ADAPTIVE(2);
#endif

#ifdef ParamLMG_HCLM3SP0Time
    LMG_LOAD_MASTER_SETPOINTS(3);
    #if defined(ParamLMG_HCLM3SeasonMode) && defined(ParamLMG_HCLM3SP0SummerKelvin)
    if (ParamLMG_HCLM3SeasonMode != 0)
        LMG_LOAD_MASTER_SUMMER_SETPOINTS(3);
    #endif
    #ifdef ParamLMG_HCLM3CurveType
    LMG_APPLY_MASTER_ADVANCED(3);
    #endif
#endif
#ifdef ParamLMG_HCLM3AdaptiveMode
    LMG_LOAD_MASTER_ADAPTIVE(3);
#endif

#ifdef ParamLMG_HCLM4SP0Time
    LMG_LOAD_MASTER_SETPOINTS(4);
    #if defined(ParamLMG_HCLM4SeasonMode) && defined(ParamLMG_HCLM4SP0SummerKelvin)
    if (ParamLMG_HCLM4SeasonMode != 0)
        LMG_LOAD_MASTER_SUMMER_SETPOINTS(4);
    #endif
    #ifdef ParamLMG_HCLM4CurveType
    LMG_APPLY_MASTER_ADVANCED(4);
    #endif
#endif
#ifdef ParamLMG_HCLM4AdaptiveMode
    LMG_LOAD_MASTER_ADAPTIVE(4);
#endif

#ifdef ParamLMG_HCLM5SP0Time
    LMG_LOAD_MASTER_SETPOINTS(5);
    #if defined(ParamLMG_HCLM5SeasonMode) && defined(ParamLMG_HCLM5SP0SummerKelvin)
    if (ParamLMG_HCLM5SeasonMode != 0)
        LMG_LOAD_MASTER_SUMMER_SETPOINTS(5);
    #endif
    #ifdef ParamLMG_HCLM5CurveType
    LMG_APPLY_MASTER_ADVANCED(5);
    #endif
#endif
#ifdef ParamLMG_HCLM5AdaptiveMode
    LMG_LOAD_MASTER_ADAPTIVE(5);
#endif

#ifdef ParamLMG_HCLM6SP0Time
    LMG_LOAD_MASTER_SETPOINTS(6);
    #if defined(ParamLMG_HCLM6SeasonMode) && defined(ParamLMG_HCLM6SP0SummerKelvin)
    if (ParamLMG_HCLM6SeasonMode != 0)
        LMG_LOAD_MASTER_SUMMER_SETPOINTS(6);
    #endif
    #ifdef ParamLMG_HCLM6CurveType
    LMG_APPLY_MASTER_ADVANCED(6);
    #endif
#endif
#ifdef ParamLMG_HCLM6AdaptiveMode
    LMG_LOAD_MASTER_ADAPTIVE(6);
#endif

#ifdef ParamLMG_HCLM7SP0Time
    LMG_LOAD_MASTER_SETPOINTS(7);
    #if defined(ParamLMG_HCLM7SeasonMode) && defined(ParamLMG_HCLM7SP0SummerKelvin)
    if (ParamLMG_HCLM7SeasonMode != 0)
        LMG_LOAD_MASTER_SUMMER_SETPOINTS(7);
    #endif
    #ifdef ParamLMG_HCLM7CurveType
    LMG_APPLY_MASTER_ADVANCED(7);
    #endif
#endif
#ifdef ParamLMG_HCLM7AdaptiveMode
    LMG_LOAD_MASTER_ADAPTIVE(7);
#endif

#ifdef ParamLMG_HCLM8SP0Time
    LMG_LOAD_MASTER_SETPOINTS(8);
    #if defined(ParamLMG_HCLM8SeasonMode) && defined(ParamLMG_HCLM8SP0SummerKelvin)
    if (ParamLMG_HCLM8SeasonMode != 0)
        LMG_LOAD_MASTER_SUMMER_SETPOINTS(8);
    #endif
    #ifdef ParamLMG_HCLM8CurveType
    LMG_APPLY_MASTER_ADVANCED(8);
    #endif
#endif
#ifdef ParamLMG_HCLM8AdaptiveMode
    LMG_LOAD_MASTER_ADAPTIVE(8);
#endif

#ifdef ParamLMG_HCLM9SP0Time
    LMG_LOAD_MASTER_SETPOINTS(9);
    #if defined(ParamLMG_HCLM9SeasonMode) && defined(ParamLMG_HCLM9SP0SummerKelvin)
    if (ParamLMG_HCLM9SeasonMode != 0)
        LMG_LOAD_MASTER_SUMMER_SETPOINTS(9);
    #endif
    #ifdef ParamLMG_HCLM9CurveType
    LMG_APPLY_MASTER_ADVANCED(9);
    #endif
#endif
#ifdef ParamLMG_HCLM9AdaptiveMode
    LMG_LOAD_MASTER_ADAPTIVE(9);
#endif

#ifdef ParamLMG_HCLM10SP0Time
    LMG_LOAD_MASTER_SETPOINTS(10);
    #if defined(ParamLMG_HCLM10SeasonMode) && defined(ParamLMG_HCLM10SP0SummerKelvin)
    if (ParamLMG_HCLM10SeasonMode != 0)
        LMG_LOAD_MASTER_SUMMER_SETPOINTS(10);
    #endif
    #ifdef ParamLMG_HCLM10CurveType
    LMG_APPLY_MASTER_ADVANCED(10);
    #endif
#endif
#ifdef ParamLMG_HCLM10AdaptiveMode
    LMG_LOAD_MASTER_ADAPTIVE(10);
#endif

#ifdef ParamLMG_HCLM11SP0Time
    LMG_LOAD_MASTER_SETPOINTS(11);
    #if defined(ParamLMG_HCLM11SeasonMode) && defined(ParamLMG_HCLM11SP0SummerKelvin)
    if (ParamLMG_HCLM11SeasonMode != 0)
        LMG_LOAD_MASTER_SUMMER_SETPOINTS(11);
    #endif
    #ifdef ParamLMG_HCLM11CurveType
    LMG_APPLY_MASTER_ADVANCED(11);
    #endif
#endif
#ifdef ParamLMG_HCLM11AdaptiveMode
    LMG_LOAD_MASTER_ADAPTIVE(11);
#endif

#ifdef ParamLMG_HCLM12SP0Time
    LMG_LOAD_MASTER_SETPOINTS(12);
    #if defined(ParamLMG_HCLM12SeasonMode) && defined(ParamLMG_HCLM12SP0SummerKelvin)
    if (ParamLMG_HCLM12SeasonMode != 0)
        LMG_LOAD_MASTER_SUMMER_SETPOINTS(12);
    #endif
    #ifdef ParamLMG_HCLM12CurveType
    LMG_APPLY_MASTER_ADVANCED(12);
    #endif
#endif
#ifdef ParamLMG_HCLM12AdaptiveMode
    LMG_LOAD_MASTER_ADAPTIVE(12);
#endif

#ifdef ParamLMG_HCLM13SP0Time
    LMG_LOAD_MASTER_SETPOINTS(13);
    #if defined(ParamLMG_HCLM13SeasonMode) && defined(ParamLMG_HCLM13SP0SummerKelvin)
    if (ParamLMG_HCLM13SeasonMode != 0)
        LMG_LOAD_MASTER_SUMMER_SETPOINTS(13);
    #endif
    #ifdef ParamLMG_HCLM13CurveType
    LMG_APPLY_MASTER_ADVANCED(13);
    #endif
#endif
#ifdef ParamLMG_HCLM13AdaptiveMode
    LMG_LOAD_MASTER_ADAPTIVE(13);
#endif

#ifdef ParamLMG_HCLM14SP0Time
    LMG_LOAD_MASTER_SETPOINTS(14);
    #if defined(ParamLMG_HCLM14SeasonMode) && defined(ParamLMG_HCLM14SP0SummerKelvin)
    if (ParamLMG_HCLM14SeasonMode != 0)
        LMG_LOAD_MASTER_SUMMER_SETPOINTS(14);
    #endif
    #ifdef ParamLMG_HCLM14CurveType
    LMG_APPLY_MASTER_ADVANCED(14);
    #endif
#endif
#ifdef ParamLMG_HCLM14AdaptiveMode
    LMG_LOAD_MASTER_ADAPTIVE(14);
#endif

#ifdef ParamLMG_HCLM15SP0Time
    LMG_LOAD_MASTER_SETPOINTS(15);
    #if defined(ParamLMG_HCLM15SeasonMode) && defined(ParamLMG_HCLM15SP0SummerKelvin)
    if (ParamLMG_HCLM15SeasonMode != 0)
        LMG_LOAD_MASTER_SUMMER_SETPOINTS(15);
    #endif
    #ifdef ParamLMG_HCLM15CurveType
    LMG_APPLY_MASTER_ADVANCED(15);
    #endif
#endif
#ifdef ParamLMG_HCLM15AdaptiveMode
    LMG_LOAD_MASTER_ADAPTIVE(15);
#endif

#ifdef ParamLMG_HCLM16SP0Time
    LMG_LOAD_MASTER_SETPOINTS(16);
    #if defined(ParamLMG_HCLM16SeasonMode) && defined(ParamLMG_HCLM16SP0SummerKelvin)
    if (ParamLMG_HCLM16SeasonMode != 0)
        LMG_LOAD_MASTER_SUMMER_SETPOINTS(16);
    #endif
    #ifdef ParamLMG_HCLM16CurveType
    LMG_APPLY_MASTER_ADVANCED(16);
    #endif
#endif
#ifdef ParamLMG_HCLM16AdaptiveMode
    LMG_LOAD_MASTER_ADAPTIVE(16);
#endif

#undef LMG_LOAD_MASTER_SETPOINTS
#undef LMG_LOAD_MASTER_SUMMER_SETPOINTS
#undef LMG_APPLY_MASTER_ADVANCED
#undef LMG_LOAD_MASTER_ADAPTIVE
}

// ---- Lock state machine ----

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
            const uint32_t durationMs = getHclFallbackDurationMs(static_cast<HclLockFallbackMode>(_hclLockFallbackMode));
            if (durationMs > 0)
                _hclLockAutoReleaseMs = _hclLockActivatedMs + durationMs;
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
            _hclLockActivationMinuteOfDay = static_cast<int16_t>((timeinfo.tm_hour * 60) + timeinfo.tm_min);
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
    if (managerNumber < 1 || managerNumber > HCL::MasterManager::MAX_MASTERS)
        return;

    const uint8_t idx = managerNumber - 1;
    const bool changed = (_hclManagerLockActive[idx] != active);
    _hclManagerLockActive[idx] = active;
    HCL::masterManager.setMasterApplyBlocked(managerNumber, active);

    if (active)
    {
        _hclManagerLockActivatedMs[idx] = millis();
        _hclManagerLockAutoReleaseMs[idx] = 0;
        _hclManagerLockActivationDayOfYear[idx] = -1;
        _hclManagerLockActivationMinuteOfDay[idx] = -1;

        const HclLockFallbackPolicy policy = static_cast<HclLockFallbackPolicy>(_hclManagerFallbackPolicy[idx]);
        if (policy == HclLockFallbackPolicy::Legacy)
        {
            const uint32_t durationMs = getHclFallbackDurationMs(static_cast<HclLockFallbackMode>(_hclManagerLockFallbackMode[idx]));
            if (durationMs > 0)
                _hclManagerLockAutoReleaseMs[idx] = _hclManagerLockActivatedMs[idx] + durationMs;
        }
        else if (policy == HclLockFallbackPolicy::Duration || policy == HclLockFallbackPolicy::DurationOrTime)
        {
            if (_hclManagerFallbackDurationMs[idx] > 0)
                _hclManagerLockAutoReleaseMs[idx] = _hclManagerLockActivatedMs[idx] + _hclManagerFallbackDurationMs[idx];
        }

        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 0))
        {
            _hclManagerLockActivationDayOfYear[idx] = static_cast<int16_t>(timeinfo.tm_yday);
            _hclManagerLockActivationMinuteOfDay[idx] = static_cast<int16_t>((timeinfo.tm_hour * 60) + timeinfo.tm_min);
        }

        if (changed)
            Serial.printf("[LightManagerModule] HCL master %u lock enabled (%s)\n", (unsigned)managerNumber, reason ? reason : "n/a");
    }
    else
    {
        _hclManagerLockActivatedMs[idx] = 0;
        _hclManagerLockAutoReleaseMs[idx] = 0;
        _hclManagerLockActivationDayOfYear[idx] = -1;
        _hclManagerLockActivationMinuteOfDay[idx] = -1;
        if (changed)
            Serial.printf("[LightManagerModule] HCL master %u lock disabled (%s)\n", (unsigned)managerNumber, reason ? reason : "n/a");
    }

    publishHclManagerLockStatus(managerNumber);
}

void LightManagerModule::publishHclLockStatus()
{
    #ifdef LMG_KoLMGHCLLockStatus
    knx.getGroupObject(LMG_KoLMGHCLLockStatus).value(_hclLockActive, Dpt(1, 1));
    #endif
}

void LightManagerModule::publishHclManagerLockStatus(uint8_t managerNumber)
{
    if (managerNumber < 1 || managerNumber > HCL::MasterManager::MAX_MASTERS)
        return;

    const bool active = _hclManagerLockActive[managerNumber - 1];
    switch (managerNumber)
    {
#ifdef LMG_KoLMGHCLM1LockStatus
        case 1: knx.getGroupObject(LMG_KoLMGHCLM1LockStatus).value(active, Dpt(1, 1)); break;
#endif
#ifdef LMG_KoLMGHCLM2LockStatus
        case 2: knx.getGroupObject(LMG_KoLMGHCLM2LockStatus).value(active, Dpt(1, 1)); break;
#endif
#ifdef LMG_KoLMGHCLM3LockStatus
        case 3: knx.getGroupObject(LMG_KoLMGHCLM3LockStatus).value(active, Dpt(1, 1)); break;
#endif
#ifdef LMG_KoLMGHCLM4LockStatus
        case 4: knx.getGroupObject(LMG_KoLMGHCLM4LockStatus).value(active, Dpt(1, 1)); break;
#endif
#ifdef LMG_KoLMGHCLM5LockStatus
        case 5: knx.getGroupObject(LMG_KoLMGHCLM5LockStatus).value(active, Dpt(1, 1)); break;
#endif
#ifdef LMG_KoLMGHCLM6LockStatus
        case 6: knx.getGroupObject(LMG_KoLMGHCLM6LockStatus).value(active, Dpt(1, 1)); break;
#endif
#ifdef LMG_KoLMGHCLM7LockStatus
        case 7: knx.getGroupObject(LMG_KoLMGHCLM7LockStatus).value(active, Dpt(1, 1)); break;
#endif
#ifdef LMG_KoLMGHCLM8LockStatus
        case 8: knx.getGroupObject(LMG_KoLMGHCLM8LockStatus).value(active, Dpt(1, 1)); break;
#endif
#ifdef LMG_KoLMGHCLM9LockStatus
    case 9: knx.getGroupObject(LMG_KoLMGHCLM9LockStatus).value(active, Dpt(1, 1)); break;
#endif
#ifdef LMG_KoLMGHCLM10LockStatus
    case 10: knx.getGroupObject(LMG_KoLMGHCLM10LockStatus).value(active, Dpt(1, 1)); break;
#endif
#ifdef LMG_KoLMGHCLM11LockStatus
    case 11: knx.getGroupObject(LMG_KoLMGHCLM11LockStatus).value(active, Dpt(1, 1)); break;
#endif
#ifdef LMG_KoLMGHCLM12LockStatus
    case 12: knx.getGroupObject(LMG_KoLMGHCLM12LockStatus).value(active, Dpt(1, 1)); break;
#endif
#ifdef LMG_KoLMGHCLM13LockStatus
    case 13: knx.getGroupObject(LMG_KoLMGHCLM13LockStatus).value(active, Dpt(1, 1)); break;
#endif
#ifdef LMG_KoLMGHCLM14LockStatus
    case 14: knx.getGroupObject(LMG_KoLMGHCLM14LockStatus).value(active, Dpt(1, 1)); break;
#endif
#ifdef LMG_KoLMGHCLM15LockStatus
    case 15: knx.getGroupObject(LMG_KoLMGHCLM15LockStatus).value(active, Dpt(1, 1)); break;
#endif
#ifdef LMG_KoLMGHCLM16LockStatus
    case 16: knx.getGroupObject(LMG_KoLMGHCLM16LockStatus).value(active, Dpt(1, 1)); break;
#endif
        default: break;
    }
}

void LightManagerModule::evaluateHclLockFallback(const tm* timeinfo, bool hasTime)
{
    if (!_hclLockActive)
        return;

    const HclLockFallbackPolicy policy = static_cast<HclLockFallbackPolicy>(_hclFallbackPolicy);
    if (policy == HclLockFallbackPolicy::ExternalOnly)
        return;

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

    // Legacy mode
    const HclLockFallbackMode mode = static_cast<HclLockFallbackMode>(_hclLockFallbackMode);
    if (mode == HclLockFallbackMode::None)
        return;

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

void LightManagerModule::evaluateHclManagerLockFallback(const tm* timeinfo, bool hasTime)
{
    for (uint8_t m = 1; m <= HCL::MasterManager::MAX_MASTERS; m++)
    {
        const uint8_t idx = m - 1;
        if (!_hclManagerLockActive[idx])
            continue;

        const HclLockFallbackPolicy policy = static_cast<HclLockFallbackPolicy>(_hclManagerFallbackPolicy[idx]);
        if (policy == HclLockFallbackPolicy::ExternalOnly)
            continue;

        if (policy != HclLockFallbackPolicy::Legacy)
        {
            const bool byDuration = (_hclManagerLockAutoReleaseMs[idx] != 0) &&
                                    (static_cast<long>(millis() - _hclManagerLockAutoReleaseMs[idx]) >= 0);
            const bool byTime = (policy == HclLockFallbackPolicy::TimeOfDay || policy == HclLockFallbackPolicy::DurationOrTime) &&
                                shouldReleaseByPolicyTime(_hclManagerLockActivationDayOfYear[idx],
                                                         _hclManagerLockActivationMinuteOfDay[idx],
                                                         _hclManagerFallbackReleaseMinuteOfDay[idx],
                                                         timeinfo, hasTime);
            if (byDuration || byTime)
                setHclManagerLock(m, false, byTime ? "fallback release time" : "fallback duration elapsed");
            continue;
        }

        // Legacy mode
        const HclLockFallbackMode mode = static_cast<HclLockFallbackMode>(_hclManagerLockFallbackMode[idx]);
        if (mode == HclLockFallbackMode::None)
            continue;

        if (_hclManagerLockAutoReleaseMs[idx] != 0)
        {
            if (static_cast<long>(millis() - _hclManagerLockAutoReleaseMs[idx]) >= 0)
                setHclManagerLock(m, false, "fallback duration elapsed");
            continue;
        }

        if (mode == HclLockFallbackMode::NextDay && hasTime && timeinfo != nullptr)
        {
            if (_hclManagerLockActivationDayOfYear[idx] >= 0 &&
                timeinfo->tm_yday != _hclManagerLockActivationDayOfYear[idx])
                setHclManagerLock(m, false, "fallback day change");
        }
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
    if (!hasTime || timeinfo == nullptr || releaseMinuteOfDay == 0xFFFF)
        return false;
    if (activationDayOfYear < 0 || activationMinuteOfDay < 0)
        return false;

    const int16_t currentDay = static_cast<int16_t>(timeinfo->tm_yday);
    const int16_t currentMinute = static_cast<int16_t>((timeinfo->tm_hour * 60) + timeinfo->tm_min);

    if (currentDay < activationDayOfYear)
        return false;
    if (currentDay == activationDayOfYear)
    {
        if (releaseMinuteOfDay <= static_cast<uint16_t>(activationMinuteOfDay))
            return false;
        return currentMinute >= static_cast<int16_t>(releaseMinuteOfDay);
    }
    return currentMinute >= static_cast<int16_t>(releaseMinuteOfDay);
}


// ---------------------------------------------------------------------------
// Flash persistence: per-master "is summer" state for season mode 3 (per KO)
// ---------------------------------------------------------------------------
// Without persistence the master's _isSummer flag would reset to false on every
// boot/reprogramming. KNX ReadOnInit only works when another bus device answers,
// which is unreliable. We therefore mirror the flag in flash.
//
// Layout (3 bytes):
//   [0]   version byte (0x01)
//   [1-2] uint16_t bitmask, bit (N-1) = isSummer of master N (N=1..16)

static constexpr uint8_t LMG_SUMMER_FLASH_VERSION = 0x01;

uint16_t LightManagerModule::flashSize()
{
    return 3;
}

void LightManagerModule::writeFlash()
{
    uint16_t mask = 0;
    for (uint8_t m = 1; m <= HCL::MasterManager::MAX_MASTERS && m <= 16; m++)
    {
        HCL::Master* master = HCL::masterManager.getMaster(m);
        if (master && master->isSummer())
            mask |= static_cast<uint16_t>(1u << (m - 1));
    }
    openknx.flash.writeByte(LMG_SUMMER_FLASH_VERSION);
    openknx.flash.writeByte(static_cast<uint8_t>(mask >> 8));
    openknx.flash.writeByte(static_cast<uint8_t>(mask & 0xFF));
}

void LightManagerModule::readFlash(const uint8_t* data, const uint16_t size)
{
    if (size < 3) return; // first boot or incompatible data

    const uint8_t version = openknx.flash.readByte();
    if (version != LMG_SUMMER_FLASH_VERSION) return;

    const uint8_t hi = openknx.flash.readByte();
    const uint8_t lo = openknx.flash.readByte();
    const uint16_t mask = (static_cast<uint16_t>(hi) << 8) | lo;

    auto restore = [&](uint8_t masterNumber, uint8_t seasonMode) {
        if (seasonMode != 3) return; // only "per KO" persists; others are derived dynamically
        HCL::Master* master = HCL::masterManager.getMaster(masterNumber);
        if (!master) return;
        const bool wasSummer = (mask & static_cast<uint16_t>(1u << (masterNumber - 1))) != 0;
        master->setIsSummer(wasSummer);
    };

#ifdef ParamLMG_HCLM1SeasonMode
    restore(1, ParamLMG_HCLM1SeasonMode);
#endif
#ifdef ParamLMG_HCLM2SeasonMode
    restore(2, ParamLMG_HCLM2SeasonMode);
#endif
#ifdef ParamLMG_HCLM3SeasonMode
    restore(3, ParamLMG_HCLM3SeasonMode);
#endif
#ifdef ParamLMG_HCLM4SeasonMode
    restore(4, ParamLMG_HCLM4SeasonMode);
#endif
#ifdef ParamLMG_HCLM5SeasonMode
    restore(5, ParamLMG_HCLM5SeasonMode);
#endif
#ifdef ParamLMG_HCLM6SeasonMode
    restore(6, ParamLMG_HCLM6SeasonMode);
#endif
#ifdef ParamLMG_HCLM7SeasonMode
    restore(7, ParamLMG_HCLM7SeasonMode);
#endif
#ifdef ParamLMG_HCLM8SeasonMode
    restore(8, ParamLMG_HCLM8SeasonMode);
#endif
#ifdef ParamLMG_HCLM9SeasonMode
    restore(9, ParamLMG_HCLM9SeasonMode);
#endif
#ifdef ParamLMG_HCLM10SeasonMode
    restore(10, ParamLMG_HCLM10SeasonMode);
#endif
#ifdef ParamLMG_HCLM11SeasonMode
    restore(11, ParamLMG_HCLM11SeasonMode);
#endif
#ifdef ParamLMG_HCLM12SeasonMode
    restore(12, ParamLMG_HCLM12SeasonMode);
#endif
#ifdef ParamLMG_HCLM13SeasonMode
    restore(13, ParamLMG_HCLM13SeasonMode);
#endif
#ifdef ParamLMG_HCLM14SeasonMode
    restore(14, ParamLMG_HCLM14SeasonMode);
#endif
#ifdef ParamLMG_HCLM15SeasonMode
    restore(15, ParamLMG_HCLM15SeasonMode);
#endif
#ifdef ParamLMG_HCLM16SeasonMode
    restore(16, ParamLMG_HCLM16SeasonMode);
#endif
}
