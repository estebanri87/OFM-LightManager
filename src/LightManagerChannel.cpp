#include "LightManagerChannel.h"
#include "LightManagerUtil.h"
#include "knxprod.h"
// Phase 2.H2: OFM-LogicModule integration for vacation KO + holiday calendar.
#include "Timer.h"
#include <cstdlib>  // std::abs (Phase 3 F1/F3 Preview-Delta)

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
    // Phase 2.J.b: legacy loadSetpoints/loadSummerSetpoints retired.
    // ProfileV2 is the sole source for HCL setpoints.
    loadProfilesV2();

    applyAdvanced(latitudeDeg, longitudeDeg, timezoneOffsetMin);
    loadAdaptive();
    loadLockFallbackParams();

    // Per-channel timing (replaces former global LMGHCLUpdateInterval / LMGHCLFadeDuration)
    _updateIntervalSec = ParamLMG_CHUpdateInterval;
    if (_updateIntervalSec < 60)   _updateIntervalSec = 60;
    if (_updateIntervalSec > 3600) _updateIntervalSec = 3600;
    _fadeDurationSec = ParamLMG_CHFadeDuration;
    if (_fadeDurationSec < 1)  _fadeDurationSec = 1;
    if (_fadeDurationSec > 60) _fadeDurationSec = 60;
    _lastPushedMs = 0;
}

void LightManagerChannel::applyAdvanced(float latitudeDeg, float longitudeDeg, int16_t timezoneOffsetMin)
{
    HCL::Master* m = master();
    if (!m) return;

    // Phase 2.J.c: master-level slew (setSlewRateKelvinPerMinute) retired.
    // Slew is now ProfileV2-resolved per channel (ParamLMG_CHSlewRateBrightness +
    // day/night variants). Master only needs geo + sunrise/sunset hints.
    m->setLocation(latitudeDeg, longitudeDeg);
    m->setTimezoneOffsetMinutes(timezoneOffsetMin);
    m->setSunTimes(ParamLMG_CHSunrise, ParamLMG_CHSunset);
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

    // Phase 2.G: run new ProfileV2 pipeline alongside legacy engine.
    // Outputs (_activeProfileIndex / _resolvedSetpoints) are computed but
    // not yet consumed by pushIfChanged() — wiring is staged for 2.H.
    if (hasTime && timeinfo != nullptr)
    {
        // Punkt 5 (F6): echte Astro-Werte aus Master::computeSolarPosition().
        const uint16_t dayOfYear   = static_cast<uint16_t>(timeinfo->tm_yday + 1);
        const uint16_t nowMinutes  = static_cast<uint16_t>(timeinfo->tm_hour * 60 + timeinfo->tm_min);
        int16_t solarDeclDeci      = INT16_MIN;
        int16_t solarAltCenti      = INT16_MIN;
        if (master())
            master()->computeSolarPosition(dayOfYear, nowMinutes, solarDeclDeci, solarAltCenti);

        // Season + DayNight: feed sentinels for inputs not yet wired (astro
        // declination / altitude). Engine falls back to legacy paths.
        HCL::SeasonInputs seasonIn{};
        seasonIn.todayMonthDay         = static_cast<uint16_t>((timeinfo->tm_mon + 1) * 100 + timeinfo->tm_mday);
        seasonIn.summerStartMonthDay   = static_cast<uint16_t>(ParamLMG_CHSummerStartMonth * 100 + ParamLMG_CHSummerStartDay);
        seasonIn.summerEndMonthDay     = static_cast<uint16_t>(ParamLMG_CHSummerEndMonth   * 100 + ParamLMG_CHSummerEndDay);
        seasonIn.koSummerActive        = master() ? master()->isSummer() : false;
        seasonIn.solarDeclinationDeci  = solarDeclDeci; // Punkt 5: real

        HCL::DayNightInputs dayNightIn{};
        // Phase 2.H: K06 (DPT 1.024 Day/Night) → true=Nacht.
        // Wird nur bei DayNightSource::FromKo (=1) ausgewertet.
        dayNightIn.koIsNight           = KoLMG_CHDayNight.value(DPT_Switch);
        dayNightIn.solarAltitudeCenti  = solarAltCenti; // Punkt 5: real

        const auto seasonSrc   = static_cast<HCL::SeasonSource>(ParamLMG_CHSeasonSource);
        const auto dayNightSrc = static_cast<HCL::DayNightSource>(ParamLMG_CHDayNightSource);
        evaluateSeasonAndDayNight(seasonSrc, seasonIn, dayNightSrc, dayNightIn);

        // Build day context: weekday tm_wday=Sun(0)..Sat(6) → Mon(0)..Sun(6).
        // Phase 2.H2: vacation from KoLOG_Vacation (DPT 1.001), holiday from
        // Timer::instance().holidayToday() (>0 = any holiday class today).
        HCL::DayContext ctx{};
        ctx.weekday    = static_cast<uint8_t>((timeinfo->tm_wday + 6) % 7);
        ctx.isSummer   = _cachedSummerActive;
        ctx.isWinter   = !_cachedSummerActive && (seasonSrc != HCL::SeasonSource::Off);
        ctx.isVacation = KoLOG_Vacation.value(DPT_Switch);
        ctx.isHoliday  = (Timer::instance().holidayToday() > 0);
        ctx.isFallback = false;
        updateActiveProfile(ctx);

        // Phase 2.H1: feed real astro from Master. CivilDawn/Dusk/SolarNoon
        // approximated per plan.md Step 5 (AstroSource=Manuell): ±30 min around
        // sunrise/sunset, solar noon = midpoint. TODO 2.H+: replace with true
        // civil twilight from solar calculations.
        HCL::AstroTimes astro{};
        if (master() && master()->hasSunTimes())
        {
            const int16_t sr = static_cast<int16_t>(master()->getSunriseMinutes());
            const int16_t ss = static_cast<int16_t>(master()->getSunsetMinutes());
            astro.sunrise   = sr;
            astro.sunset    = ss;
            astro.civilDawn = static_cast<int16_t>(sr - 30);
            astro.civilDusk = static_cast<int16_t>(ss + 30);
            astro.solarNoon = static_cast<int16_t>((sr + ss) / 2);
        }
        else
        {
            astro.sunrise = astro.sunset = astro.civilDawn = astro.civilDusk = astro.solarNoon = -1;
        }
        _lastAstroTimes = astro;

        // Punkt 5: DST-Cache-Invalidierung. Resolver nur bei Datums-/DST-/Profil-Wechsel.
        const uint16_t todayMonthDay = static_cast<uint16_t>((timeinfo->tm_mon + 1) * 100 + timeinfo->tm_mday);
        const int16_t  dstOffsetMin  = (timeinfo->tm_isdst > 0) ? 60 : 0;
        const bool dateChanged    = (_resolvedSetpointsValidForDate      != todayMonthDay);
        const bool dstChanged     = (_resolvedSetpointsValidForDstOffset != dstOffsetMin);
        const bool profileChanged = (_resolvedSetpointsValidForProfile   != _activeProfileIndex);
        if (dateChanged || dstChanged || profileChanged)
        {
            resolveActiveProfile(astro);
            _resolvedSetpointsValidForDate      = todayMonthDay;
            _resolvedSetpointsValidForDstOffset = dstOffsetMin;
            _resolvedSetpointsValidForProfile   = _activeProfileIndex;
        }

        // Phase 2.I: cache minute-of-day for pushIfChanged() interpolation.
        _lastTimeMinutes = static_cast<uint16_t>(timeinfo->tm_hour * 60 + timeinfo->tm_min);
    }

    evaluateLockFallback(timeinfo, hasTime);
}

