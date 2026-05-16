#pragma once

#include <stdint.h>

/**
 * @brief Interface for receiving Light Manager push updates.
 *
 * Implement this interface in any class that wants to receive HCL values
 * from LightManagerModule. Register instances via LightManagerModule::registerOutput().
 */
class ILightManagerOutput
{
public:
    virtual ~ILightManagerOutput() = default;

    /**
     * @brief Called by LightManagerModule when a new value is available for a master.
     *
     * Only called when the master is NOT blocked. When blocked, no push occurs.
     * Also called immediately when the master transitions from blocked to unblocked.
     *
     * @param masterNum  Master number (1-8)
     * @param kelvin     Color temperature in Kelvin
     * @param brightness Brightness in percent (0-100)
     * @param fadeDuration Transition duration in seconds
     */
    virtual void onLightManagerValue(uint8_t masterNum, uint16_t kelvin, uint8_t brightness, uint8_t fadeDuration) = 0;
};
