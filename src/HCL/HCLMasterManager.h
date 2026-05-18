#pragma once

#include "HCLMaster.h"
#include "knxprod.h"
#include <Arduino.h>

namespace HCL {

/**
 * @brief Provider interface implemented by the owner of HCL::Master instances
 *        (typically `LightManagerModule`). Allows the stateless
 *        MasterManager facade to look up channel-hosted masters by number.
 *
 *        Master numbers passed across the interface are 1-based (1..N).
 */
class IMasterProvider {
public:
    virtual ~IMasterProvider() = default;

    /** Number of configured masters (0..MAX_MASTERS). */
    virtual uint8_t providerMasterCount() const = 0;

    /** Lookup a master by 1-based number. Returns nullptr if out-of-range. */
    virtual Master* providerGetMaster(uint8_t masterNum) = 0;

    /** Last interpolated value for a master (cached in the channel). */
    virtual InterpolatedValue providerGetCurrentValue(uint8_t masterNum) const = 0;

    /** Update the cached interpolated value for a master. */
    virtual void providerSetCurrentValue(uint8_t masterNum, const InterpolatedValue& value) = 0;

    /** Per-master apply-blocked flag (HCL output suppressed). */
    virtual bool providerIsMasterApplyBlocked(uint8_t masterNum) const = 0;
    virtual void providerSetMasterApplyBlocked(uint8_t masterNum, bool blocked) = 0;

    /** Is the adaptive brightness curve currently active for this master? */
    virtual bool providerIsMasterAdaptiveActive(uint8_t masterNum, uint16_t currentTimeMinutes, uint32_t nowMs) const = 0;
};

/**
 * @brief Stateless facade over channel-hosted HCL::Master instances.
 *
 * The MasterManager itself no longer stores Master objects — those live
 * inside `LightManagerChannel`. The manager keeps only global runtime state
 * (enable/apply-blocked, update interval, fade duration, scheduling clock)
 * and delegates all per-master accesses to the registered `IMasterProvider`.
 *
 * When no provider is registered, all per-master lookups return safe
 * defaults (nullptr / zero / false).
 */
class MasterManager {
public:
    // Follows OAM-defined NumChannels (LMG_ChannelCount from knxprod.h).
    static constexpr uint8_t MAX_MASTERS = LMG_ChannelCount;
    static_assert(LMG_ChannelCount > 0 && LMG_ChannelCount <= 255,
                  "LMG_ChannelCount must fit into uint8_t");

    MasterManager();

    /** Register the channel-owning module as master provider. */
    void setProvider(IMasterProvider* provider) { _provider = provider; }

    /** Reset internal scheduling clocks. */
    void setup();

    /** Periodic update — recalculate cached values when time advances. */
    void loop(uint16_t currentTimeMinutes, int16_t dayOfYear = -1);

    Master* getMaster(uint8_t masterNum);
    InterpolatedValue getCurrentValue(uint8_t masterNum) const;

    bool isEnabled() const { return _enabled; }
    void setEnabled(bool enabled) { _enabled = enabled; }

    /** Number of configured masters — delegated to provider (0 if none). */
    uint8_t getMasterCount() const { return _provider ? _provider->providerMasterCount() : 0; }

    /** Global apply-block (affects all masters). */
    void setApplyBlocked(bool blocked) { _applyBlocked = blocked; }
    bool isApplyBlocked() const { return _applyBlocked; }

    /** Per-master apply-block — delegated to provider. */
    void setMasterApplyBlocked(uint8_t masterNum, bool blocked);
    bool isMasterApplyBlocked(uint8_t masterNum) const;

    /** Last calculated time-of-day in minutes (or 0xFFFF before first loop). */
    uint16_t getLastTimeMinutes() const { return _lastTimeMinutes; }

    void setUpdateInterval(uint16_t seconds) { _updateIntervalMs = static_cast<uint32_t>(seconds) * 1000UL; }
    uint16_t getUpdateInterval() const { return static_cast<uint16_t>(_updateIntervalMs / 1000UL); }

    void setFadeDuration(uint8_t seconds) { _fadeDurationSec = seconds; }
    uint8_t getFadeDuration() const { return _fadeDurationSec; }

    void forceUpdate();
    uint32_t getTimeUntilNextUpdate() const;

    // --- Adaptive Helligkeit (delegating to master via provider) ---
    void setMasterAmbientLux(uint8_t masterNum, float lux);
    void setMasterDaytime(uint8_t masterNum, bool isDaytime);
    bool isMasterAdaptiveActive(uint8_t masterNum) const;

private:
    IMasterProvider* _provider = nullptr;
    bool _enabled;
    bool _applyBlocked;
    uint32_t _updateIntervalMs;
    uint8_t _fadeDurationSec;
    uint32_t _lastUpdateMs;
    uint16_t _lastTimeMinutes;
    int16_t _lastDayOfYear;

    void updateCurrentValues(uint16_t currentTimeMinutes, int16_t dayOfYear);
};

// Global facade instance (no static-init order traps — holds no Master state).
extern MasterManager masterManager;

} // namespace HCL
