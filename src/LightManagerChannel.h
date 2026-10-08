#pragma once

#include "HCL/HCLMasterManager.h"
#include "HCL/HCLProfile.h"
#include "HCL/HCLProfileSelector.h"
#include "HCL/HCLProfileResolver.h"
#include "HCL/HCLSeasonDayNight.h"
#include "OpenKNX.h"
#include <Arduino.h>
#include <time.h>

class LightManagerModule;

class LightManagerChannel : public OpenKNX::Channel
{
public:
    enum class LockFallbackMode : uint8_t
    {
        None    = 0,
        Min1    = 1,
        Min2    = 2,
        Min5    = 3,
        Min10   = 4,
        Min20   = 5,
        Min30   = 6,
        Hour1   = 7,
        Hour2   = 8,
        Hour5   = 9,
        Hour8   = 10,
        Hour12  = 11,
        NextDay = 12
    };

    enum class LockFallbackPolicy : uint8_t
    {
        Legacy         = 0,
        Duration       = 1,
        TimeOfDay      = 2,
        DurationOrTime = 3,
        ExternalOnly   = 4,
        Disabled       = 5
    };

    explicit LightManagerChannel(uint8_t channelIndex);

    /** OpenKNX::Base override (pure virtual). */
    const std::string name() override { return "LightManagerChannel"; }

    /** Master number (1..LMG_ChannelCount) used by HCL::masterManager API. */
    uint8_t masterNumber() const { return _channelIndex + 1; }

    /** Read ETS parameters for this channel and configure the HCL master. */
    void setupHcl(float latitudeDeg, float longitudeDeg, int16_t timezoneOffsetMin);

    /** Per-loop tick: season update + lock fallback evaluation. */
    void loopHcl(const tm* timeinfo, bool hasTime);

    /** Per-loop: push status / detect blocked state change. */
    void pushIfChanged();

    /** Channel-routed KO. Returns true if the KO belongs to this channel. */
    bool processChannelKo(GroupObject& ko, uint16_t channelKoIndex);

    /** Set this channel's lock state (with fallback timer setup). */
    void setLock(bool active, const char* reason);

    /** Phase 3 F2/L2: per-axis lock (UseLock=Getrennt, K09 LockColor / K10 LockBrightness).
     *  No-op wenn UseLock!=2. K02-Vollsperre dominiert. */
    void setLockColor(bool active, const char* reason);
    void setLockBrightness(bool active, const char* reason);

    /** Phase 3 F2: effektive Achsen-Sperre (kombiniert Vollsperre + Achsenlock). */
    bool isColorLocked()      const { return _applyBlocked || _lockColorActive; }
    bool isBrightnessLocked() const { return _applyBlocked || _lockBrightnessActive; }

    /** Persist summer state into bitmask (bit channelIndex) and read back. */
    bool isSummer() const;
    void restoreSummerFromMask(uint16_t mask);

    /** Force re-push of status on next pushIfChanged(). */
    void invalidatePushedState() { _lastBlocked = true; }

    // --- IMasterProvider backing storage (LightManagerModule routes here) ---
    HCL::Master* masterPtr() { return &_master; }
    HCL::InterpolatedValue currentValue() const { return _currentValue; }
    void setCurrentValue(const HCL::InterpolatedValue& v) { _currentValue = v; }
    bool applyBlocked() const { return _applyBlocked; }
    void setApplyBlocked(bool blocked) { _applyBlocked = blocked; }
    bool isAdaptiveActive(uint16_t currentTimeMinutes, uint32_t nowMs) const
    {
        return _master.isAdaptiveCurrentlyActive(currentTimeMinutes, nowMs);
    }

    /** Per-channel timing (ETS parameters, in seconds). */
    uint16_t updateIntervalSec() const { return _updateIntervalSec; }
    uint8_t  fadeDurationSec()   const { return _fadeDurationSec; }

