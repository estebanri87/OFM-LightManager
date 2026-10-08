// SPDX-License-Identifier: GPL-3.0-or-later
// Phase 2.D: Profile-Resolver — converts an active ProfileV2 (with anchor /
// offset / clamp metadata) into a flat list of ResolvedSetpoint entries with
// concrete minute-of-day values. Pure logic, no I/O. Sorting is left to the
// caller (interpolator step in 2.E/2.F).

#pragma once

#include <cstdint>

#include "HCLProfile.h"
#include "HCLTypes.h"

namespace HCL
{

/**
 * Astro time references for the current day, in minutes of day (0..1439).
 * Use -1 to mark "unavailable today" (polar day/night). SPs whose anchor
 * resolves to -1 are dropped from the output.
 */
struct AstroTimes
{
    int16_t sunrise   = -1;
    int16_t sunset    = -1;
    int16_t civilDawn = -1;
    int16_t civilDusk = -1;
    int16_t solarNoon = -1;
};

/** Return base minute-of-day for the given anchor, or -1 if unavailable. */
inline int16_t anchorBaseMinutes(AnchorType a, uint8_t hour, uint8_t minute, const AstroTimes& at)
{
    switch (a)
    {
        case AnchorType::FixedTime:    return static_cast<int16_t>(hour) * 60 + minute;
        case AnchorType::SunriseRel:   return at.sunrise;
        case AnchorType::SunsetRel:    return at.sunset;
        case AnchorType::CivilDawnRel: return at.civilDawn;
        case AnchorType::CivilDuskRel: return at.civilDusk;
        case AnchorType::SolarNoonRel: return at.solarNoon;
    }
    return -1;
}

/** Wrap a (possibly negative or >1440) minute value into 0..1439. */
inline uint16_t wrapMinutes(int32_t m)
{
    m %= 1440;
    if (m < 0) m += 1440;
    return static_cast<uint16_t>(m);
}

/**
 * Resolve all active setpoints of `profile` against today's `astro` times.
 *
 * - FixedTime SPs use Hour:Minute directly; offsetMinutes is ignored.
 * - Astro-anchored SPs add offsetMinutes, then apply clamp (NotBefore / NotAfter).
 * - SPs whose anchor is unavailable (-1) are silently dropped.
 * - Output is in declaration order (NOT sorted by time).
 *
 * @return Number of entries written to `out` (0..ProfileV2::MAX_SETPOINTS).
 */
inline uint8_t resolveProfile(const ProfileV2& profile,
                              const AstroTimes& astro,
                              ResolvedSetpoint out[ProfileV2::MAX_SETPOINTS])
{
    uint8_t n = 0;
    const uint8_t count = (profile.spCount > ProfileV2::MAX_SETPOINTS)
                              ? ProfileV2::MAX_SETPOINTS
                              : profile.spCount;
    for (uint8_t i = 0; i < count; ++i)
    {
        const SetpointV2& sp = profile.sps[i];

        const int16_t base = anchorBaseMinutes(sp.anchorType, sp.hour, sp.minute, astro);
        if (base < 0) continue;

        int32_t t = base;
        if (sp.anchorType != AnchorType::FixedTime)
            t += sp.offsetMinutes;

        const int16_t clampLimit =
            static_cast<int16_t>(sp.clampHour) * 60 + sp.clampMinute;
        switch (sp.clampMode)
        {
            case ClampMode::NotBefore: if (t < clampLimit) t = clampLimit; break;
            case ClampMode::NotAfter:  if (t > clampLimit) t = clampLimit; break;
            case ClampMode::Disabled:  break;
        }

        ResolvedSetpoint& r = out[n++];
        r.timeMinutes      = wrapMinutes(t);
        r.kelvin           = sp.kelvin;
        r.brightness       = sp.brightness;
        r.extColorTempMode = sp.extColorTempMode;
        r.extMixPercent    = sp.extMixPercent;
    }
    return n;
}

} // namespace HCL
