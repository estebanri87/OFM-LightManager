#pragma once

#include <Arduino.h>

namespace HCL {

/**
 * @brief Represents a single HCL setpoint with time, color temperature and brightness
 */
struct Setpoint {
    uint16_t timeMinutes;  // Time of day in minutes since midnight (0-1439)
    uint16_t kelvin;       // Color temperature in Kelvin (2000-6500)
    uint8_t brightness;    // Brightness in percent (0-100)
    
    /**
     * @brief Default constructor
     */
    Setpoint() : timeMinutes(0), kelvin(4000), brightness(100) {}
    
    /**
     * @brief Constructor with values
     */
    Setpoint(uint16_t time, uint16_t k, uint8_t b)
        : timeMinutes(time), kelvin(k), brightness(b) {}

    /**
     * @brief Check if setpoint is valid
     */
    bool isValid() const {
        return timeMinutes < 1440 && kelvin >= 2000 && kelvin <= 6500 && brightness <= 100;
    }
};

/**
 * @brief Interpolated values at a specific time
 */
struct InterpolatedValue {
    uint16_t kelvin;
    uint8_t brightness;
    
    InterpolatedValue() : kelvin(4000), brightness(100) {}
    InterpolatedValue(uint16_t k, uint8_t b) : kelvin(k), brightness(b) {}
};

} // namespace HCL
