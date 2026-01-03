#pragma once

#include "ICommand.h"
#include <vector>

namespace Cartograph {

/**
 * Command to move marker(s).
 * Supports undo/redo for repositioning.
 */
class MoveMarkersCommand : public ICommand {
public:
    struct MarkerMove {
        std::string markerId;
        float oldX, oldY;
        float newX, newY;
    };
    
    explicit MoveMarkersCommand(const std::vector<MarkerMove>& moves);
    MoveMarkersCommand(const std::string& markerId, 
                       float oldX, float oldY, 
                       float newX, float newY);
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    
private:
    std::vector<MarkerMove> m_moves;
};

} // namespace Cartograph

