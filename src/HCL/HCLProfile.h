#pragma once

#include "HCLTypes.h"
#include <Arduino.h>

namespace HCL {

// =============================================================================
// Stützpunkt v2 (Phase 2)
//
// In-Memory-Repräsentation eines konfigurierten Stützpunkts. Die Werte werden
// von LightManagerChannel::loadParameters() aus dem ETS-Memory (Profile-Block)
// gelesen — siehe Phase 2 Step 2.B.
//
// Verhältnis zum alten HCL::Setpoint (HCLSetpoint.h):
//   - Alter Setpoint hat absolutes timeMinutes (0..1439) — wird nach Anchor-
//     Resolver erst zu ResolvedSetpoint (siehe unten).
//   - Neuer SetpointV2 trägt die ETS-Konfiguration inkl. Anker, Clamp und
//     Per-SP Externe-Farbtemp-Bedingung.
//
// Speicherlayout (Soll-Größe: 14 B, dicht gepackt, ohne Padding-Garantie):
//   active            1 B   Aktiv-Flag (true = SP wirkt; false = übersprungen)
//   anchorType        1 B   HCL::AnchorType (FixedTime / Sunrise±/…)
//   hour              1 B   0..23   (gültig nur bei FixedTime)
//   minute            1 B   0..59   (gültig nur bei FixedTime)
//   offsetMinutes     2 B   int16 −180..+180 (gültig bei anchorType ≠ FixedTime)
//   clampMode         1 B   HCL::ClampMode (Disabled/NotBefore/NotAfter)
//   clampHour         1 B   0..23   (gültig bei clampMode ≠ Disabled)
//   clampMinute       1 B   0..59   (gültig bei clampMode ≠ Disabled)
//   kelvin            2 B   1500..10000 K
//   brightness        1 B   0..100 %
//   extColorTempMode  1 B   HCL::ExtColorTempMode (Off/Always/Greater/Smaller)
//   extMixPercent     1 B   0..100 % (gültig wenn extColorTempMode ≠ Off)
//
// Beim ETS-Speichermapping wird das gleiche Layout 1:1 in den Profile-Block
// gelegt (Param-IDs siehe templ.xml SP{n}_*).
// =============================================================================
struct SetpointV2 {
    bool             active;
    AnchorType       anchorType;
    uint8_t          hour;
    uint8_t          minute;
    int16_t          offsetMinutes;
    ClampMode        clampMode;
    uint8_t          clampHour;
    uint8_t          clampMinute;
    uint16_t         kelvin;
    uint8_t          brightness;
    ExtColorTempMode extColorTempMode;
    uint8_t          extMixPercent;

    SetpointV2()
        : active(false),
          anchorType(AnchorType::FixedTime),
          hour(0), minute(0),
          offsetMinutes(0),
          clampMode(ClampMode::Disabled),
          clampHour(0), clampMinute(0),
          kelvin(4000),
          brightness(80),
          extColorTempMode(ExtColorTempMode::Off),
          extMixPercent(0) {}
};

// =============================================================================
// Profil v2 (Phase 2)
//
// Container für einen der bis zu 4 Profile pro Kanal.
// "Gilt für"-Filter (weekdayMask) Bits:
//   Bit 0..6  : Mo..So
//   Bit 7     : Urlaub (KoLOG_Vacation)
//   Bit 8     : Feiertag (Timer::holidayToday())
//   Bit 9     : Default-Fallback (exklusiv, #8) — andere Bits don't care
//   Bit 10    : Sommer (SeasonSource ≠ Off und _isSummerActive = true)
//   Bit 11    : Winter (SeasonSource ≠ Off und _isSummerActive = false)
//   Bit 12..15: reserved (0)
//
// Profil mit Bit 9 wird in Selektor in separate Fallback-Liste verschoben
// (siehe Phase 2 Step 2.C selectActiveProfile).
// =============================================================================
struct ProfileV2 {
    static constexpr uint8_t MAX_SETPOINTS = 10;
    static constexpr uint8_t NAME_LEN = 13; // entsprechend ETS-Param P{N}_Name

    bool        active;                  // Profil-Aktiv-Flag
    char        name[NAME_LEN + 1];      // 0-terminierter Anzeigename
    uint16_t    weekdayMask;             // Siehe Bit-Belegung oben
    uint8_t     spCount;                 // 0..10 — Anzahl gültiger SPs
    SetpointV2  sps[MAX_SETPOINTS];

    ProfileV2()
        : active(false),
          weekdayMask(0),
          spCount(0) {
        name[0] = '\0';
    }
};

// Weekday-Mask Bit-Helfer
namespace WeekdayMaskBits {
    constexpr uint16_t Monday     = 1u << 0;
    constexpr uint16_t Tuesday    = 1u << 1;
    constexpr uint16_t Wednesday  = 1u << 2;
    constexpr uint16_t Thursday   = 1u << 3;
    constexpr uint16_t Friday     = 1u << 4;
    constexpr uint16_t Saturday   = 1u << 5;
    constexpr uint16_t Sunday     = 1u << 6;
    constexpr uint16_t Vacation   = 1u << 7;
    constexpr uint16_t Holiday    = 1u << 8;
    constexpr uint16_t Fallback   = 1u << 9;
    constexpr uint16_t Summer     = 1u << 10;
    constexpr uint16_t Winter     = 1u << 11;
}

// =============================================================================
// Resolved Setpoint (Output von resolveAnchors() — Phase 2 Step 2.E)
//
// Das Ergebnis des Anchor-Resolvers: pro SP wird die abstrakte Anker-Definition
// (z. B. "Sunrise + 15 min, nicht vor 06:00") zu einer konkreten Tageszeit
// (timeMinutes 0..1439) aufgelöst. ExtColorTempMode/Mix bleiben durchgereicht,
// damit der Output-Pfad sie für getEffectiveColorTemp() nutzen kann.
//
// Cache-Schlüssel: Datum (MM-DD) + DST-Offset → siehe HCLMaster::_resolvedSetpoints.
// =============================================================================
struct ResolvedSetpoint {
    uint16_t         timeMinutes;        // 0..1439, sortiert
    uint16_t         kelvin;              // 1500..10000 K
    uint8_t          brightness;          // 0..100 %
    ExtColorTempMode extColorTempMode;
    uint8_t          extMixPercent;

    ResolvedSetpoint()
        : timeMinutes(0), kelvin(4000), brightness(80),
          extColorTempMode(ExtColorTempMode::Off), extMixPercent(0) {}
};

} // namespace HCL
