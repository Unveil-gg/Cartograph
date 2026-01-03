#include "RemovePaletteColorCommand.h"

namespace Cartograph {

RemovePaletteColorCommand::RemovePaletteColorCommand(
    int tileId, 
    int replacementTileId
) : m_tileId(tileId), 
    m_replacementTileId(replacementTileId),
    m_savedTile{} {}

void RemovePaletteColorCommand::Execute(Model& model) {
    // First execution - save tile data for undo
    if (m_savedTile.id == 0 && m_tileId != 0) {
        const TileType* tile = model.FindPaletteEntry(m_tileId);
        if (tile) {
            m_savedTile = *tile;
        }
    }
    
    // Replace all uses of this tile with replacement (if specified)
    if (m_replacementTileId >= 0) {
        // Only capture replacements on first execution
        if (m_tileReplacements.empty()) {
            for (auto& row : model.tiles) {
                for (const auto& run : row.runs) {
                    if (run.tileId == m_tileId) {
                        // Record each tile that needs replacement
                        for (int x = run.startX; x < run.startX + run.count; 
                             ++x) {
                            PaintTilesCommand::TileChange change;
                            change.roomId = row.roomId;
                            change.x = x;
                            change.y = row.y;
                            change.oldTileId = m_tileId;
                            change.newTileId = m_replacementTileId;
                            m_tileReplacements.push_back(change);
                        }
                    }
                }
            }
        }
        
        // Apply replacements
        for (const auto& change : m_tileReplacements) {
            model.SetTileAt(change.roomId, change.x, change.y, 
                          change.newTileId);
        }
    }
    
    // Remove from palette
    model.RemovePaletteColor(m_tileId);
}

void RemovePaletteColorCommand::Undo(Model& model) {
    // Restore the palette entry
    model.palette.push_back(m_savedTile);
    
    // Restore tiles that were replaced
    for (const auto& change : m_tileReplacements) {
        model.SetTileAt(change.roomId, change.x, change.y, change.oldTileId);
    }
    
    model.MarkDirty();
}

std::string RemovePaletteColorCommand::GetDescription() const {
    return "Remove Color: " + m_savedTile.name;
}

} // namespace Cartograph

