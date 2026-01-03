#pragma once

#include "ICommand.h"
#include "PaintTilesCommand.h"
#include <vector>

namespace Cartograph {

/**
 * Command to fill connected tiles in a room.
 * Similar to PaintTilesCommand but for flood fill operations.
 */
class FillTilesCommand : public ICommand {
public:
    using TileChange = PaintTilesCommand::TileChange;
    
    explicit FillTilesCommand(const std::vector<TileChange>& changes);
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    
private:
    std::vector<TileChange> m_changes;
};

} // namespace Cartograph