void LightManagerChannel::updateSeason(const tm* timeinfo)
{
    HCL::Master* m = master();
    if (!m) return;

    // Phase 2.F: SeasonSource enum (0=Off, 1=Automatic, 2=FixedDate, 3=FromKo).
    // Automatic now means astro-declination based (was DST in 0.2.x). Until 2.G
    // wires the declination input via evaluateSeasonAndDayNight(), Automatic
    // falls back to DST-shim to preserve legacy behavior.
    const uint8_t seasonSource = ParamLMG_CHSeasonSource;
    if (seasonSource == 0)
    {
        m->setIsSummer(false);
    }
    else if (seasonSource == 1)
    {
        // TODO 2.G: replace DST-shim with HCL::computeSummerActive(Automatic,...)
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
    else if (seasonSource == 2)
    {
        m->setIsSummer(isDateInSummerRange(
            static_cast<uint8_t>(timeinfo->tm_mon + 1), static_cast<uint8_t>(timeinfo->tm_mday),
            ParamLMG_CHSummerStartMonth, ParamLMG_CHSummerStartDay,
            ParamLMG_CHSummerEndMonth,   ParamLMG_CHSummerEndDay));
    }
    // seasonSource == 3 (FromKo) -> set externally via K04 SummerActive
}

namespace
{
// Encode DPT 249.600 (PDT_GENERIC_06): TimePeriod(U16, 100ms) | ColourTemp(U16, K) | Brightness(U8, raw) | Mask(B8).
// Static mask 0x07 = TimePeriod+ColourTemp+Brightness valid.
void encodeDpt249_600(uint8_t* buf, uint16_t kelvin, uint8_t brightnessPct, uint8_t fadeSec)
{
    if (kelvin < 1000)  kelvin = 1000;
    if (kelvin > 40000) kelvin = 40000;
    if (brightnessPct > 100) brightnessPct = 100;

    const uint16_t timePeriod = static_cast<uint16_t>(fadeSec) * 10;
    const uint8_t  br8 = static_cast<uint8_t>((static_cast<uint16_t>(brightnessPct) * 255 + 50) / 100);

    buf[0] = static_cast<uint8_t>((timePeriod >> 8) & 0xFF);
    buf[1] = static_cast<uint8_t>(timePeriod & 0xFF);
    buf[2] = static_cast<uint8_t>((kelvin >> 8) & 0xFF);
    buf[3] = static_cast<uint8_t>(kelvin & 0xFF);
    buf[4] = br8;
    buf[5] = 0x07;
}
} // namespace

bool LightManagerChannel::internalOutputEnabled()
{
    // IntegrationMode: 0=Intern, 1=Extern, 2=Intern&Extern
    const uint8_t mode = ParamLMG_CHIntegrationMode;
    return (mode == 0 || mode == 2);
}

uint8_t LightManagerChannel::integrationMode()
{
    return ParamLMG_CHIntegrationMode;
}

uint8_t LightManagerChannel::astroSource()
{
    return ParamLMG_CHAstroSource;
}

// Phase 3 F11/L1: validMask fuer interne Sinks (Hue) berechnen.
// Eingeschraenkt durch StatusKoOutput-Caps + per-Axis-Locks (UseLock=Getrennt).
uint8_t LightManagerChannel::internalValidMask() const
{
    uint8_t mask = 0b11; // Bit 0 = Kelvin, Bit 1 = Brightness

    // StatusKoOutput Caps (0=K+B, 1=B, 2=K, 3=Combi, 4=All)
    const uint8_t output = ParamLMG_CHStatusKoOutput;
    const bool capsKelvin     = (output == 0 || output == 2 || output == 3 || output == 4);
    const bool capsBrightness = (output == 0 || output == 1 || output == 3 || output == 4);
    if (!capsKelvin)     mask &= ~0b01;
    if (!capsBrightness) mask &= ~0b10;

    // Per-Axis-Lock (UseLock=Getrennt) ODER Vollsperre.
    if (isColorLocked())      mask &= ~0b01;
    if (isBrightnessLocked()) mask &= ~0b10;

    return mask;
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

    // Phase 2.J.a: ProfileV2 is sole source. No resolved setpoints → hold
    // last value (no slew, no push). Prevents 0 K / 0 % glitch at boot when
    // no active profile is configured / config error.
    if (_resolvedCount == 0)
        return;

    if (_lastTimeMinutes != 0xFFFF)
        computeCurrentValueFromResolved(_lastTimeMinutes);

    // Punkt 6 (F12): externe Quellen K16-K19 vor Slew einrechnen, damit
    // applySlew() auf das effektive Ziel slewt. Brightness ist kanal-weit
    // (kein Per-SP-Modus); Kelvin nutzt zusätzlich den aktuellen SP für
    // Per-SP-Mode + ExtMixPercent.
    {
        const uint32_t now0 = millis();
        if (_lastTimeMinutes != 0xFFFF)
        {
            const uint8_t curIdx = _findCurrentSpIdx(_lastTimeMinutes);
            if (curIdx != 0xFF)
            {
                _currentValue.kelvin = getEffectiveColorTemp(
                    _currentValue.kelvin, _resolvedSetpoints[curIdx], now0);
            }
        }
        _currentValue.brightness = getEffectiveBrightness(_currentValue.brightness, now0);
    }

    // Phase 2.I.b: rate-limit toward _currentValue and write into local val.
    applySlew(millis());
    HCL::InterpolatedValue val = _currentValue;
    val.kelvin     = _appliedKelvin;
    val.brightness = _appliedBrightness;

    // Phase 2.K.2: Adaptive-Schicht anwenden (sofern aktiv). Der adaptierte Wert
    // ist die einzige Wahrheit fuer Bus-Send und interne Senken; das Routing
    // (Bus / intern / beide) regelt Variante E (IntegrationMode + StatusKoOutput).
    if (_master.getAdaptiveConfig().mode != HCL::AdaptiveMode::Disabled)
    {
        HCL::InterpolatedValue adapted = _master.applyAdaptiveBrightness(val, _lastTimeMinutes, millis());
        _adaptiveBrightness = adapted.brightness;
    }
    else
    {
        _adaptiveBrightness = _appliedBrightness;
    }
    val.brightness = _adaptiveBrightness;

    const uint32_t now = millis();
    const bool changed = (val.kelvin != _lastPushedValue.kelvin
                          || val.brightness != _lastPushedValue.brightness);
    const uint32_t intervalMs = static_cast<uint32_t>(_updateIntervalSec) * 1000UL;
    const bool periodicDue = (_lastPushedMs == 0)
                             || (intervalMs > 0 && (now - _lastPushedMs) >= intervalMs);
    if (!wasBlocked && !changed && !periodicDue)
    {
        // Phase 3 F1/F3/F4: Preview/Progress kann sich ändern (zeitabhängig),
        // selbst wenn Hauptwert konstant — eigene Delta-Filter im Helper.
        _publishPreviewAndProgress();
        return;
    }

    _lastPushedValue = val;
    _lastPushedMs    = now;

    // External KO emission is controlled by per-channel ETS parameters (Variante E):
    //   IntegrationMode: 0=Intern, 1=Extern, 2=Intern&Extern
    //   BusStatusEnable (bool): explicit gate for Intern / Intern+Extern modes
    //   StatusKoOutput: 0=1B+2B, 1=1B only, 2=2B only, 3=6B combi, 4=all parallel
    // Note: for Extern (mode==1), BusStatusEnable is hidden in ETS and always 0;
    // the ETS template unconditionally shows the status KOs for Extern mode because
    // sending to the bus IS the purpose of Extern mode. busOn only gates Intern-derived modes.
    const uint8_t mode    = ParamLMG_CHIntegrationMode;
    const bool    busOn   = ParamLMG_CHBusStatusEnable;
    const uint8_t output  = ParamLMG_CHStatusKoOutput;
    const bool    externAllowed = (mode == 1) || busOn;

    if (externAllowed)
    {
        uint16_t kelvinClamped = val.kelvin;
        if (kelvinClamped < 1000)  kelvinClamped = 1000;
        if (kelvinClamped > 40000) kelvinClamped = 40000;
        uint8_t brightnessClamped = val.brightness;
        if (brightnessClamped > 100) brightnessClamped = 100;

        const bool send1B   = (output == 0 || output == 1 || output == 4);
        const bool send2B   = (output == 0 || output == 2 || output == 4);
        const bool sendCombi= (output == 3 || output == 4);

        // Phase 3 F1/L7: per-Achse zusätzlich gegen HclAxes + TimeWindow + Lock filtern.
        if (send1B && _shouldSendAxis(1) && !isBrightnessLocked())
            knx.getGroupObject(LMG_KoCalcNumber(LMG_KoCHStatusBrightness)).value(brightnessClamped, Dpt(5, 1));
        if (send2B && _shouldSendAxis(0) && !isColorLocked())
            knx.getGroupObject(LMG_KoCalcNumber(LMG_KoCHStatusColorTemp)).value(kelvinClamped, Dpt(7, 600));
        if (sendCombi)
        {
            uint8_t buf[6];
            encodeDpt249_600(buf, kelvinClamped, brightnessClamped, _fadeDurationSec);
            GroupObject& go = knx.getGroupObject(LMG_KoCalcNumber(LMG_KoCHStatusCombined));
            uint8_t* ref = go.valueRef();
            if (ref != nullptr)
            {
                memcpy(ref, buf, 6);
                go.objectWritten();
            }
        }
    }

    // Phase 3 F1/F3/F4/L7: Preview-/Progress-/Phase-Sends (eigener Delta-Filter,
    // unabhängig vom IntegrationMode/BusStatusEnable-Gate — Tools für Visu).
    _publishPreviewAndProgress();
}

// ---------------------------------------------------------------------------
// Phase 3/4: Axis-Gate + Preview/Progress helpers
// ---------------------------------------------------------------------------

bool LightManagerChannel::_shouldSendAxis(uint8_t axis) const
{
    // HclAxes: 0=KH, 1=NurFarbe, 2=NurHellig, 3=Aus
    const uint8_t axes = ParamLMG_CHHclAxes;
    if (axes == 3) return false;
    if (axis == 0) { // Kelvin
        if (axes != 0 && axes != 1) return false;
    } else if (axis == 1) { // Brightness
        if (axes != 0 && axes != 2) return false;
    }
    // axis == 2 (Meta): nur HclAxes != 3 reicht.

    // HclTimeWindow: 0=Immer, 1=NurTag, 2=NurNacht
    const uint8_t tw = ParamLMG_CHHclTimeWindow;
    if (tw == 1 && _cachedIsNight) return false;
    if (tw == 2 && !_cachedIsNight) return false;
    return true;
}

bool LightManagerChannel::_getNextSetpoint(uint16_t nowMin, uint16_t& outK,
                                           uint8_t& outB, uint16_t& outMinutesUntil) const
{
    if (_resolvedCount == 0) { outMinutesUntil = 0xFFFF; return false; }

    // _resolvedSetpoints wurde von computeCurrentValueFromResolved() ascending sortiert.
    for (uint8_t i = 0; i < _resolvedCount; ++i)
    {
        if (_resolvedSetpoints[i].timeMinutes > nowMin)
        {
            outK = _resolvedSetpoints[i].kelvin;
            outB = _resolvedSetpoints[i].brightness;
            outMinutesUntil = static_cast<uint16_t>(_resolvedSetpoints[i].timeMinutes - nowMin);
            return true;
        }
    }
    // Wrap: erster SP morgen.
    outK = _resolvedSetpoints[0].kelvin;
    outB = _resolvedSetpoints[0].brightness;
    outMinutesUntil = static_cast<uint16_t>(1440u - nowMin + _resolvedSetpoints[0].timeMinutes);
    return true;
}

uint8_t LightManagerChannel::_computeDayProgress(uint16_t nowMin) const
{
    if (_resolvedCount < 2) return 0;
    const uint16_t first = _resolvedSetpoints[0].timeMinutes;
    const uint16_t last  = _resolvedSetpoints[_resolvedCount - 1].timeMinutes;
    if (nowMin <= first) return 0;
    if (nowMin >= last)  return 100;
    const uint32_t span = static_cast<uint32_t>(last - first);
    if (span == 0) return 0;
    return static_cast<uint8_t>((static_cast<uint32_t>(nowMin - first) * 100u) / span);
}

uint8_t LightManagerChannel::_computeDayPhase(uint16_t nowMin) const
{
    // Punkt 5: Astro-basierte Tagesphasen 0..5 wenn Astro-Times valide,
    // sonst Fallback auf Stunden-Buckets.
    if (_cachedIsNight) return 0; // Nacht (Sonne <= -0.83°)

    const HCL::AstroTimes& a = _lastAstroTimes;
    const bool astroValid = (a.sunrise   >= 0 && a.sunrise   < 1440)
                         && (a.sunset    >= 0 && a.sunset    < 1440)
                         && (a.solarNoon >= 0 && a.solarNoon < 1440);
    if (astroValid)
    {
        // 1 = Morgendämmerung (vor Sunrise)
        if (static_cast<int16_t>(nowMin) < a.sunrise) return 1;
        const int16_t noonBandHalf = 30;
        // 2 = Vormittag (Sunrise .. SolarNoon-30)
        if (static_cast<int16_t>(nowMin) <  a.solarNoon - noonBandHalf) return 2;
        // 3 = Mittag (SolarNoon ± 30 min)
        if (static_cast<int16_t>(nowMin) <= a.solarNoon + noonBandHalf) return 3;
        // 4 = Nachmittag (.. Sunset)
        if (static_cast<int16_t>(nowMin) <  a.sunset) return 4;
        // 5 = Abenddämmerung
        return 5;
    }

    // Fallback (keine Astro): Stunden-Buckets.
    const uint16_t h = nowMin / 60;
    if (h < 9)   return 1;
    if (h < 11)  return 2;
    if (h < 14)  return 3;
    if (h < 18)  return 4;
    return 5;
}

// Punkt 6 (F12): Index des aktuellen (jüngsten <= nowMin) SP im sortierten
// _resolvedSetpoints[]. 0xFF wenn leer. Wrap auf letzten SP wenn nowMin <
// erstem SP (Mitternachts-Wrap, deckungsgleich zu computeCurrentValueFromResolved()).
uint8_t LightManagerChannel::_findCurrentSpIdx(uint16_t nowMin) const
{
    if (_resolvedCount == 0) return 0xFF;
    uint8_t prev = static_cast<uint8_t>(_resolvedCount - 1);
    for (uint8_t i = 0; i < _resolvedCount; ++i)
    {
        if (_resolvedSetpoints[i].timeMinutes <= nowMin)
            prev = i;
    }
    return prev;
}

// Punkt 6 (F12): Externe Kelvin-Quelle einrechnen.
// Kanal-Source (ExtColorTempSource): 0=Nein, 1=Override, 2=Fallback.
// Per-SP-Mode (ExtColorTempMode): Off/Always/OnlyGreater/OnlySmaller.
// Mix-Anteil (ExtMixPercent 0..100) bestimmt SP/Ext-Verhältnis.
// Frische-Check via ExtFallbackTimeoutSec (0 = kein Timeout).
uint16_t LightManagerChannel::getEffectiveColorTemp(uint16_t interpolated,
                                                    const HCL::ResolvedSetpoint& sp,
                                                    uint32_t nowMs) const
{
    const uint8_t src = ParamLMG_CHExtColorTempSource;
    if (src == 0) return interpolated;
    if (_extKelvinValue == 0) return interpolated;

    const uint16_t timeoutSec = ParamLMG_CHExtFallbackTimeoutSec;
    if (timeoutSec > 0
        && (nowMs - _extKelvinLastUpdateMs) > static_cast<uint32_t>(timeoutSec) * 1000UL)
        return interpolated;

    const uint16_t extK = _extKelvinValue;

    bool mixActive = false;
    switch (sp.extColorTempMode)
    {
        case HCL::ExtColorTempMode::Off:         mixActive = false; break;
        case HCL::ExtColorTempMode::Always:      mixActive = true;  break;
        case HCL::ExtColorTempMode::OnlyGreater: mixActive = (extK > interpolated); break;
        case HCL::ExtColorTempMode::OnlySmaller: mixActive = (extK < interpolated); break;
    }
    if (!mixActive) return interpolated;

    uint8_t mix = sp.extMixPercent;
    if (mix > 100) mix = 100;
    const uint32_t result = (static_cast<uint32_t>(interpolated) * (100U - mix)
                              + static_cast<uint32_t>(extK) * mix) / 100U;
    uint32_t clamped = result;
    if (clamped < 1500U)  clamped = 1500U;
    if (clamped > 10000U) clamped = 10000U;
    return static_cast<uint16_t>(clamped);
}

// Punkt 6 (F12): Externe Helligkeits-Quelle einrechnen.
// Override/Fallback verhalten sich hier identisch: solange frisch ersetzt
// die externe Quelle den interpolierten Wert vollstaendig.
uint8_t LightManagerChannel::getEffectiveBrightness(uint8_t interpolated,
                                                    uint32_t nowMs) const
{
    const uint8_t src = ParamLMG_CHExtBrightnessSource;
    if (src == 0) return interpolated;
    if (_extBrightnessValue == 0xFF) return interpolated;

    const uint16_t timeoutSec = ParamLMG_CHExtFallbackTimeoutSec;
    if (timeoutSec > 0
        && (nowMs - _extBrightnessLastUpdateMs) > static_cast<uint32_t>(timeoutSec) * 1000UL)
        return interpolated;

    uint8_t v = _extBrightnessValue;
    if (v > 100) v = 100;
    return v;
}

void LightManagerChannel::_publishPreviewAndProgress()
{
    if (_lastTimeMinutes == 0xFFFF) return;
    if (_resolvedCount == 0) return;

    const uint16_t nowMin = _lastTimeMinutes;

    // K11/K12/K13: Vorausschau (PreviewEnable + axes-Gate).
    if (ParamLMG_CHPreviewEnable && _shouldSendAxis(2))
    {
        uint16_t nk = 0; uint8_t nb = 0; uint16_t nmin = 0;
        if (_getNextSetpoint(nowMin, nk, nb, nmin))
        {
            // K11 PreviewMinutes (DPT 7-6, Δ ≥ 1 Minute).
            if (_lastPushedPreviewMinutes < 0
                || std::abs(static_cast<int32_t>(nmin) - _lastPushedPreviewMinutes) >= 1)
            {
                knx.getGroupObject(LMG_KoCalcNumber(LMG_KoCHPreviewMinutes)).value(nmin, Dpt(7, 6));
                _lastPushedPreviewMinutes = static_cast<int16_t>(nmin);
            }
            // K12 PreviewColorTemp (DPT 7-600, Δ ≥ 50 K, nur wenn Kelvin-Achse aktiv & nicht gesperrt).
            if (_shouldSendAxis(0) && !isColorLocked())
            {
                if (_lastPushedPreviewKelvin == 0
                    || std::abs(static_cast<int32_t>(nk) - _lastPushedPreviewKelvin) >= 50)
                {
                    knx.getGroupObject(LMG_KoCalcNumber(LMG_KoCHPreviewColorTemp)).value(nk, Dpt(7, 600));
                    _lastPushedPreviewKelvin = nk;
                }
            }
            // K13 PreviewBrightness (DPT 5-1, Δ ≥ 1 %, nur wenn Brightness-Achse aktiv & nicht gesperrt).
            if (_shouldSendAxis(1) && !isBrightnessLocked())
            {
                if (_lastPushedPreviewBrightness < 0
                    || std::abs(static_cast<int32_t>(nb) - _lastPushedPreviewBrightness) >= 1)
                {
                    knx.getGroupObject(LMG_KoCalcNumber(LMG_KoCHPreviewBrightness)).value(nb, Dpt(5, 1));
                    _lastPushedPreviewBrightness = static_cast<int16_t>(nb);
                }
            }
        }
    }

    // K14/K15: Fortschritt + Phase (ProgressEnable + axes-Gate).
    if (ParamLMG_CHProgressEnable && _shouldSendAxis(2))
    {
        const uint8_t progress = _computeDayProgress(nowMin);
        if (_lastPushedDayProgress < 0 || progress != _lastPushedDayProgress)
        {
            knx.getGroupObject(LMG_KoCalcNumber(LMG_KoCHDayProgress)).value(progress, Dpt(5, 1));
            _lastPushedDayProgress = static_cast<int16_t>(progress);
        }

        const uint8_t phase = _computeDayPhase(nowMin);
        if (_lastPushedDayPhase < 0 || phase != _lastPushedDayPhase)
        {
            knx.getGroupObject(LMG_KoCalcNumber(LMG_KoCHDayPhase)).value(phase, Dpt(5, 10));
            _lastPushedDayPhase = static_cast<int16_t>(phase);
        }
    }
}

// ---------------------------------------------------------------------------
// KO routing
// ---------------------------------------------------------------------------

bool LightManagerChannel::processChannelKo(GroupObject& ko, uint16_t channelKoIndex)
{
    const uint8_t useLock = ParamLMG_CHUseLock; // 0=Nein, 1=Vollsperre, 2=Getrennt

    switch (channelKoIndex)
    {
        case LMG_KoCHLock:
            // Phase 3 F2/L2: UseLock=Nein → KO ignorieren.
            if (useLock == 0) return true;
            setLock(ko.value(Dpt(1, 1)), "KO");
            return true;

        case LMG_KoCHLockColor:
            // Phase 3 F2/L2: K09 nur bei UseLock=Getrennt aktiv.
            if (useLock != 2) return true;
            setLockColor(ko.value(Dpt(1, 3)), "KO");
            return true;

        case LMG_KoCHLockBrightness:
            // Phase 3 F2/L2: K10 nur bei UseLock=Getrennt aktiv.
            if (useLock != 2) return true;
            setLockBrightness(ko.value(Dpt(1, 3)), "KO");
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
            // Phase 2.K.2: kein forceUpdate() mehr — die Adaptive-Schicht wird
            // im nächsten regulären pushIfChanged()-Tick neu berechnet.
            publishAdaptiveActive();
            return true;
        }

        case LMG_KoCHDayNight:
            _master.setDaytime(ko.value(Dpt(1, 1)));
            return true;

        // Punkt 6 (F12): externe Quellen K16-K19 mit L6-Konvertierung.
        // DPT-Alias-Logik: K16/K17 sind Brightness-Alias-Slots (1B Prozent
        // bzw. 2B Lux), K18/K19 sind ColorTemp-Alias-Slots (2B Kelvin bzw.
        // 1B Skalar). Nur der via ExtBrightnessDpt/ExtColorTempDpt gewaehlte
        // Slot wird ausgewertet; das ETS-Schema blendet den anderen ohnehin aus.
        case LMG_KoCHExtBrightnessPercent:
        {
            // ExtBrightnessDpt: 0=1B Prozent, 1=2B Lux
            if (ParamLMG_CHExtBrightnessDpt != 0) return true;
            uint8_t pct = ko.value(Dpt(5, 1));
            if (pct > 100) pct = 100;
            _extBrightnessValue        = pct;
            _extBrightnessLastUpdateMs = millis();
            return true;
        }

        case LMG_KoCHExtBrightnessLux:
        {
            if (ParamLMG_CHExtBrightnessDpt != 1) return true;
            const float lux = ko.value(Dpt(9, 4));
            uint16_t luxMax = ParamLMG_CHExtLuxMax;
            if (luxMax == 0) luxMax = 500;
            int32_t pct = static_cast<int32_t>(lux * 100.0f / static_cast<float>(luxMax));
            if (pct < 0)   pct = 0;
            if (pct > 100) pct = 100;
            _extBrightnessValue        = static_cast<uint8_t>(pct);
            _extBrightnessLastUpdateMs = millis();
            return true;
        }

        case LMG_KoCHExtColorTempKelvin:
        {
            // ExtColorTempDpt: 0=2B Kelvin, 1=1B Skalar
            if (ParamLMG_CHExtColorTempDpt != 0) return true;
            uint16_t k = ko.value(Dpt(7, 600));
            if (k < 1500)  k = 1500;
            if (k > 10000) k = 10000;
            _extKelvinValue        = k;
            _extKelvinLastUpdateMs = millis();
            return true;
        }

        case LMG_KoCHExtColorTempScalar:
        {
            if (ParamLMG_CHExtColorTempDpt != 1) return true;
            // DPT-5-6 (0..255): Skalar -> Kelvin linear via ExtKelvinMin/Max.
            uint8_t scalar = ko.value(Dpt(5, 1));
            uint16_t kMin = ParamLMG_CHExtKelvinMin;
            uint16_t kMax = ParamLMG_CHExtKelvinMax;
            if (kMin == 0) kMin = 2700;
            if (kMax == 0) kMax = 6500;
            if (kMax < kMin) { const uint16_t tmp = kMin; kMin = kMax; kMax = tmp; }
            uint32_t k = static_cast<uint32_t>(kMin)
                       + (static_cast<uint32_t>(scalar) * (kMax - kMin)) / 255U;
            if (k < 1500)  k = 1500;
            if (k > 10000) k = 10000;
            _extKelvinValue        = static_cast<uint16_t>(k);
            _extKelvinLastUpdateMs = millis();
            return true;
        }

        default:
            return false;
    }
}

