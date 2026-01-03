#pragma once

#include "ICommand.h"
#include <vector>

namespace Cartograph {

/**
 * Command to delete marker(s).
 */
class DeleteMarkerCommand : public ICommand {
public:
    explicit DeleteMarkerCommand(const std::vector<std::string>& markerIds);
    explicit DeleteMarkerCommand(const std::string& markerId);
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    
private:
    std::vector<std::string> m_markerIds;      // IDs to delete
    std::vector<Marker> m_deletedMarkers;      // Saved state for undo
};

} // namespace Cartograph