    /** Internal-output mode active (StatusKoEnable = Intern (0) or Both (2)). */
    bool internalOutputEnabled();
    /** 0 = Intern (an Hue), 1 = Extern (KNX-Bus), 2 = Intern+Extern. */
    uint8_t integrationMode();
    /** 0 = Manuell, 1 = Automatisch (Master-Geo). */
    uint8_t astroSource();

    /** Phase 3 F11/L1: validMask fuer interne Sinks (onLightManagerPartial).
     *  Bit 0 = Kelvin valid, Bit 1 = Brightness valid.
     *  Eingeschraenkt durch StatusKoOutput-Caps + per-Axis-Locks. */
    uint8_t internalValidMask() const;

    // --- Phase 2.J.c.2: ProfileV2 inspection (for HueGatewayModule diagnostics) ---
    uint8_t                       resolvedCount()  const { return _resolvedCount; }
    const HCL::ResolvedSetpoint&  resolvedAt(uint8_t i) const { return _resolvedSetpoints[i]; }
    int8_t                        activeProfileIndex() const { return _activeProfileIndex; }
    const char*                   activeProfileName()  const {
        return (_activeProfileIndex >= 0 && _activeProfileIndex < 4)
            ? _profilesV2[_activeProfileIndex].name : "";
    }
    uint16_t                      lastResolvedTimeMinutes() const { return _lastTimeMinutes; }
    bool                          summerActive() const { return _cachedSummerActive; }
    bool                          nightActive()  const { return _cachedIsNight; }

    // Phase 2.K.2: liefert die Helligkeit, die interne Senken (z. B. Hue) lesen
    // sollen — der adaptierte Wert (sofern Adaptive aktiv) oder der reine
    // HCL-Slew-Wert. Routing Bus/Intern regelt Variante E (IntegrationMode).
    uint8_t effectiveBrightness() const;

    // Phase 2.K.3: geslewter Kelvin-Wert, den interne Senken (Hue) zusammen mit
    // effectiveBrightness() lesen sollen — hier gibt es keine Adaptive-Schicht.
    uint16_t appliedKelvin() const { return _appliedKelvin; }

private:
    HCL::Master* master() { return &_master; }
    const HCL::Master* master() const { return &_master; }

    // Setup helpers
    // Phase 2.B: read all 4 profile slots into _profilesV2[] (additive, not yet wired).
    void loadProfilesV2();
    // Phase 2.C: recompute active profile slot from day context (additive).
    void updateActiveProfile(const HCL::DayContext& ctx);
    // Phase 2.D: resolve currently active profile against today's astro times.
    // No-op if _activeProfileIndex < 0. Stores result in _resolvedSetpoints[].
    void resolveActiveProfile(const HCL::AstroTimes& astro);
    // Phase 2.E: evaluate Season + Day/Night sources and cache result flags.
    // Inputs are external (KOs / astro), source enums come from ETS params.
    void evaluateSeasonAndDayNight(HCL::SeasonSource seasonSrc,
                                   const HCL::SeasonInputs& seasonIn,
                                   HCL::DayNightSource dayNightSrc,
                                   const HCL::DayNightInputs& dayNightIn);
    // Phase 2.I: interpolate _currentValue from _resolvedSetpoints[] for the
    // given minute-of-day. No-op if _resolvedCount == 0 (legacy MasterManager
    // value remains active). Linear interpolation between bracketed SPs with
    // wrap-around midnight. Assumes _resolvedSetpoints[] is sorted ascending
    // by timeMinutes (sorted in-place by this method).
    void computeCurrentValueFromResolved(uint16_t minuteOfDay);
    // Phase 2.I.b: rate-limit _currentValue → _appliedKelvin/_appliedBrightness
    // based on per-channel Kelvin (K/min) and Brightness (%/min) slew rates.
    // Snaps on profile switch (5a). Rate=0 disables slew on that axis.
    void applySlew(uint32_t nowMs);
    bool cachedSummerActive() const { return _cachedSummerActive; }
    bool cachedIsNight()      const { return _cachedIsNight; }
    void applyAdvanced(float latitudeDeg, float longitudeDeg, int16_t timezoneOffsetMin);
    void loadAdaptive();
    void loadLockFallbackParams();
    void updateSeason(const tm* timeinfo);
    void evaluateLockFallback(const tm* timeinfo, bool hasTime);
    void publishLockStatus();
    void publishAdaptiveActive();
    uint32_t fallbackDurationMs(LockFallbackMode mode) const;
    bool shouldReleaseByPolicyTime(const tm* timeinfo, bool hasTime) const;
    // Phase 3 F2/L2: gemeinsamer Fallback-Timer fuer K02/K09/K10.
    void armLockFallbackTimer();
    void disarmLockFallbackTimerIfAllReleased();
    void releaseAllAxisLocks(const char* reason);
    bool anyLockActive() const { return _lockActive || _lockColorActive || _lockBrightnessActive; }