// ---------------------------------------------------------------------------
// Lock state machine
// ---------------------------------------------------------------------------

void LightManagerChannel::setLock(bool active, const char* reason)
{
    // Phase 3 F2/L2: UseLock=Nein → K02 hat keinen Effekt.
    if (ParamLMG_CHUseLock == 0) return;

    if (active && static_cast<LockFallbackPolicy>(_lockFallbackPolicy) == LockFallbackPolicy::Disabled)
        return;

    const bool changed = (_lockActive != active);
    _lockActive = active;
    _applyBlocked = active;

    if (active)
    {
        armLockFallbackTimer();
        if (changed)
            Serial.printf("[LMG ch%u] lock enabled (%s)\n", (unsigned)masterNumber(), reason ? reason : "n/a");
    }
    else
    {
        disarmLockFallbackTimerIfAllReleased();
        if (changed)
            Serial.printf("[LMG ch%u] lock disabled (%s)\n", (unsigned)masterNumber(), reason ? reason : "n/a");
    }

    publishLockStatus();
}

// Phase 3 F2/L2: Per-Axis-Lock (UseLock=Getrennt). K02 dominiert über K09/K10.
// Wirksamkeit im Output-Pfad via isColorLocked()/isBrightnessLocked() in pushIfChanged()
// und onLightManagerPartial-Caller (folgende Phase-3-Punkte).
void LightManagerChannel::setLockColor(bool active, const char* reason)
{
    if (_lockColorActive == active) return;
    _lockColorActive = active;
    Serial.printf("[LMG ch%u] lock-color %s (%s)\n",
                  (unsigned)masterNumber(), active ? "enabled" : "disabled",
                  reason ? reason : "n/a");
    if (active) armLockFallbackTimer();
    else        disarmLockFallbackTimerIfAllReleased();
    invalidatePushedState();
}

