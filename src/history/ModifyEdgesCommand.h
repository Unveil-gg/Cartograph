#pragma once

#include "ICommand.h"
#include <vector>

namespace Cartograph {

/**
 * Command to modify edges (walls/doors).
 * Supports coalescing for continuous edge clicks.
 */
class ModifyEdgesCommand : public ICommand {
public:
    struct EdgeChange {
        EdgeId edgeId;
        EdgeState oldState;
        EdgeState newState;
    };
    
    explicit ModifyEdgesCommand(const std::vector<EdgeChange>& changes);
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    bool TryCoalesce(ICommand* other, uint64_t timeDelta, 
                     float distanceSq) override;
    
private:
    std::vector<EdgeChange> m_changes;
};

} // namespace Cartograph