    // Phase 3 F1/L7: Axis-Send-Gate (HclAxes + HclTimeWindow).
    // axis: 0=Kelvin, 1=Brightness, 2=Meta (Minutes/Progress/Phase).
    bool _shouldSendAxis(uint8_t axis) const;
    // Phase 4 F3: naechster Setpoint nach minuteOfDay (mit Wrap auf morgen).
    bool _getNextSetpoint(uint16_t nowMin, uint16_t& outK, uint8_t& outB, uint16_t& outMinutesUntil) const;
    // Phase 4 F4: linearer Fortschritt zwischen erstem/letztem SP [0..100%].
    uint8_t _computeDayProgress(uint16_t nowMin) const;
    // Phase 4 F4: Tagesphase 0..5 (Nacht..Abenddaemmerung). Heuristisch via cachedIsNight + minute-of-day;
    // Punkt 5 verfeinert mit Sonnenstand/Civil-Dawn/Dusk.
    uint8_t _computeDayPhase(uint16_t nowMin) const;
    // Phase 3 F1: Preview/Progress/Phase Sends ausfuehren (Delta-gefiltert).
    void _publishPreviewAndProgress();

    // Punkt 6 (F12): aktuellen Setpoint-Index zum Zeitpunkt nowMin im
    // bereits sortierten _resolvedSetpoints[] finden (0xFF wenn leer).
    uint8_t _findCurrentSpIdx(uint16_t nowMin) const;
    // Punkt 6 (F12): externe Override/Fallback mit L6-Konvertierung + Per-SP-Mix.
    uint16_t getEffectiveColorTemp(uint16_t interpolatedSpKelvin,
                                   const HCL::ResolvedSetpoint& currentSp,
                                   uint32_t nowMs) const;
    uint8_t  getEffectiveBrightness(uint8_t interpolatedBrightness,
                                    uint32_t nowMs) const;

    // Lock state
    bool          _lockActive            = false;
    // Phase 3 F2/L2: Achsen-Sperren (nur bei UseLock=Getrennt aktiv).
    bool          _lockColorActive       = false;
    bool          _lockBrightnessActive  = false;
    uint8_t       _lockFallbackMode      = 0;
    uint8_t       _lockFallbackPolicy    = 0;
    uint32_t      _lockFallbackDurationMs= 0;
    uint16_t      _lockFallbackReleaseMinuteOfDay = 0xFFFF;
    unsigned long _lockActivatedMs       = 0;
    unsigned long _lockAutoReleaseMs     = 0;
    int16_t       _lockActivationDayOfYear = -1;
    int16_t       _lockActivationMinuteOfDay = -1;

    // Push state
    HCL::InterpolatedValue _lastPushedValue;
    bool _lastBlocked = false;
    uint32_t _lastPushedMs = 0;

