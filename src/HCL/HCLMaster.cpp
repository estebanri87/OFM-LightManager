#include "HCLMaster.h"
#include <algorithm>
#include <math.h>

namespace HCL {

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDeg2Rad = kPi / 180.0f;
constexpr float kRad2Deg = 180.0f / kPi;
} // namespace

// F6 Astro-Engine (Punkt 5).
// Quelle: NOAA Solar Position (Spencer 1971 Approximation) + Bennett-Refraktion.
void Master::computeSolarPosition(uint16_t dayOfYear,
                                  uint16_t timeMinutes,
                                  float latitudeDeg,
                                  float longitudeDeg,
                                  int16_t timezoneOffsetMin,
                                  int16_t& outDeclinationDeci,
                                  int16_t& outAltitudeCenti)
{
    if (dayOfYear == 0) dayOfYear = 1;
    if (dayOfYear > 366) dayOfYear = 366;
    if (timeMinutes > 1439) timeMinutes = 1439;

    // Fractional year [rad] gemäß Spencer 1971.
    const float hourFrac = static_cast<float>(timeMinutes) / 60.0f;
    const float gamma = (2.0f * kPi / 365.0f) *
                        (static_cast<float>(dayOfYear - 1) + (hourFrac - 12.0f) / 24.0f);

    // Equation of Time [min]
    const float eqTime = 229.18f * (0.000075f
        + 0.001868f * cosf(gamma)
        - 0.032077f * sinf(gamma)
        - 0.014615f * cosf(2.0f * gamma)
        - 0.040849f * sinf(2.0f * gamma));

    // Sonnen-Deklination [rad]
    const float declRad = 0.006918f
        - 0.399912f * cosf(gamma)
        + 0.070257f * sinf(gamma)
        - 0.006758f * cosf(2.0f * gamma)
        + 0.000907f * sinf(2.0f * gamma)
        - 0.002697f * cosf(3.0f * gamma)
        + 0.001480f * sinf(3.0f * gamma);
    const float declDeg = declRad * kRad2Deg;

    // Wahre Sonnenzeit [min] = lokale Zeit + 4*Länge - tzOffset + EqT
    float trueSolarMin = static_cast<float>(timeMinutes)
                       + 4.0f * longitudeDeg
                       - static_cast<float>(timezoneOffsetMin)
                       + eqTime;
    // Normalisieren auf [0..1440)
    while (trueSolarMin < 0.0f)     trueSolarMin += 1440.0f;
    while (trueSolarMin >= 1440.0f) trueSolarMin -= 1440.0f;

    // Stunden-Winkel [deg]: 0 = solar noon, negativ = vor noon.
    const float hourAngleDeg = (trueSolarMin / 4.0f) - 180.0f;
    const float hourAngleRad = hourAngleDeg * kDeg2Rad;
    const float latRad = latitudeDeg * kDeg2Rad;

    // Sonnen-Höhe
    float sinAlt = sinf(latRad) * sinf(declRad)
                 + cosf(latRad) * cosf(declRad) * cosf(hourAngleRad);
    if (sinAlt > 1.0f) sinAlt = 1.0f;
    if (sinAlt < -1.0f) sinAlt = -1.0f;
    float altDeg = asinf(sinAlt) * kRad2Deg;

    // Bennett-Refraktion (vereinfacht) nur bei niedriger Sonne (Plan #5).
    if (altDeg < 5.0f && altDeg > -2.0f)
    {
        const float arg = altDeg + 7.31f / (altDeg + 4.4f);
        const float refractionDeg = 0.0167f / tanf(arg * kDeg2Rad);
        altDeg += refractionDeg;
    }

    // Decl: deci-degree; Altitude: centi-degree, mit Clamping auf int16-Range.
    long declDeci = lroundf(declDeg * 10.0f);
    if (declDeci > 235)  declDeci = 235;
    if (declDeci < -235) declDeci = -235;
    outDeclinationDeci = static_cast<int16_t>(declDeci);

    long altCenti = lroundf(altDeg * 100.0f);
    if (altCenti > 9000)  altCenti = 9000;
    if (altCenti < -9000) altCenti = -9000;
    outAltitudeCenti = static_cast<int16_t>(altCenti);
}

Master::Master() {
    _isSummer = false;
    _sunriseMinutes = 390; // 06:30
    _sunsetMinutes = 1170; // 19:30
    _sunTimesValid = false;
    _latitudeDeg = 50.0f;
    _longitudeDeg = 8.0f;
    _timezoneOffsetMin = 60;
}

void Master::setSunTimes(uint16_t sunriseMinutes, uint16_t sunsetMinutes) {
    if (sunriseMinutes >= 1440 || sunsetMinutes >= 1440) {
        return;
    }

    _sunriseMinutes = sunriseMinutes;
    _sunsetMinutes = sunsetMinutes;
    _sunTimesValid = true;
}

