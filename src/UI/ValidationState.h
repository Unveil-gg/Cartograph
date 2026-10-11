#pragma once

#include "../Reachability.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Cartograph {

// State of the gate-validation window and canvas overlay.
struct ValidationState {
    bool open = false;
    bool overlay = true;          // Tint rooms on the canvas
    bool hasReport = false;
    uint64_t revision = 0;        // Model::revision when the report ran
    ValidationReport report;
    std::vector<std::string> lines;
};

}  // namespace Cartograph