void LightManagerChannel::setLockBrightness(bool active, const char* reason)
{
    if (_lockBrightnessActive == active) return;
    _lockBrightnessActive = active;
    Serial.printf("[LMG ch%u] lock-brightness %s (%s)\n",
                  (unsigned)masterNumber(), active ? "enabled" : "disabled",
                  reason ? reason : "n/a");
    if (active) armLockFallbackTimer();
    else        disarmLockFallbackTimerIfAllReleased();
    invalidatePushedState();
}

void LightManagerChannel::armLockFallbackTimer()
{
    // Idempotent: nur setzen wenn Timer noch nicht gestartet.
    if (_lockActivatedMs != 0) return;

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
    if (LightManagerUtil::tryGetLocalTime(timeinfo))
    {
        _lockActivationDayOfYear   = static_cast<int16_t>(timeinfo.tm_yday);
        _lockActivationMinuteOfDay = static_cast<int16_t>(timeinfo.tm_hour * 60 + timeinfo.tm_min);
    }
}

void LightManagerChannel::disarmLockFallbackTimerIfAllReleased()
{
    if (anyLockActive()) return;
    _lockActivatedMs = 0;
    _lockAutoReleaseMs = 0;
    _lockActivationDayOfYear = -1;
    _lockActivationMinuteOfDay = -1;
}

