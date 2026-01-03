#pragma once

#include "ICommand.h"
#include <vector>
#include <string>

namespace Cartograph {

/**
 * Command to paint tiles in a room.
 * Supports coalescing for continuous brush strokes.
 */
class PaintTilesCommand : public ICommand {
public:
    struct TileChange {
        std::string roomId;
        int x, y;
        int oldTileId;
        int newTileId;
    };
    
    explicit PaintTilesCommand(const std::vector<TileChange>& changes);
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    bool TryCoalesce(ICommand* other, uint64_t timeDelta, 
                     float distanceSq) override;
    
private:
    std::vector<TileChange> m_changes;
};

} // namespace Cartograph

