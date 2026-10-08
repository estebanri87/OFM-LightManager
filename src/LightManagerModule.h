#pragma once

#include <Arduino.h>
#include <memory>
#include <time.h>
#include <vector>
#include "OpenKNX.h"
#include "HCL/HCLMasterManager.h"
#include "ILightManagerOutput.h"
#include "LightManagerChannel.h"

class LightManagerModule : public OpenKNX::Module, public HCL::IMasterProvider
{
public:
    enum class HclLockFallbackMode : uint8_t {
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

    enum class HclLockFallbackPolicy : uint8_t {
        Legacy          = 0,
        Duration        = 1,
        TimeOfDay       = 2,
        DurationOrTime  = 3,
        ExternalOnly    = 4
    };

    LightManagerModule();

    // OpenKNX::Module interface
    const std::string name() override;
    const std::string version() override;
    void setup() override;
    void loop() override;
    void processInputKo(GroupObject& ko) override;
    bool processCommand(const std::string cmd, bool diagnoseKo) override;

    // Flash persistence for per-master summer state (season mode "per KO")
    uint16_t flashSize() override;
    void writeFlash() override;
    void readFlash(const uint8_t* data, const uint16_t size) override;

    void registerOutput(uint8_t masterNum, ILightManagerOutput* target);
    void unregisterOutput(ILightManagerOutput* target);
    void notifyOutputActive(uint8_t masterNum, bool active);

    // Per-channel timing accessors (1-based masterNum; 0 = invalid).
    uint16_t channelUpdateIntervalSec(uint8_t masterNum) const;
    uint8_t  channelFadeDurationSec(uint8_t masterNum) const;

    // Phase 2.J.c.2: expose channel pointer for diagnostics (HueGatewayModule).
    // Returns nullptr for channels that are deactivated or suspended in the ETS.
    LightManagerChannel* channel(uint8_t masterNum) {
        return (masterNum >= 1 && masterNum <= HCL::MasterManager::MAX_MASTERS)
            ? _channels[masterNum - 1].get() : nullptr;
    }
    const LightManagerChannel* channel(uint8_t masterNum) const {
        return (masterNum >= 1 && masterNum <= HCL::MasterManager::MAX_MASTERS)
            ? _channels[masterNum - 1].get() : nullptr;
    }

    // Channel is activated in the channel selection and not suspended (0-based index).
    bool channelConfigured(uint8_t channelIndex) const;
    // Number of channels that are running (activated and not suspended).
    uint8_t activeChannelCount() const;

    // Public API used by HueGatewayModule
    void setHclLock(bool active, const char* reason);
    void setHclManagerLock(uint8_t managerNumber, bool active, const char* reason);

    // --- HCL::IMasterProvider implementation -------------------------------
    // Highest possible master number. Channels are sparse: check channel(n) != nullptr.
    uint8_t           providerMasterCount() const override { return HCL::MasterManager::MAX_MASTERS; }
    HCL::Master*      providerGetMaster(uint8_t masterNum) override;
    HCL::InterpolatedValue providerGetCurrentValue(uint8_t masterNum) const override;
    void              providerSetCurrentValue(uint8_t masterNum, const HCL::InterpolatedValue& value) override;
    bool              providerIsMasterApplyBlocked(uint8_t masterNum) const override;
    void              providerSetMasterApplyBlocked(uint8_t masterNum, bool blocked) override;
    bool              providerIsMasterAdaptiveActive(uint8_t masterNum, uint16_t currentTimeMinutes, uint32_t nowMs) const override;

private:
    struct OutputRegistration {
        uint8_t masterNum;
        ILightManagerOutput* target;
    };

    // Punkt 5: Init-Pfad fuer SummerActive bei Magic-Mismatch / SavePower=Nein.
    void applySummerActiveInit();

    std::vector<OutputRegistration> _outputs;
    // Index = masterNum - 1; nullptr for deactivated or suspended channels.
    std::unique_ptr<LightManagerChannel> _channels[HCL::MasterManager::MAX_MASTERS];

    // Global HCL lock state (KO 400/401/402 — affects all masters)
    bool          _hclLockActive            = false;
    uint8_t       _hclLockFallbackMode      = 0;
    uint8_t       _hclFallbackPolicy        = 0;
    uint32_t      _hclFallbackDurationMs    = 0;
    uint16_t      _hclFallbackReleaseMinuteOfDay = 0xFFFF;
    unsigned long _hclLockActivatedMs       = 0;
    unsigned long _hclLockAutoReleaseMs     = 0;
    int16_t       _hclLockActivationDayOfYear = -1;
    int16_t       _hclLockActivationMinuteOfDay = -1;

    void publishHclLockStatus();
    void evaluateHclLockFallback(const tm* timeinfo, bool hasTime);
    uint32_t getHclFallbackDurationMs(HclLockFallbackMode mode) const;
    bool shouldReleaseByPolicyTime(int16_t activationDayOfYear, int16_t activationMinuteOfDay,
                                   uint16_t releaseMinuteOfDay, const tm* timeinfo, bool hasTime) const;
};

extern LightManagerModule openknxLightManagerModule;
