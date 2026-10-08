// SPDX-License-Identifier: GPL-3.0-or-later
// Phase 2.C: Profile-Selector — selects today's active profile slot from a
// channel's 4-slot ProfileV2 array, based on a DayContext (weekday + season +
// special-day flags). Pure logic, no I/O, no params, no engine wiring.

#pragma once

#include <cstdint>

#include "HCLProfile.h"

namespace HCL
{

/**
 * Context describing the current day. Built externally (typically once per
 * minute) from struct tm + season/holiday flags. The selector ORs all set
 * flags into a bitmask and compares it against each ProfileV2::weekdayMask.
 *
 * Weekday convention: 0=Monday .. 6=Sunday (ISO). Convert from tm_wday
 * (0=Sunday) at the call site.
 */
struct DayContext
{
    uint8_t weekday;     ///< 0=Mon .. 6=Sun; values >=7 contribute no weekday bit.
    bool    isSummer;
    bool    isWinter;
    bool    isVacation;
    bool    isHoliday;
    bool    isFallback;  ///< Set when no time source / fallback profile required.

    /** OR together all set context bits into a weekday-mask-compatible value. */
    constexpr uint16_t toMask() const
    {
        uint16_t m = 0;
        if (weekday < 7) m = static_cast<uint16_t>(1u << weekday);
        if (isVacation)  m |= WeekdayMaskBits::Vacation;
        if (isHoliday)   m |= WeekdayMaskBits::Holiday;
        if (isFallback)  m |= WeekdayMaskBits::Fallback;
        if (isSummer)    m |= WeekdayMaskBits::Summer;
        if (isWinter)    m |= WeekdayMaskBits::Winter;
        return m;
    }
};

/**
 * Pick the active profile for today.
 *
 * Algorithmus (Phase 2 Step 4, Punkt 4):
 *  1) Iteriere nur Profile [0..profileCount-1] (L5). Slots ausserhalb der
 *     ETS-konfigurierten Sichtbarkeit werden ignoriert.
 *  2) Profile mit Bit 9 (Default-Fallback) sind exklusiv (#8) und werden aus
 *     dem regulaeren Match-Pfad ausgeschlossen, separat als Fallback-Liste.
 *  3) Spezifitaet (Plan): Feiertag > Urlaub > Saison+Wochentag > Wochentag.
 *     Bei gleicher Score gewinnt der niedrigere Slot-Index.
 *  4) Wenn kein regulaerer Match: niedrigster Index aus Fallback-Liste.
 *  5) Wenn auch das leer ist: -1.
 *
 * Saison-Bits (10/11) werden vom Caller via DayContext::isSummer/isWinter
 * konditioniert (bei SeasonSource=Off setzt der Caller beide auf false →
 * Saison-Profile matchen dann nicht).
 *
 * @param profiles      Slot-Array mit max. 4 Profilen.
 * @param ctx           Tages-Kontext (Wochentag/Saison/Spezial-Flags).
 * @param profileCount  Anzahl konfigurierter Slots (1..4) aus ETS.
 * @return  Index 0..3 des gewaehlten Profils, oder -1 falls keiner passt.
 */
constexpr int8_t selectProfile(const ProfileV2 (&profiles)[4], const DayContext& ctx,
                               uint8_t profileCount)
{
    if (profileCount == 0) return -1;
    if (profileCount > 4)  profileCount = 4;

    const uint16_t mask = ctx.toMask();
    if (mask == 0) return -1;

    int8_t  bestIdx   = -1;
    uint8_t bestScore = 0;
    int8_t  fallbackIdx = -1;

    for (int8_t i = 0; i < static_cast<int8_t>(profileCount); ++i)
    {
        const ProfileV2& p = profiles[i];
        if (!p.active) continue;
        const uint16_t pm = p.weekdayMask;

        // (2) Default-Fallback exklusiv: andere Bits dont care, separate Liste.
        if (pm & WeekdayMaskBits::Fallback)
        {
            if (fallbackIdx < 0) fallbackIdx = i;
            continue;
        }

        // Profile passt grundsaetzlich nur, wenn mindestens ein Bit mit ctx ueberlappt.
        if ((pm & mask) == 0) continue;

        // (3) Spezifitaets-Score: Feiertag(4) > Urlaub(3) > Saison+WT(2) > WT(1).
        uint8_t score = 0;
        if ((pm & WeekdayMaskBits::Holiday) && ctx.isHoliday)
        {
            score = 4;
        }
        else if ((pm & WeekdayMaskBits::Vacation) && ctx.isVacation)
        {
            score = 3;
        }
        else
        {
            const uint16_t weekdayBit = (ctx.weekday < 7) ? static_cast<uint16_t>(1u << ctx.weekday) : 0;
            const bool weekdayMatch = (weekdayBit != 0) && ((pm & weekdayBit) != 0);
            const bool seasonMatch  =
                ((pm & WeekdayMaskBits::Summer) && ctx.isSummer)
             || ((pm & WeekdayMaskBits::Winter) && ctx.isWinter);
            if (weekdayMatch && seasonMatch) score = 2;
            else if (weekdayMatch)           score = 1;
            else                              continue; // nur Saison ohne WT → kein Match
        }

        if (score > bestScore)
        {
            bestScore = score;
            bestIdx   = i;
        }
        // Bei gleicher Score: niedrigerer Index gewinnt → schon gesetzt, ignorieren.
    }

    if (bestIdx >= 0)   return bestIdx;
    return fallbackIdx; // -1 falls auch leer
}

// 0.2.0-Kompatibilitaets-Overload (nutzt alle 4 Slots ohne profileCount-Gate).
constexpr int8_t selectProfile(const ProfileV2 (&profiles)[4], const DayContext& ctx)
{
    return selectProfile(profiles, ctx, 4);
}

} // namespace HCL
