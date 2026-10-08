#pragma once

#include <Arduino.h>

namespace HCL {

// -----------------------------------------------------------------------------
// Sentinels (F11) — uint16 0 / uint8 0xFF markieren "Achse nicht valide"
// im Per-Kanal Cache _lastKelvin/_lastBrightness sowie in onLightManagerPartial.
// 0 K liegt außerhalb des sinnvollen Range 1500..10000 K → kollisionsfrei.
// 0xFF % liegt außerhalb 0..100 % → kollisionsfrei.
// -----------------------------------------------------------------------------
constexpr uint16_t kInvalidKelvin = 0;
constexpr uint8_t kInvalidBrightness = 0xFF;

// validMask Bits für ILightManagerOutput::onLightManagerPartial()
constexpr uint8_t kValidBitKelvin = 0x01;     // Bit 0
constexpr uint8_t kValidBitBrightness = 0x02; // Bit 1
// Bits 2..7 reserved (0)

// -----------------------------------------------------------------------------
// F1 — HCL-Achsen-Auswahl: welche Achsen berechnet/gesendet werden
// -----------------------------------------------------------------------------
enum class HclAxes : uint8_t {
    KelvinAndBrightness = 0, // KH (Default)
    KelvinOnly = 1,          // Nur Farbtemperatur
    BrightnessOnly = 2,      // Nur Helligkeit
    Off = 3                  // Aus (Kanal HCL-frei)
};

// -----------------------------------------------------------------------------
// F1 — HCL-Zeitfenster: wann gesendet wird (DayNight-gated)
// -----------------------------------------------------------------------------
enum class HclTimeWindow : uint8_t {
    Always = 0,    // 24/7
    DayOnly = 1,   // Nur tagsüber
    NightOnly = 2  // Nur nachts
};

// -----------------------------------------------------------------------------
// L1 — Variante-E Output-Dispatch
// -----------------------------------------------------------------------------
enum class IntegrationMode : uint8_t {
    Internal = 0,            // Nur interne Sinks (z.B. Hue), kein Bus
    External = 1,            // Nur Bus, keine internen Sinks
    InternalAndExternal = 2  // Beide aktiv (Default)
};

enum class StatusKoOutput : uint8_t {
    BrightnessAndKelvin = 0, // K00 + K01 (Default)
    BrightnessOnly = 1,      // Nur K00
    KelvinOnly = 2,          // Nur K01
    CombinedOnly = 3,        // Nur K08 (DPT-249-600)
    All = 4                  // K00 + K01 + K08
};

struct OutputCaps {
    bool brightness;
    bool kelvin;
    bool combined;
};

static constexpr OutputCaps capsFor(StatusKoOutput o) {
    switch (o) {
        case StatusKoOutput::BrightnessAndKelvin: return {true, true, false};
        case StatusKoOutput::BrightnessOnly:      return {true, false, false};
        case StatusKoOutput::KelvinOnly:          return {false, true, false};
        case StatusKoOutput::CombinedOnly:        return {false, false, true};
        case StatusKoOutput::All:                 return {true, true, true};
    }
    return {false, false, false};
}

// -----------------------------------------------------------------------------
// F8 — Stützpunkt-Anker (Resolver-Quelle für die SP-Zeit)
// -----------------------------------------------------------------------------
enum class AnchorType : uint8_t {
    FixedTime = 0,        // Hour:Minute direkt
    SunriseRel = 1,       // Sunrise + Offset
    SunsetRel = 2,        // Sunset + Offset
    CivilDawnRel = 3,     // Bürgerliche Morgendämmerung + Offset
    CivilDuskRel = 4,     // Bürgerliche Abenddämmerung + Offset
    SolarNoonRel = 5      // Sonnenhöchststand + Offset
};

// F8 — Clamp-Modus (Sicherheitsgrenze bei astro-relativen Ankern)
enum class ClampMode : uint8_t {
    Disabled = 0,    // Keine Grenze
    NotBefore = 1,   // ClampHour:ClampMin ist Mindestzeit
    NotAfter = 2     // ClampHour:ClampMin ist Maximalzeit
};

// Per-SP externe Farbtemp-Bedingung
enum class ExtColorTempMode : uint8_t {
    Off = 0,         // Extern ignoriert, SP-Kelvin gilt
    Always = 1,      // Immer extern (sofern frisch)
    OnlyGreater = 2, // Nur wenn extern > SP-Kelvin
    OnlySmaller = 3  // Nur wenn extern < SP-Kelvin
};

// -----------------------------------------------------------------------------
// F7-Hybrid — Saison-Quelle
// -----------------------------------------------------------------------------
enum class SeasonSource : uint8_t {
    Off = 0,          // Saison-neutral
    Automatic = 1,    // Astro (Deklination + Offset)
    FixedDate = 2,    // SummerStart..SummerEnd
    FromKo = 3        // K04 SummerActive
};

enum class SummerActiveInit : uint8_t {
    Nothing = 0,      // Undefiniert (Profile mit Saison-Bits matchen nicht)
    ReadFromBus = 1,  // Read-Request beim Boot
    Winter = 2,       // false
    Summer = 3        // true
};

// -----------------------------------------------------------------------------
// F12 — Astro-Quelle Per-Kanal (Master-Geo vs. manuelle Sunrise/Sunset)
// -----------------------------------------------------------------------------
enum class AstroSource : uint8_t {
    Automatic = 0,  // Master-Geo (NOAA)
    Manual = 1      // Per-Kanal Sunrise/Sunset + 30-min-Heuristik
};

// F12 — Tag/Nacht-Quelle für Slew-Rate + HclTimeWindow
enum class DayNightSource : uint8_t {
    Off = 0,         // Keine Quelle (HclTimeWindow=Immer, SlewRate-Fallback=Day)
    FromKo = 1,      // K06 IsNight
    AstroInternal = 2 // Sonne ≤ −0.83° = Nacht (Per-Kanal-Berechnung)
};

// -----------------------------------------------------------------------------
// F12 — Externe Quellen (Override/Fallback) für Kelvin/Brightness
// -----------------------------------------------------------------------------
enum class ExtColorTempSource : uint8_t {
    Off = 0,
    Override = 1,
    Fallback = 2
};

enum class ExtBrightnessSource : uint8_t {
    Off = 0,
    Override = 1,
    Fallback = 2
};

enum class ExtColorTempDpt : uint8_t {
    Kelvin = 0,  // K18 DPT-7-600
    Scalar = 1   // K19 DPT-5-6 + L6 Konvertierung
};

enum class ExtBrightnessDpt : uint8_t {
    Percent = 0, // K16 DPT-5-1
    Lux = 1      // K17 DPT-9-4 + L6 Konvertierung
};

// -----------------------------------------------------------------------------
// L2 — Per-Kanal Sperr-Modus
// -----------------------------------------------------------------------------
enum class UseLock : uint8_t {
    None = 0,        // Kein Lock (KOs unsichtbar)
    FullLock = 1,    // Nur K02/K03 (Default)
    Separate = 2     // K02/K03 + K09/K10 (Farbe/Helligkeit getrennt)
};

// L2 — Rückfall-Policy (aus 0.2.0 unverändert)
enum class FallbackPolicy : uint8_t {
    DefinedFallback = 0,   // Enum LockFallback (Sektion 3)
    FreeDuration = 1,      // FallbackDurationSec
    FreeTime = 2,          // FallbackReleaseTime
    DurationOrTime = 3,    // was zuerst eintritt
    OnlyExternal = 4,      // Nur durch K02/K09/K10=0
    Deactivated = 5        // Kein Auto-Release
};

// -----------------------------------------------------------------------------
// #14 — Adaptive Tag/Nacht Polarität
// -----------------------------------------------------------------------------
enum class AdaptiveDayNightPolarity : uint8_t {
    BrighterAtNight = 0, // Default (0.2.0-Hardcoded-Verhalten)
    DarkerAtNight = 1
};

} // namespace HCL
