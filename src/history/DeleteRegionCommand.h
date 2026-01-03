#pragma once

#include "ICommand.h"
#include <vector>

namespace Cartograph {

/**
 * Command to delete a region.
 * Saves region data and room assignments for undo.
 */
class DeleteRegionCommand : public ICommand {
public:
    explicit DeleteRegionCommand(const std::string& regionId);
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    
private:
    std::string m_regionId;
    RegionGroup m_savedRegion;
    std::vector<std::string> m_orphanedRoomIds;  // Rooms that were in region
};

} // namespace Cartograph

