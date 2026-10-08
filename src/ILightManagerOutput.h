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

    /**
     * @brief Partial push (0.3.0, F11) — only the axes flagged in @p validMask
     *        carry fresh values; the other axes must be filled by the sink
     *        from its own cache (the LightManagerChannel is the cache-owner
     *        for non-valid axes).
     *
     * Default implementation forwards to onLightManagerValue() with the full
     * tuple, so existing 0.2.0 sinks keep working unchanged.
     *
     * @param masterNum    Master number (1-8)
     * @param kelvin       Color temperature in Kelvin (valid iff bit 0 of validMask)
     * @param brightness   Brightness in percent 0..100 (valid iff bit 1 of validMask)
     * @param validMask    Bit 0 = Kelvin valid, Bit 1 = Brightness valid,
     *                     Bits 2..7 reserved (= 0).
     * @param fadeDuration Transition duration in seconds
     */
    virtual void onLightManagerPartial(uint8_t masterNum,
                                       uint16_t kelvin,
                                       uint8_t brightness,
                                       uint8_t validMask,
                                       uint8_t fadeDuration)
    {
        onLightManagerValue(masterNum, kelvin, brightness, fadeDuration);
    }
};
