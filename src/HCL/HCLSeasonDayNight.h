// SPDX-License-Identifier: GPL-3.0-or-later
// Phase 2.E: Season + Day/Night source evaluators. Pure functions that map
// SeasonSource / DayNightSource enum + per-call inputs to bool flags suitable
// for HCL::DayContext (selector input). No state, no I/O.

#pragma once

#include <cstdint>

#include "HCLTypes.h"

namespace HCL
{

// ---------------------------------------------------------------------------
// Season
// ---------------------------------------------------------------------------

struct SeasonInputs
{
    /// Today as MM*100+DD (e.g. May 21 -> 521). Used by FixedDate.
    int16_t todayMonthDay        = 0;
    /// SummerStart MM*100+DD. Used by FixedDate.
    int16_t summerStartMonthDay  = 0;
    /// SummerEnd MM*100+DD. Used by FixedDate. If start > end, range wraps year.
    int16_t summerEndMonthDay    = 0;
    /// Latest KO value (K04 SummerActive). Used by FromKo.
    bool    koSummerActive       = false;
    /// Solar declination in 0.1°-units (e.g. 235 = +23.5°). >=0 in N hemisphere
    /// summer. Used by Automatic. Sentinel: INT16_MIN if unavailable.
    int16_t solarDeclinationDeci = INT16_MIN;
};

/** MM*100+DD range check including year-wrap (start > end means crosses Jan 1). */
constexpr bool isInRangeMMDD(int16_t today, int16_t start, int16_t end)
{
    if (start <= end) return today >= start && today <= end;
    return today >= start || today <= end;
}

/** Determine summer-active flag from configured source + per-call inputs. */
constexpr bool computeSummerActive(SeasonSource src, const SeasonInputs& in)
{
    switch (src)
    {
        case SeasonSource::Off:       return false;
        case SeasonSource::Automatic: return in.solarDeclinationDeci != INT16_MIN
                                          && in.solarDeclinationDeci >= 0;
        case SeasonSource::FixedDate: return isInRangeMMDD(
                                          in.todayMonthDay,
                                          in.summerStartMonthDay,
                                          in.summerEndMonthDay);
        case SeasonSource::FromKo:    return in.koSummerActive;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Day / Night
// ---------------------------------------------------------------------------

struct DayNightInputs
{
    /// Latest KO value (K06 IsNight). Used by FromKo.
    bool    koIsNight              = false;
    /// Solar altitude above horizon in 0.01°-units (e.g. -83 = -0.83°).
    /// Sentinel: INT16_MIN if unavailable. Used by AstroInternal.
    int16_t solarAltitudeCenti     = INT16_MIN;
};

/**
 * Civil sunset/sunrise threshold per AstroSource doc: sun centre at -0.83°
 * (atmospheric refraction). Stored as centi-degrees: -83.
 */
constexpr int16_t kAstroNightAltitudeCenti = -83;

/** Determine night flag from configured source + per-call inputs. */
constexpr bool computeIsNight(DayNightSource src, const DayNightInputs& in)
{
    switch (src)
    {
        case DayNightSource::Off:           return false;
        case DayNightSource::FromKo:        return in.koIsNight;
        case DayNightSource::AstroInternal: return in.solarAltitudeCenti != INT16_MIN
                                                && in.solarAltitudeCenti <= kAstroNightAltitudeCenti;
    }
    return false;
}

} // namespace HCL
