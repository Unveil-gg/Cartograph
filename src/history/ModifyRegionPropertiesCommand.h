#pragma once

#include "ICommand.h"
#include "RegionPropertiesSnapshot.h"

namespace Cartograph {

/**
 * Command to modify region properties (name, description, tags).
 * Supports coalescing for rapid edits like typing.
 */
class ModifyRegionPropertiesCommand : public ICommand {
public:
    ModifyRegionPropertiesCommand(
        const std::string& regionId,
        const RegionPropertiesSnapshot& oldProps,
        const RegionPropertiesSnapshot& newProps
    );
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    bool TryCoalesce(ICommand* other, uint64_t timeDelta, 
                     float distanceSq) override;
    
private:
    std::string m_regionId;
    RegionPropertiesSnapshot m_oldProps;
    RegionPropertiesSnapshot m_newProps;
};

} // namespace Cartograph

