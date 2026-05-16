#pragma once

#include <Arduino.h>
#include <vector>
#include "OpenKNX.h"
#include "HCL/HCLMasterManager.h"
#include "ILightManagerOutput.h"

class LightManagerModule : public OpenKNX::Module
{
public:
    LightManagerModule();

    // OpenKNX::Module interface
    const std::string name() override;
    const std::string version() override;
    void setup() override;
    void loop() override;
    void processInputKo(GroupObject& ko) override;
    bool processCommand(const std::string cmd, bool diagnoseKo) override;

    /**
     * @brief Register an output target for a specific master.
     *
     * The target's onLightManagerValue() will be called whenever the master
     * calculates a new value and is not blocked.
     *
     * @param masterNum Master number (1-8)
     * @param target    Pointer to the output target (must remain valid)
     */
    void registerOutput(uint8_t masterNum, ILightManagerOutput* target);

    /**
     * @brief Notify the LightManager that an output has changed its active state.
     *
     * Call with active=false when a light is turned off. LightManagerModule
     * decides internally what this means for lock state.
     *
     * @param masterNum Master number (1-8)
     * @param active    true = output active (light on), false = output inactive (light off)
     */
    void notifyOutputActive(uint8_t masterNum, bool active);

private:
    struct OutputRegistration {
        uint8_t masterNum;
        ILightManagerOutput* target;
    };

    std::vector<OutputRegistration> _outputs;

    // Per-master state for push logic
    HCL::InterpolatedValue _lastPushedValues[HCL::MasterManager::MAX_MASTERS];
    bool _lastBlockedState[HCL::MasterManager::MAX_MASTERS];

    void pushToOutputs(uint8_t masterNum, const HCL::InterpolatedValue& value);
    void setupHclFromParams();
};

extern LightManagerModule openknxLightManagerModule;
