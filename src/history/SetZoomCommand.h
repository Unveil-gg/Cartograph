#pragma once

#include "ICommand.h"

namespace Cartograph {

// Forward declaration
class Canvas;

/**
 * Command to set canvas zoom level.
 * Stores reference to Canvas for direct modification.
 */
class SetZoomCommand : public ICommand {
public:
    /**
     * Create a zoom command.
     * @param canvas Reference to canvas (must remain valid)
     * @param oldZoom Previous zoom level
     * @param newZoom New zoom level
     * @param displayPercent Display percentage for description
     */
    SetZoomCommand(Canvas& canvas, float oldZoom, float newZoom,
                   int displayPercent);
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    
private:
    Canvas& m_canvas;
    float m_oldZoom;
    float m_newZoom;
    int m_displayPercent;  // For description (e.g., "Zoom to 150%")
};

} // namespace Cartograph

