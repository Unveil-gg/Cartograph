#pragma once

#include "ICommand.h"

namespace Cartograph {

/**
 * Command to place or modify a marker.
 * If marker ID exists, modifies it; otherwise creates new marker.
 */
class PlaceMarkerCommand : public ICommand {
public:
    PlaceMarkerCommand(const Marker& marker, bool isNew = true);
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    
private:
    Marker m_marker;
    Marker m_oldMarker;  // For undo when modifying existing
    bool m_isNew;
};

} // namespace Cartograph

