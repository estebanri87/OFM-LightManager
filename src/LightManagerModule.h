#pragma once

#include <Arduino.h>
#include <memory>
#include <time.h>
#include <vector>
#include "OpenKNX.h"
#include "HCL/HCLMasterManager.h"
#include "ILightManagerOutput.h"
#include "LightManagerChannel.h"

class LightManagerModule : public OpenKNX::Module
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

    // Public API used by HueGatewayModule
    void setHclLock(bool active, const char* reason);
    void setHclManagerLock(uint8_t managerNumber, bool active, const char* reason);

private:
    struct OutputRegistration {
        uint8_t masterNum;
        ILightManagerOutput* target;
    };

    std::vector<OutputRegistration> _outputs;
    std::vector<std::unique_ptr<LightManagerChannel>> _channels;

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
