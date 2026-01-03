#pragma once

#include "ICommand.h"
#include "MarkerPropertiesSnapshot.h"

namespace Cartograph {

/**
 * Command to modify marker properties (label, icon, color).
 * Supports coalescing for rapid edits like typing.
 */
class ModifyMarkerPropertiesCommand : public ICommand {
public:
    ModifyMarkerPropertiesCommand(
        const std::string& markerId,
        const MarkerPropertiesSnapshot& oldProps,
        const MarkerPropertiesSnapshot& newProps
    );
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    bool TryCoalesce(ICommand* other, uint64_t timeDelta, 
                     float distanceSq) override;
    
private:
    std::string m_markerId;
    MarkerPropertiesSnapshot m_oldProps;
    MarkerPropertiesSnapshot m_newProps;
};

} // namespace Cartograph

