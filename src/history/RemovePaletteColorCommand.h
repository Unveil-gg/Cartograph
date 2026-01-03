#pragma once

#include "ICommand.h"
#include "PaintTilesCommand.h"
#include <vector>

namespace Cartograph {

/**
 * Command to remove a color from the palette.
 * Optionally replaces all uses with a replacement tile ID.
 */
class RemovePaletteColorCommand : public ICommand {
public:
    RemovePaletteColorCommand(int tileId, int replacementTileId = 0);
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    
private:
    int m_tileId;
    int m_replacementTileId;
    TileType m_savedTile;  // For undo
    std::vector<PaintTilesCommand::TileChange> m_tileReplacements;  // Track
};

} // namespace Cartograph

