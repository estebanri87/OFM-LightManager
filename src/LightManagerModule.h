#pragma once

#include <Arduino.h>
#include <time.h>
#include <vector>
#include "OpenKNX.h"
#include "HCL/HCLMasterManager.h"
#include "ILightManagerOutput.h"

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
    void notifyOutputActive(uint8_t masterNum, bool active);

    // Public for use by HueGatewayModule during migration (Phase 4)
    void setHclLock(bool active, const char* reason);
    void setHclManagerLock(uint8_t managerNumber, bool active, const char* reason);

private:
    struct OutputRegistration {
        uint8_t masterNum;
        ILightManagerOutput* target;
    };

    std::vector<OutputRegistration> _outputs;

    // Per-master push state
    HCL::InterpolatedValue _lastPushedValues[HCL::MasterManager::MAX_MASTERS];
    bool _lastBlockedState[HCL::MasterManager::MAX_MASTERS];

    // Global HCL lock state
    bool     _hclLockActive;
    uint8_t  _hclLockFallbackMode;
    uint8_t  _hclFallbackPolicy;
    uint32_t _hclFallbackDurationMs;
    uint16_t _hclFallbackReleaseMinuteOfDay;
    unsigned long _hclLockActivatedMs;
    unsigned long _hclLockAutoReleaseMs;
    int16_t  _hclLockActivationDayOfYear;
    int16_t  _hclLockActivationMinuteOfDay;

    // Per-master HCL lock state
    bool          _hclManagerLockActive[HCL::MasterManager::MAX_MASTERS];
    uint8_t       _hclManagerLockFallbackMode[HCL::MasterManager::MAX_MASTERS];
    uint8_t       _hclManagerFallbackPolicy[HCL::MasterManager::MAX_MASTERS];
    uint32_t      _hclManagerFallbackDurationMs[HCL::MasterManager::MAX_MASTERS];
    uint16_t      _hclManagerFallbackReleaseMinuteOfDay[HCL::MasterManager::MAX_MASTERS];
    unsigned long _hclManagerLockActivatedMs[HCL::MasterManager::MAX_MASTERS];
    unsigned long _hclManagerLockAutoReleaseMs[HCL::MasterManager::MAX_MASTERS];
    int16_t       _hclManagerLockActivationDayOfYear[HCL::MasterManager::MAX_MASTERS];
    int16_t       _hclManagerLockActivationMinuteOfDay[HCL::MasterManager::MAX_MASTERS];

    void pushToOutputs(uint8_t masterNum, const HCL::InterpolatedValue& value);
    void setupHclFromParams();

    void publishHclLockStatus();
    void publishHclManagerLockStatus(uint8_t managerNumber);

    void evaluateHclLockFallback(const tm* timeinfo, bool hasTime);
    void evaluateHclManagerLockFallback(const tm* timeinfo, bool hasTime);

    uint32_t getHclFallbackDurationMs(HclLockFallbackMode mode) const;
    bool shouldReleaseByPolicyTime(int16_t activationDayOfYear, int16_t activationMinuteOfDay,
                                   uint16_t releaseMinuteOfDay, const tm* timeinfo, bool hasTime) const;
};

extern LightManagerModule openknxLightManagerModule;
