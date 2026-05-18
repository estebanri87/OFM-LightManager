#pragma once

/**
 * @file LightManagerApi.h
 * @brief Public consumer API of the OFM-LightManager module.
 *
 * This header bundles the symbols a consumer OAM (e.g. OFM-HueGatewayModule,
 * DALI gateway, MQTT bridge, ...) needs in order to:
 *   - read the current HCL master state (brightness / color temperature),
 *   - learn how many masters are configured,
 *   - check whether the LightManager has globally or per-master suspended
 *     output (apply-block / lock),
 *   - feed ambient-lux and day/night information back to a master.
 *
 * The LightManager itself stays the single source of truth — consumers
 * should treat all accessors as read-only snapshots and only react to
 * the values, never mutate internal state directly.
 *
 * Typical usage (consumer side, e.g. HueGatewayModule):
 * @code
 *   #include <HCL/LightManagerApi.h>
 *
 *   const uint8_t count = HCL::masterManager.getMasterCount();
 *   if (selectedMaster > 0 && selectedMaster <= count)
 *   {
 *       HCL::Master* m = HCL::masterManager.getMaster(selectedMaster);
 *       if (m && !HCL::masterManager.isApplyBlocked()
 *             && !HCL::masterManager.isMasterApplyBlocked(selectedMaster))
 *       {
 *           const auto v = HCL::masterManager.getCurrentValue(selectedMaster);
 *           applyToHardware(v.brightness, v.kelvin);
 *       }
 *   }
 * @endcode
 *
 * Cross-module ETS parameter type:
 *   Use `%AID%_PT-LMGMasterSelect` (defined in LightManagerModule.share.xml)
 *   to offer "master selection" dropdowns in consumer modules — this keeps
 *   the enum centralized and in sync across all OAMs.
 *
 * @note Master numbers are **1-based** (1..MAX_MASTERS). 0 means "no master
 *       assigned" and is the conventional "disabled" sentinel.
 */

#include "HCLMasterManager.h"   // HCL::masterManager, MasterManager, IMasterProvider
#include "HCLMaster.h"          // HCL::Master, HCL::InterpolatedValue

namespace HCL {

/**
 * @defgroup LightManagerConsumerApi LightManager Consumer API
 * @brief Read-only accessors a consumer OAM should rely on.
 *
 * All functions listed here are methods on the global ::HCL::masterManager
 * facade and remain valid even when no `LightManagerModule` is registered
 * (they return safe defaults: 0 / nullptr / false).
 *
 * - `MasterManager::getMasterCount()`         &mdash; number of configured masters (0..16).
 * - `MasterManager::getMaster(masterNum)`     &mdash; pointer to the master config, or `nullptr`.
 * - `MasterManager::getCurrentValue(num)`     &mdash; latest interpolated `{brightness, kelvin}`.
 * - `MasterManager::isEnabled()`              &mdash; global enable flag (KO/parameter driven).
 * - `MasterManager::isApplyBlocked()`         &mdash; global lock active (e.g. presence override).
 * - `MasterManager::isMasterApplyBlocked(num)`&mdash; per-master lock active.
 * - `MasterManager::isMasterAdaptiveActive(num)` &mdash; ambient/daytime curve currently in use.
 *
 * @{
 */

/**
 * @brief Convenience: report ambient illuminance to a master.
 *
 * Equivalent to `HCL::masterManager.setMasterAmbientLux(masterNum, lux)`.
 * Triggers an immediate recalculation of the cached interpolated value.
 */
inline void reportAmbientLux(uint8_t masterNum, float lux)
{
    masterManager.setMasterAmbientLux(masterNum, lux);
    masterManager.forceUpdate();
}

/**
 * @brief Convenience: report day/night state to a master.
 *
 * Equivalent to `HCL::masterManager.setMasterDaytime(masterNum, isDay)`.
 */
inline void reportDaytime(uint8_t masterNum, bool isDay)
{
    masterManager.setMasterDaytime(masterNum, isDay);
}

/** @} */

} // namespace HCL
