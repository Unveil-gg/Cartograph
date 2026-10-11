// Marker placement shared by the Marker tool click and icon drag-drop.

#include "CanvasPanel.h"
#include "../MarkerKinds.h"

namespace Cartograph {

/**
 * Place a marker. A new map-start marker demotes any existing start
 * marker to "custom" first, so the map keeps a single start.
 * @param model Model to edit
 * @param history Undo history
 * @param marker Marker to add (id, position and look already set)
 */
void CanvasPanel::PlaceNewMarker(
    Model& model, History& history, const Marker& marker
) {
    if (marker.kind == kKindStart) {
        std::vector<SetMarkerKindCommand::Change> demote;
        for (const auto& other : model.markers) {
            if (other.kind == kKindStart) {
                demote.push_back({other.id, kKindStart, kKindCustom});
            }
        }
        if (!demote.empty()) {
            history.AddCommand(
                std::make_unique<SetMarkerKindCommand>(demote), model);
        }
    }
    history.AddCommand(
        std::make_unique<PlaceMarkerCommand>(marker, true), model);
}

}  // namespace Cartograph