void Master::setAmbientLux(float lux) {
    if (lux < 0.0f) lux = 0.0f;
    _luxFilterBuffer[_luxFilterIndex] = lux;
    _luxFilterIndex = (_luxFilterIndex + 1) % 3;
    _ambientLux = lux;
    _lastLuxReceiveMs = millis();
}

bool Master::isSensorValid(uint32_t nowMs) const {
    if (_adaptiveConfig.sensorTimeoutMinutes == 0) return true;
    if (_lastLuxReceiveMs == 0) return false;
    uint32_t timeoutMs = static_cast<uint32_t>(_adaptiveConfig.sensorTimeoutMinutes) * 60000UL;
    return (nowMs - _lastLuxReceiveMs) < timeoutMs;
}

float Master::getFilteredLux() const {
    float sum = 0.0f;
    for (uint8_t i = 0; i < 3; i++) {
        sum += _luxFilterBuffer[i];
    }
    return sum / 3.0f;
}

bool Master::isAdaptiveCurrentlyActive(uint16_t currentTimeMinutes, uint32_t nowMs) const {
    if (_adaptiveConfig.mode == AdaptiveMode::Disabled) return false;
    if (!isSensorValid(nowMs)) return false;

    switch (_adaptiveConfig.activeMode) {
        case AdaptiveActiveMode::Always:
            return true;
        case AdaptiveActiveMode::DayOnly: {
            bool isDay = _adaptiveConfig.dayNightPolarity ? !_isDaytime : _isDaytime;
            return isDay;
        }
        case AdaptiveActiveMode::TimeRange: {
            uint16_t s = _adaptiveConfig.activeStartMinutes;
            uint16_t e = _adaptiveConfig.activeEndMinutes;
            if (s <= e) {
                return currentTimeMinutes >= s && currentTimeMinutes < e;
            } else {
                // Mitternachts-Wrap
                return currentTimeMinutes >= s || currentTimeMinutes < e;
            }
        }
        default:
            return false;
    }
}

InterpolatedValue Master::applyAdaptiveBrightness(InterpolatedValue val, uint16_t currentTimeMinutes, uint32_t nowMs) {
    if (!isAdaptiveCurrentlyActive(currentTimeMinutes, nowMs)) {
        _lastSentBrightness = val.brightness;
        return val;
    }

    float filteredLux = getFilteredLux();
    uint8_t output = val.brightness;

    if (_adaptiveConfig.mode == AdaptiveMode::OpenLoop) {
        float maxLux = static_cast<float>(_adaptiveConfig.maxLux);
        if (maxLux < 1.0f) maxLux = 1.0f;
        float ambientNorm = filteredLux / maxLux;
        if (ambientNorm > 1.0f) ambientNorm = 1.0f;
        float raw = static_cast<float>(val.brightness) * (1.0f - ambientNorm * (_adaptiveConfig.strength / 100.0f));
        output = static_cast<uint8_t>(constrain(static_cast<int>(raw + 0.5f), _adaptiveConfig.minBrightness, 100));

    } else if (_adaptiveConfig.mode == AdaptiveMode::ClosedLoop) {
        float maxLux = static_cast<float>(_adaptiveConfig.maxLux);
        if (maxLux < 1.0f) maxLux = 1.0f;
        float targetLux = (static_cast<float>(val.brightness) / 100.0f) * maxLux;

        float error = filteredLux - targetLux;
        if (error < 0.0f) error = -error;

        if (error < static_cast<float>(_adaptiveConfig.deadbandLux)) {
            // Totband: kein Update
            if (_lastSentBrightness == 255) {
                _lastSentBrightness = val.brightness;
            }
            return {val.kelvin, _lastSentBrightness};
        }

        float errorNorm = (targetLux - filteredLux) / maxLux;
        float raw = static_cast<float>(val.brightness) + _adaptiveConfig.kp * errorNorm * 100.0f;
        uint8_t ceiling = _adaptiveConfig.ceilToHCL ? val.brightness : 100;
        output = static_cast<uint8_t>(constrain(static_cast<int>(raw + 0.5f), _adaptiveConfig.minBrightness, ceiling));
    }

    // Mindestschrittgröße
    if (_lastSentBrightness != 255) {
        uint8_t diff = (output > _lastSentBrightness) ? (output - _lastSentBrightness) : (_lastSentBrightness - output);
        if (diff < _adaptiveConfig.minChangePercent) {
            return {val.kelvin, _lastSentBrightness};
        }
    }

    _lastSentBrightness = output;
    return {val.kelvin, output};
}

} // namespace HCL