    // Phase 3 F1/F3/F4/L7: Preview/Progress last-pushed Werte für Änderungs-Erkennung.
    int16_t  _lastPushedPreviewMinutes    = -1;   // -1 = nie gepusht
    uint16_t _lastPushedPreviewKelvin     = 0;    // 0 = nie gepusht
    int16_t  _lastPushedPreviewBrightness = -1;   // -1 = nie gepusht
    int16_t  _lastPushedDayProgress       = -1;   // -1 = nie gepusht
    int16_t  _lastPushedDayPhase          = -1;   // -1 = nie gepusht

    // Per-channel timing (ETS configurable)
    uint16_t _updateIntervalSec = 60;
    uint8_t  _fadeDurationSec   = 6;

    // Channel-owned HCL state (replaces former MasterManager-internal arrays).
    HCL::Master            _master;
    HCL::InterpolatedValue _currentValue;
    bool                   _applyBlocked = false;

    // Phase 2.B: per-channel profile storage (4 slots). Populated by loadProfilesV2(),
    // consumed in later sub-steps (selector / resolver). Currently write-only.
    HCL::ProfileV2         _profilesV2[4];

    // Phase 2.C: currently selected profile slot (-1 = none); updated by
    // updateActiveProfile(). Read by future resolver in 2.D.
    int8_t                 _activeProfileIndex = -1;

    // Phase 2.D: resolved setpoints of the active profile for today.
    // Populated by resolveActiveProfile(); count in _resolvedCount.
    HCL::ResolvedSetpoint  _resolvedSetpoints[HCL::ProfileV2::MAX_SETPOINTS];
    uint8_t                _resolvedCount = 0;

    // Phase 2.E: cached Season + Day/Night flags from last evaluation.
    bool                   _cachedSummerActive = false;
    bool                   _cachedIsNight      = false;

    // Punkt 5: DST/Datum-Cache für resolveActiveProfile() Invalidierung.
    // _resolvedSetpointsValidForDate = MM*100+DD; _DstOffset = Minuten.
    // Sentinel 0xFFFF / INT16_MIN = noch nicht aufgelöst.
    uint16_t               _resolvedSetpointsValidForDate      = 0xFFFF;
    int16_t                _resolvedSetpointsValidForDstOffset = INT16_MIN;
    int8_t                 _resolvedSetpointsValidForProfile   = -2; // -2 = uninit

    // Punkt 5: zuletzt berechnete Astro-Times (für _computeDayPhase Astro-Pfad).
    HCL::AstroTimes        _lastAstroTimes{};

    // Phase 2.I: minute-of-day from last loopHcl() tick (0xFFFF = none yet).
    // Used by computeCurrentValueFromResolved() to bracket SPs.
    uint16_t               _lastTimeMinutes    = 0xFFFF;

    // Phase 2.I.b: slew state (target = _currentValue, applied = below).
    // 0xFFFF/0xFF sentinels signal "uninitialized" → first applySlew snaps.
    uint16_t               _appliedKelvin      = 0xFFFF;
    uint8_t                _appliedBrightness  = 0xFF;
    uint32_t               _lastSlewMs         = 0;
    int8_t                 _lastSlewProfileIdx = -2;  // -2 = uninit, -1 = none

    // Phase 2.K.2: zuletzt durch applyAdaptiveBrightness() berechneter Helligkeitswert.
    // 0xFF = noch nicht berechnet / adaptive deaktiviert (Fallback _appliedBrightness).
    uint8_t                _adaptiveBrightness = 0xFF;

    // Punkt 6 (F12): Cache fuer externe Quellen K16-K19 (nach L6-Konvertierung).
    // _extKelvinValue == 0 / _extBrightnessValue == 0xFF = noch nie empfangen.
    uint16_t               _extKelvinValue            = 0;
    uint32_t               _extKelvinLastUpdateMs     = 0;
    uint8_t                _extBrightnessValue        = 0xFF;
    uint32_t               _extBrightnessLastUpdateMs = 0;
};
