#pragma once

#include "HCLSetpoint.h" // InterpolatedValue
#include "HCLTypes.h"    // Phase 2 enums/sentinels
#include "HCLProfile.h"  // SetpointV2 / ProfileV2 / ResolvedSetpoint
#include <Arduino.h>

namespace HCL {

enum class AdaptiveMode : uint8_t {
    Disabled = 0,
    OpenLoop = 1,   // Proportionale Kompensation: helle Umgebung → weniger Licht
    ClosedLoop = 2  // P-Regler: Umgebungshelligkeit auf Sollwert regeln
};

enum class AdaptiveActiveMode : uint8_t {
    Always = 0,    // Immer aktiv
    DayOnly = 1,   // Nur wenn Tag/Nacht-KO = Tag
    TimeRange = 2  // Nur innerhalb definierter Uhrzeit
};

struct AdaptiveConfig {
    AdaptiveMode mode = AdaptiveMode::Disabled;
    AdaptiveActiveMode activeMode = AdaptiveActiveMode::Always;
    uint8_t strength = 80;             // OpenLoop: Kompensationsstärke 0-100%
    uint16_t maxLux = 1000;            // Skalierungsmaximum in Lux
    uint8_t minBrightness = 5;         // Mindestausgabe 0-100%
    float kp = 1.0f;                   // ClosedLoop: Proportionalfaktor
    uint16_t activeStartMinutes = 360; // TimeRange: Start (Minuten seit Mitternacht)
    uint16_t activeEndMinutes = 1320;  // TimeRange: Ende (Minuten seit Mitternacht)
    bool ceilToHCL = true;             // Ausgabe auf HCL-Wert gedeckelt
    bool dayNightPolarity = false;     // false: KO-Wert 1=Tag; true: KO-Wert 1=Nacht
    uint16_t deadbandLux = 50;         // ClosedLoop: Totband in Lux
    uint8_t sensorTimeoutMinutes = 5;  // Failsafe: Rückfall nach X Minuten (0=deaktiviert)
    uint8_t minChangePercent = 2;      // Mindestschrittgröße in %
};

/**
 * @brief Per-channel HCL Master.
 *
 * Phase 2.J: legacy setpoint storage / curve calculators / master-level slew
 * retired. The remaining responsibilities are:
 *   - geo + sunrise/sunset hints for the ProfileV2 resolver
 *   - season flag (winter/summer)
 *   - adaptive brightness state (lux filter + config) — currently dormant,
 *     pending re-integration into the ProfileV2 pipeline (Phase 2.K).
 */
class Master {
public:
    Master();

    // --- Geo / time ---
    void setLocation(float latitudeDeg, float longitudeDeg) {
        _latitudeDeg = constrain(latitudeDeg, -90.0f, 90.0f);
        _longitudeDeg = constrain(longitudeDeg, -180.0f, 180.0f);
    }
    void setTimezoneOffsetMinutes(int16_t timezoneOffsetMin) {
        _timezoneOffsetMin = constrain(timezoneOffsetMin, static_cast<int16_t>(-720), static_cast<int16_t>(840));
    }

    // --- Sunrise / sunset hints (used by ProfileV2 anchors) ---
    void setSunTimes(uint16_t sunriseMinutes, uint16_t sunsetMinutes);
    void clearSunTimes() { _sunTimesValid = false; }
    bool hasSunTimes() const { return _sunTimesValid; }
    uint16_t getSunriseMinutes() const { return _sunriseMinutes; }
    uint16_t getSunsetMinutes() const { return _sunsetMinutes; }

    // --- F6 Astro-Engine (Punkt 5) ---
    // Berechnet Sonnen-Deklination (0.1°-Einheiten) und Sonnen-Höhe
    // (0.01°-Einheiten) für (dayOfYear, timeMinutes UTC-lokal) anhand Geo.
    // Algorithmus: NOAA/Spencer 1971-Approximation. Refraktions-Korrektur
    // (Bennett) nur wenn altDeg < 5° (Plan F6 #5).
    // dayOfYear: 1..366. timeMinutes: 0..1439 (lokale Tageszeit inkl. DST).
    static void computeSolarPosition(uint16_t dayOfYear,
                                     uint16_t timeMinutes,
                                     float latitudeDeg,
                                     float longitudeDeg,
                                     int16_t timezoneOffsetMin,
                                     int16_t& outDeclinationDeci,
                                     int16_t& outAltitudeCenti);

    // Convenience: nutzt Master-Geo + setTimezoneOffsetMinutes.
    void computeSolarPosition(uint16_t dayOfYear, uint16_t timeMinutes,
                              int16_t& outDeclinationDeci,
                              int16_t& outAltitudeCenti) const
    {
        computeSolarPosition(dayOfYear, timeMinutes,
                             _latitudeDeg, _longitudeDeg, _timezoneOffsetMin,
                             outDeclinationDeci, outAltitudeCenti);
    }

    float getLatitudeDeg() const { return _latitudeDeg; }
    float getLongitudeDeg() const { return _longitudeDeg; }
    int16_t getTimezoneOffsetMin() const { return _timezoneOffsetMin; }

    // --- Season flag ---
    void setIsSummer(bool isSummer) { _isSummer = isSummer; }
    bool isSummer() const { return _isSummer; }

    // --- Adaptive Helligkeit (Phase 2.K wiring pending) ---
    void setAdaptiveConfig(const AdaptiveConfig& config) { _adaptiveConfig = config; }
    const AdaptiveConfig& getAdaptiveConfig() const { return _adaptiveConfig; }
    void setAmbientLux(float lux);
    void setDaytime(bool isDaytime) { _isDaytime = isDaytime; }
    bool isAdaptiveCurrentlyActive(uint16_t currentTimeMinutes, uint32_t nowMs) const;
    // Phase 2.K.2: öffentlicher Wrapper für applyAdaptiveBrightness, damit
    // LightManagerChannel::pushIfChanged() die Adaptive-Schicht aufrufen kann.
    InterpolatedValue applyAdaptiveBrightness(InterpolatedValue val, uint16_t currentTimeMinutes, uint32_t nowMs);

private:
    bool _isSummer;
    uint16_t _sunriseMinutes;
    uint16_t _sunsetMinutes;
    bool _sunTimesValid;
    float _latitudeDeg;
    float _longitudeDeg;
    int16_t _timezoneOffsetMin;

    // --- Adaptive Helligkeit ---
    AdaptiveConfig _adaptiveConfig;
    float _ambientLux = 0.0f;
    bool _isDaytime = true;
    uint32_t _lastLuxReceiveMs = 0;
    uint8_t _lastSentBrightness = 255; // 255 = noch kein Wert gesendet
    float _luxFilterBuffer[3] = {0.0f, 0.0f, 0.0f};
    uint8_t _luxFilterIndex = 0;

    // --- Adaptive Helligkeit (privat) ---
    bool isSensorValid(uint32_t nowMs) const;
    float getFilteredLux() const;
};

} // namespace HCL
