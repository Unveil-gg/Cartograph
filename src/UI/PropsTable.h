#pragma once

#include "../ModelProps.h"

#include <string>
#include <vector>

namespace Cartograph {

// Suggested keys offered in the "add" dropdown. Free-form keys are fine.
extern const std::vector<const char*> kRoomPropKeys;
extern const std::vector<const char*> kRegionPropKeys;
extern const std::vector<const char*> kDoorPropKeys;

/**
 * ImGui editor for a Props table. Values edit in place, keys are fixed,
 * rows can be removed and new keys added. Keep one instance per call
 * site (a function-local static is fine).
 */
struct PropsEditor {
    Props before;           // Table as it was before the last commit
    char newKey[64] = "";
    char newVal[512] = "";

    /**
     * Draw the table inside the current ImGui window.
     * @param props Table to edit (modified immediately)
     * @param suggestions Keys offered in the dropdown
     * @param id Unique ImGui id for this table
     * @param hidden Keys not shown as rows (edited elsewhere)
     * @return true on a commit (value edit finished, row added or
     *         removed); `before` then holds the previous table
     */
    bool Render(
        Props& props,
        const std::vector<const char*>& suggestions,
        const char* id,
        const std::vector<std::string>& hidden = {}
    );
};

}  // namespace Cartograph
