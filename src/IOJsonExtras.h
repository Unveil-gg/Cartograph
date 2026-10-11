#pragma once

#include "Model.h"

#include <nlohmann/json.hpp>

namespace Cartograph {

/**
 * Write a Props table into obj["props"] (skipped when empty).
 * @param obj Target JSON object
 * @param props Table to write
 */
void SavePropsInto(nlohmann::json& obj, const Props& props);

/**
 * Read obj["props"]. Non-string scalars are stringified; entries past
 * Limits::MAX_PROPS or with oversized keys/values are skipped.
 * @param obj Source JSON object
 * @return Parsed table (empty when absent or malformed)
 */
Props LoadPropsFrom(const nlohmann::json& obj);

/**
 * Save sections added in version 3 (abilities, doorProps, interior).
 * @param model Model to save
 * @param j Root JSON object
 */
void SaveExtras(const Model& model, nlohmann::json& j);

/**
 * Load sections added in version 3. Missing sections leave defaults.
 * @param j Root JSON object
 * @param model Model to fill
 */
void LoadExtras(const nlohmann::json& j, Model& model);

}  // namespace Cartograph
