#pragma once

#include "../Model.h"
#include <string>
#include <cstdint>

namespace Cartograph {

/**
 * Abstract command interface for undo/redo.
 */
class ICommand {
public:
    virtual ~ICommand() = default;
    
    /**
     * Execute the command.
     */
    virtual void Execute(Model& model) = 0;
    
    /**
     * Undo the command.
     */
    virtual void Undo(Model& model) = 0;
    
    /**
     * Get command description for UI.
     */
    virtual std::string GetDescription() const = 0;
    
    /**
     * Try to coalesce with another command (for brush strokes).
     * @param other Another command (must be same type)
     * @param timeDelta Time since this command was created (ms)
     * @param distanceSq Squared distance between command centers
     * @return true if coalesced successfully
     */
    virtual bool TryCoalesce(
        ICommand* other, 
        uint64_t timeDelta, 
        float distanceSq
    ) {
        return false;  // By default, no coalescing
    }
};

} // namespace Cartograph

