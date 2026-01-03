#pragma once

#include "ICommand.h"
#include <vector>

namespace Cartograph {

/**
 * Command to detect rooms from painted cells.
 * Supports undo/redo by tracking created rooms and cell assignment changes.
 */
class DetectRoomsCommand : public ICommand {
public:
    struct CellChange {
        int x, y;
        std::string oldRoomId;
        std::string newRoomId;
    };
    
    DetectRoomsCommand() = default;
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    
    // Get counts for UI feedback
    int GetCreatedCount() const { 
        return static_cast<int>(m_createdRooms.size()); 
    }
    int GetSplitCount() const { return m_splitCount; }
    
private:
    // Rooms created during detection (full data for undo)
    std::vector<Room> m_createdRooms;
    
    // Cell assignment changes (for both new rooms and splits)
    std::vector<CellChange> m_cellChanges;
    
    // Count of split operations (for description)
    int m_splitCount = 0;
    
    // Flag to avoid re-running detection on redo
    bool m_hasExecuted = false;
};

} // namespace Cartograph