void LightManagerChannel::releaseAllAxisLocks(const char* reason)
{
    // Reihenfolge: per-Axis zuerst (kein Timer-Touch), dann K02 (schaltet Timer ab).
    if (_lockColorActive)      setLockColor(false, reason);
    if (_lockBrightnessActive) setLockBrightness(false, reason);
    if (_lockActive)           setLock(false, reason);
}

void LightManagerChannel::evaluateLockFallback(const tm* timeinfo, bool hasTime)
{
    // Phase 3 F2/L2: bei UseLock=Nein keinen Fallback-Tick durchführen.
    if (ParamLMG_CHUseLock == 0) return;

    // Phase 3 F2: Fallback entsperrt alle aktiven Per-Kanal-Locks (K02 ∪ K09 ∪ K10) einheitlich.
    if (!_lockActive && !_lockColorActive && !_lockBrightnessActive) return;

    const LockFallbackPolicy policy = static_cast<LockFallbackPolicy>(_lockFallbackPolicy);
    if (policy == LockFallbackPolicy::ExternalOnly) return;

    if (policy != LockFallbackPolicy::Legacy)
    {
        const bool byDuration = (_lockAutoReleaseMs != 0) &&
                                (static_cast<long>(millis() - _lockAutoReleaseMs) >= 0);
        const bool byTime = (policy == LockFallbackPolicy::TimeOfDay || policy == LockFallbackPolicy::DurationOrTime) &&
                            shouldReleaseByPolicyTime(timeinfo, hasTime);
        if (byDuration || byTime)
            releaseAllAxisLocks(byTime ? "fallback release time" : "fallback duration elapsed");
        return;
    }

    // Legacy
    const LockFallbackMode mode = static_cast<LockFallbackMode>(_lockFallbackMode);
    if (mode == LockFallbackMode::None) return;

    if (_lockAutoReleaseMs != 0)
    {
        if (static_cast<long>(millis() - _lockAutoReleaseMs) >= 0)
            releaseAllAxisLocks("fallback duration elapsed");
        return;
    }

    if (mode == LockFallbackMode::NextDay && hasTime && timeinfo != nullptr)
    {
        if (_lockActivationDayOfYear >= 0 && timeinfo->tm_yday != _lockActivationDayOfYear)
            releaseAllAxisLocks("fallback day change");
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

uint8_t LightManagerChannel::effectiveBrightness() const
{
    // Phase 2.K.2: interne Senken (Hue) lesen den adaptierten Wert, sofern die
    // Adaptive-Schicht ihn berechnet hat; sonst den reinen HCL-Slew-Wert.
    if (_adaptiveBrightness != 0xFF)
        return _adaptiveBrightness;
    return _appliedBrightness;
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
    if (ParamLMG_CHSeasonSource != 3) return; // only FromKo persists
    HCL::Master* m = master();
    if (!m) return;
    const bool wasSummer = (mask & static_cast<uint16_t>(1u << _channelIndex)) != 0;
    m->setIsSummer(wasSummer);
}

// ---------------------------------------------------------------------------
// Phase 2.B: profile-storage loader (additive; not yet consumed by engine).
// Reads per-channel profile slots 1..4 from ETS params into _profilesV2[].
// Selector / Resolver hookup follows in Phase 2.C/2.D.
// ---------------------------------------------------------------------------

#include <cstring>

#define LMG_LOAD_SP(P, S)                                                                                  \
    do                                                                                                     \
    {                                                                                                      \
        HCL::SetpointV2& sp = pr.sps[(S) - 1];                                                             \
        sp.active           = ParamLMG_CHP##P##_SP##S##_Active;                                            \
        sp.anchorType       = static_cast<HCL::AnchorType>(ParamLMG_CHP##P##_SP##S##_AnchorType);          \
        sp.hour             = ParamLMG_CHP##P##_SP##S##_Hour;                                              \
        sp.minute           = ParamLMG_CHP##P##_SP##S##_Minute;                                            \
        sp.offsetMinutes    = ParamLMG_CHP##P##_SP##S##_OffsetMinutes;                                     \
        sp.clampMode        = static_cast<HCL::ClampMode>(ParamLMG_CHP##P##_SP##S##_ClampMode);            \
        sp.clampHour        = ParamLMG_CHP##P##_SP##S##_ClampHour;                                         \
        sp.clampMinute      = ParamLMG_CHP##P##_SP##S##_ClampMinute;                                       \
        sp.kelvin           = ParamLMG_CHP##P##_SP##S##_Kelvin;                                            \
        sp.brightness       = ParamLMG_CHP##P##_SP##S##_Brightness;                                        \
        sp.extColorTempMode = static_cast<HCL::ExtColorTempMode>(ParamLMG_CHP##P##_SP##S##_ExtColorTempMode); \
        sp.extMixPercent    = ParamLMG_CHP##P##_SP##S##_ExtMixPercent;                                     \
    } while (0)

#define LMG_LOAD_PROFILE(P)                                                                                \
    do                                                                                                     \
    {                                                                                                      \
        HCL::ProfileV2& pr = _profilesV2[(P) - 1];                                                         \
        pr.active          = ParamLMG_CHP##P##_Active;                                                     \
        const auto nmStr   = ParamLMG_CHP##P##_NameStr;                                                    \
        const char* nm     = nmStr.c_str();                                                                \
        std::memset(pr.name, 0, sizeof(pr.name));                                                          \
        if (nm) std::strncpy(pr.name, nm, sizeof(pr.name) - 1);                                            \
        pr.weekdayMask     =   (ParamLMG_CHP##P##_DayMo  ? HCL::WeekdayMaskBits::Monday    : 0)            \
                             | (ParamLMG_CHP##P##_DayDi  ? HCL::WeekdayMaskBits::Tuesday   : 0)            \
                             | (ParamLMG_CHP##P##_DayMi  ? HCL::WeekdayMaskBits::Wednesday : 0)            \
                             | (ParamLMG_CHP##P##_DayDo  ? HCL::WeekdayMaskBits::Thursday  : 0)            \
                             | (ParamLMG_CHP##P##_DayFr  ? HCL::WeekdayMaskBits::Friday    : 0)            \
                             | (ParamLMG_CHP##P##_DaySa  ? HCL::WeekdayMaskBits::Saturday  : 0)            \
                             | (ParamLMG_CHP##P##_DaySo  ? HCL::WeekdayMaskBits::Sunday    : 0)            \
                             | (ParamLMG_CHP##P##_DayUrl ? HCL::WeekdayMaskBits::Vacation  : 0)            \
                             | (ParamLMG_CHP##P##_DayFei ? HCL::WeekdayMaskBits::Holiday   : 0);           \
        switch (ParamLMG_CHP##P##_SeasonFilter)                                                            \
        {                                                                                                  \
            case 1: pr.weekdayMask |= HCL::WeekdayMaskBits::Summer;   break;                              \
            case 2: pr.weekdayMask |= HCL::WeekdayMaskBits::Winter;   break;                              \
            case 3: pr.weekdayMask |= HCL::WeekdayMaskBits::Fallback; break;                              \
            default: break; /* 0 = Beide: keine zusätzlichen Bits */                                       \
        }                                                                                                  \
        uint8_t cnt        = ParamLMG_CHP##P##_SPCount;                                                    \
        if (cnt > HCL::ProfileV2::MAX_SETPOINTS) cnt = HCL::ProfileV2::MAX_SETPOINTS;                      \
        pr.spCount         = cnt;                                                                          \
        LMG_LOAD_SP(P, 1);  LMG_LOAD_SP(P, 2);  LMG_LOAD_SP(P, 3);  LMG_LOAD_SP(P, 4);  LMG_LOAD_SP(P, 5); \
        LMG_LOAD_SP(P, 6);  LMG_LOAD_SP(P, 7);  LMG_LOAD_SP(P, 8);  LMG_LOAD_SP(P, 9);  LMG_LOAD_SP(P, 10);\
    } while (0)

void LightManagerChannel::loadProfilesV2()
{
    LMG_LOAD_PROFILE(1);
    LMG_LOAD_PROFILE(2);
    LMG_LOAD_PROFILE(3);
    LMG_LOAD_PROFILE(4);
}

#undef LMG_LOAD_PROFILE
#undef LMG_LOAD_SP

// Phase 2.C: select active profile slot from day context (additive; not yet
// consumed by engine — resolver in 2.D will read _activeProfileIndex).
void LightManagerChannel::updateActiveProfile(const HCL::DayContext& ctx)
{
    // Punkt 4: Profil-Selektor respektiert nun ProfileCount (L5) + Bit-9-Exklusiv +
    // Spezifitaets-Scoring (Feiertag > Urlaub > Saison+WT > WT).
    uint8_t profileCount = ParamLMG_CHProfileCount;
    if (profileCount == 0) profileCount = 1; // ETS-Default ist >=1; defensiv.
    if (profileCount > 4)  profileCount = 4;
    _activeProfileIndex = HCL::selectProfile(_profilesV2, ctx, profileCount);
}

// Phase 2.D: resolve currently active profile against today's astro times.
// Output stays in declaration order; sorting / interpolation comes in 2.E/2.F.
void LightManagerChannel::resolveActiveProfile(const HCL::AstroTimes& astro)
{
    if (_activeProfileIndex < 0 || _activeProfileIndex >= 4)
    {
        _resolvedCount = 0;
        return;
    }
    _resolvedCount = HCL::resolveProfile(
        _profilesV2[_activeProfileIndex], astro, _resolvedSetpoints);
}

// Phase 2.E: cache Season + Day/Night flags computed from per-channel sources.
void LightManagerChannel::evaluateSeasonAndDayNight(
    HCL::SeasonSource seasonSrc, const HCL::SeasonInputs& seasonIn,
    HCL::DayNightSource dayNightSrc, const HCL::DayNightInputs& dayNightIn)
{
    _cachedSummerActive = HCL::computeSummerActive(seasonSrc, seasonIn);
    _cachedIsNight      = HCL::computeIsNight(dayNightSrc, dayNightIn);
}

// Phase 2.I: linear interpolation of _currentValue from _resolvedSetpoints[].
// Sorts the resolved array in-place ascending by timeMinutes (insertion sort,
// small N <= MAX_SETPOINTS=10). Brackets the current minute with prev/next SP
// (wrap-around midnight) and interpolates kelvin + brightness independently.
// Slew-rate is intentionally not applied here \u2014 deferred to 2.I.b.
void LightManagerChannel::computeCurrentValueFromResolved(uint16_t minuteOfDay)
{
    if (_resolvedCount == 0) return;

    if (_resolvedCount == 1)
    {
        _currentValue.kelvin     = _resolvedSetpoints[0].kelvin;
        _currentValue.brightness = _resolvedSetpoints[0].brightness;
        return;
    }

    // Insertion sort ascending by timeMinutes (stable, in-place, O(n^2) on n<=10).
    for (uint8_t i = 1; i < _resolvedCount; ++i)
    {
        const HCL::ResolvedSetpoint key = _resolvedSetpoints[i];
        int8_t j = static_cast<int8_t>(i) - 1;
        while (j >= 0 && _resolvedSetpoints[j].timeMinutes > key.timeMinutes)
        {
            _resolvedSetpoints[j + 1] = _resolvedSetpoints[j];
            --j;
        }
        _resolvedSetpoints[j + 1] = key;
    }

    // Bracket: prev = last SP with timeMinutes <= now; next = following SP.
    // Wrap-around: if now < sp[0].time, prev wraps to sp[last] with virtual
    // time (-1440 + last.time); if now > sp[last].time, next wraps to sp[0]
    // with virtual time (1440 + first.time).
    uint8_t prevIdx = static_cast<uint8_t>(_resolvedCount - 1);
    int32_t prevMin = static_cast<int32_t>(_resolvedSetpoints[prevIdx].timeMinutes) - 1440;
    uint8_t nextIdx = 0;
    int32_t nextMin = static_cast<int32_t>(_resolvedSetpoints[0].timeMinutes);

    for (uint8_t i = 0; i < _resolvedCount; ++i)
    {
        if (_resolvedSetpoints[i].timeMinutes <= minuteOfDay)
        {
            prevIdx = i;
            prevMin = _resolvedSetpoints[i].timeMinutes;
            nextIdx = static_cast<uint8_t>((i + 1) % _resolvedCount);
            nextMin = (nextIdx == 0)
                        ? static_cast<int32_t>(_resolvedSetpoints[0].timeMinutes) + 1440
                        : static_cast<int32_t>(_resolvedSetpoints[nextIdx].timeMinutes);
        }
    }

    const int32_t span = nextMin - prevMin;
    if (span <= 0)
    {
        _currentValue.kelvin     = _resolvedSetpoints[prevIdx].kelvin;
        _currentValue.brightness = _resolvedSetpoints[prevIdx].brightness;
        return;
    }

    const int32_t t  = static_cast<int32_t>(minuteOfDay) - prevMin;
    const int32_t pk = _resolvedSetpoints[prevIdx].kelvin;
    const int32_t nk = _resolvedSetpoints[nextIdx].kelvin;
    const int32_t pb = _resolvedSetpoints[prevIdx].brightness;
    const int32_t nb = _resolvedSetpoints[nextIdx].brightness;

    _currentValue.kelvin     = static_cast<uint16_t>(pk + (nk - pk) * t / span);
    _currentValue.brightness = static_cast<uint8_t> (pb + (nb - pb) * t / span);
}

// Phase 2.I.b: rate-limit applied output toward _currentValue target.
// - Sources: per-channel ETS params ParamLMG_CHSlewRate (Kelvin K/min,
//   uint16) and ParamLMG_CHSlewRateBrightness (%/min, uint8).
// - Rate == 0 disables slew on that axis (instant snap).
// - First call or profile-switch (5a) snaps both axes to target.
// - State: _appliedKelvin / _appliedBrightness / _lastSlewMs / _lastSlewProfileIdx.
void LightManagerChannel::applySlew(uint32_t nowMs)
{
    const uint16_t targetK = _currentValue.kelvin;
    const uint8_t  targetB = _currentValue.brightness;

    const bool snap = (_lastSlewProfileIdx == -2)
                      || (_lastSlewProfileIdx != _activeProfileIndex)
                      || (_appliedKelvin == 0xFFFF)
                      || (_appliedBrightness == 0xFF);

    if (snap)
    {
        _appliedKelvin      = targetK;
        _appliedBrightness  = targetB;
        _lastSlewMs         = nowMs;
        _lastSlewProfileIdx = _activeProfileIndex;
        return;
    }

    const uint32_t deltaMs = nowMs - _lastSlewMs;
    _lastSlewMs = nowMs;
    if (deltaMs == 0) return;

    // Kelvin axis (K/min). Phase 2.I.c: when UseDayNightSlew is enabled, pick
    // SlewRateDay or SlewRateNight based on cached day/night flag; else use
    // single SlewRate.
    // Phase 2.K: ParamLMG_CHSlewRate (single) wurde entfernt, nur Day/Night verbleiben.
    // Wenn UseDayNightSlew aus, nutze SlewRateDay als Standardrate.
    const uint16_t slewK = ParamLMG_CHUseDayNightSlew
                            ? (_cachedIsNight ? ParamLMG_CHSlewRateNight
                                              : ParamLMG_CHSlewRateDay)
                            : ParamLMG_CHSlewRateDay;
    if (slewK == 0)
    {
        _appliedKelvin = targetK;
    }
    else if (_appliedKelvin != targetK)
    {
        uint32_t maxStep = (static_cast<uint32_t>(slewK) * deltaMs) / 60000UL;
        if (maxStep == 0) maxStep = 1;
        if (_appliedKelvin < targetK)
        {
            const uint32_t next = static_cast<uint32_t>(_appliedKelvin) + maxStep;
            _appliedKelvin = static_cast<uint16_t>(next < targetK ? next : targetK);
        }
        else
        {
            const uint32_t cur = _appliedKelvin;
            const uint32_t next = (cur > maxStep) ? (cur - maxStep) : 0;
            _appliedKelvin = static_cast<uint16_t>(next > targetK ? next : targetK);
        }
    }

    // Brightness axis (%/min).
    const uint8_t slewB = ParamLMG_CHSlewRateBrightness;
    if (slewB == 0)
    {
        _appliedBrightness = targetB;
    }
    else if (_appliedBrightness != targetB)
    {
        uint32_t maxStep = (static_cast<uint32_t>(slewB) * deltaMs) / 60000UL;
        if (maxStep == 0) maxStep = 1;
        if (_appliedBrightness < targetB)
        {
            const uint32_t next = static_cast<uint32_t>(_appliedBrightness) + maxStep;
            _appliedBrightness = static_cast<uint8_t>(next < targetB ? next : targetB);
        }
        else
        {
            const uint32_t cur = _appliedBrightness;
            const uint32_t next = (cur > maxStep) ? (cur - maxStep) : 0;
            _appliedBrightness = static_cast<uint8_t>(next > targetB ? next : targetB);
        }
    }
}
