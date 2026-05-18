#pragma once

#include "HCL/HCLMasterManager.h"
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
        ExternalOnly   = 4
    };

    explicit LightManagerChannel(uint8_t channelIndex);

    /** OpenKNX::Base override (pure virtual). */
    const std::string name() override { return "LightManagerChannel"; }

    /** Master number (1..16) used by HCL::masterManager API. */
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

    /** Persist summer state into bitmask (bit channelIndex) and read back. */
    bool isSummer() const;
    void restoreSummerFromMask(uint16_t mask);

    /** Force re-push of status on next pushIfChanged(). */
    void invalidatePushedState() { _lastBlocked = true; }

private:
    HCL::Master* master() const { return HCL::masterManager.getMaster(masterNumber()); }

    // Setup helpers
    void loadSetpoints();
    void loadSummerSetpoints();
    void applyAdvanced(float latitudeDeg, float longitudeDeg, int16_t timezoneOffsetMin);
    void loadAdaptive();
    void loadLockFallbackParams();
    void updateSeason(const tm* timeinfo);
    void evaluateLockFallback(const tm* timeinfo, bool hasTime);
    void publishLockStatus();
    void publishAdaptiveActive();
    uint32_t fallbackDurationMs(LockFallbackMode mode) const;
    bool shouldReleaseByPolicyTime(const tm* timeinfo, bool hasTime) const;

    // Lock state
    bool          _lockActive            = false;
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
};
