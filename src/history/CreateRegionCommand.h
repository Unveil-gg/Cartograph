#pragma once

#include "ICommand.h"

namespace Cartograph {

/**
 * Command to create a new region.
 * Supports undo by removing the created region.
 */
class CreateRegionCommand : public ICommand {
public:
    explicit CreateRegionCommand(const RegionGroup& region);
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    
    const std::string& GetRegionId() const { return m_region.id; }
    
private:
    RegionGroup m_region;
};

} // namespace Cartograph

