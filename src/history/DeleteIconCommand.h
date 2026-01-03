#pragma once

#include "ICommand.h"
#include <vector>
#include <cstdint>

namespace Cartograph {

// Forward declaration
class IconManager;

/**
 * Command to delete a custom icon.
 * Optionally removes all markers using the icon.
 * Fully undoable - restores icon and any deleted markers.
 */
class DeleteIconCommand : public ICommand {
public:
    /**
     * Create a delete icon command.
     * @param iconName Name of icon to delete
     * @param removeMarkers If true, also remove markers using this icon
     * @param iconManager Reference to icon manager for restoration
     */
    DeleteIconCommand(const std::string& iconName, bool removeMarkers,
                     IconManager& iconManager);
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    
    // Called by UI to capture icon state before execution
    void CaptureIconState();
    
private:
    std::string m_iconName;
    bool m_removeMarkers;
    IconManager& m_iconManager;  // Reference for undo restoration
    
    // Saved state for undo
    std::vector<uint8_t> m_savedPixels;
    int m_savedWidth;
    int m_savedHeight;
    std::string m_savedCategory;
    std::vector<Marker> m_deletedMarkers;
    bool m_iconCaptured;
};

} // namespace Cartograph

