#pragma once

#include "Model.h"
#include <memory>
#include <vector>
#include <string>

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

/**
 * History manager for undo/redo.
 * Supports command coalescing for continuous actions like painting.
 */
class History {
public:
    History();
    
    /**
     * Add a command to the history.
     * @param cmd Command to add (transfers ownership via unique_ptr)
     * @param model Model to operate on (borrowed reference)
     * @param execute If true, executes the command; if false, assumes 
     *                already applied
     */
    void AddCommand(std::unique_ptr<ICommand> cmd, Model& model, 
                    bool execute = true);
    
    /**
     * Undo the last command.
     */
    void Undo(Model& model);
    
    /**
     * Redo the next command.
     */
    void Redo(Model& model);
    
    /**
     * Clear all history.
     */
    void Clear();
    
    /**
     * Check if undo is available.
     */
    bool CanUndo() const;
    
    /**
     * Check if redo is available.
     */
    bool CanRedo() const;
    
    /**
     * Get the description of the command that would be undone.
     */
    std::string GetUndoDescription() const;
    
    /**
     * Get the description of the command that would be redone.
     */
    std::string GetRedoDescription() const;
    
    /**
     * Get the number of commands in the undo stack.
     */
    size_t GetUndoCount() const { return m_undoStack.size(); }
    
    /**
     * Get the number of commands in the redo stack.
     */
    size_t GetRedoCount() const { return m_redoStack.size(); }
    
private:
    std::vector<std::unique_ptr<ICommand>> m_undoStack;
    std::vector<std::unique_ptr<ICommand>> m_redoStack;
    uint64_t m_lastCommandTime;  // For coalescing
};

// ============================================================================
// Concrete command implementations
// ============================================================================

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
    
    PaintTilesCommand(const std::vector<TileChange>& changes);
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    bool TryCoalesce(ICommand* other, uint64_t timeDelta, 
                     float distanceSq) override;
    
private:
    std::vector<TileChange> m_changes;
};

/**
 * Command to fill connected tiles in a room.
 * Similar to PaintTilesCommand but for flood fill operations.
 */
class FillTilesCommand : public ICommand {
public:
    using TileChange = PaintTilesCommand::TileChange;
    
    FillTilesCommand(const std::vector<TileChange>& changes);
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    
private:
    std::vector<TileChange> m_changes;
};

/**
 * Command to create/modify/delete a room.
 */
class ModifyRoomCommand : public ICommand {
public:
    // TODO: Implement room modification command
    void Execute(Model& model) override {}
    void Undo(Model& model) override {}
    std::string GetDescription() const override { return "Modify Room"; }
};

/**
 * Command to assign/unassign cells to rooms.
 * Used for room painting mode.
 */
class ModifyRoomAssignmentsCommand : public ICommand {
public:
    struct CellAssignment {
        int x, y;
        std::string oldRoomId;  // Empty if no previous assignment
        std::string newRoomId;  // Empty to unassign
    };
    
    ModifyRoomAssignmentsCommand(
        const std::vector<CellAssignment>& assignments
    );
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    
private:
    std::vector<CellAssignment> m_assignments;
};

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
    
    ModifyEdgesCommand(const std::vector<EdgeChange>& changes);
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    bool TryCoalesce(ICommand* other, uint64_t timeDelta, 
                     float distanceSq) override;
    
private:
    std::vector<EdgeChange> m_changes;
};

/**
 * Command to place or modify a marker.
 * If marker ID exists, modifies it; otherwise creates new marker.
 */
class PlaceMarkerCommand : public ICommand {
public:
    PlaceMarkerCommand(const Marker& marker, bool isNew = true);
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    
private:
    Marker m_marker;
    Marker m_oldMarker;  // For undo when modifying existing
    bool m_isNew;
};

/**
 * Command to delete marker(s).
 */
class DeleteMarkerCommand : public ICommand {
public:
    DeleteMarkerCommand(const std::vector<std::string>& markerIds);
    explicit DeleteMarkerCommand(const std::string& markerId);
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    
private:
    std::vector<std::string> m_markerIds;      // IDs to delete
    std::vector<Marker> m_deletedMarkers;      // Saved state for undo
};

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
    
    MoveMarkersCommand(const std::vector<MarkerMove>& moves);
    MoveMarkersCommand(const std::string& markerId, 
                       float oldX, float oldY, 
                       float newX, float newY);
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    
private:
    std::vector<MarkerMove> m_moves;
};

/**
 * Snapshot of marker properties for undo/redo.
 * Used by ModifyMarkerPropertiesCommand.
 */
struct MarkerPropertiesSnapshot {
    std::string label;
    std::string icon;
    Color color;
    bool showLabel;
    
    bool operator==(const MarkerPropertiesSnapshot& other) const {
        return label == other.label && icon == other.icon &&
               color.r == other.color.r && color.g == other.color.g &&
               color.b == other.color.b && color.a == other.color.a &&
               showLabel == other.showLabel;
    }
    
    bool operator!=(const MarkerPropertiesSnapshot& other) const {
        return !(*this == other);
    }
};

/**
 * Command to modify marker properties (label, icon, color).
 * Supports coalescing for rapid edits like typing.
 */
class ModifyMarkerPropertiesCommand : public ICommand {
public:
    ModifyMarkerPropertiesCommand(
        const std::string& markerId,
        const MarkerPropertiesSnapshot& oldProps,
        const MarkerPropertiesSnapshot& newProps
    );
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    bool TryCoalesce(ICommand* other, uint64_t timeDelta, 
                     float distanceSq) override;
    
private:
    std::string m_markerId;
    MarkerPropertiesSnapshot m_oldProps;
    MarkerPropertiesSnapshot m_newProps;
};

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
                     class IconManager& iconManager);
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    
    // Called by UI to capture icon state before execution
    void CaptureIconState();
    
private:
    std::string m_iconName;
    bool m_removeMarkers;
    class IconManager& m_iconManager;  // Reference for undo restoration
    
    // Saved state for undo
    std::vector<uint8_t> m_savedPixels;
    int m_savedWidth;
    int m_savedHeight;
    std::string m_savedCategory;
    std::vector<Marker> m_deletedMarkers;
    bool m_iconCaptured;
};

/**
 * Command to add a new color to the palette.
 */
class AddPaletteColorCommand : public ICommand {
public:
    AddPaletteColorCommand(const std::string& name, const Color& color);
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    
private:
    std::string m_name;
    Color m_color;
    int m_tileId;  // Set after first Execute
};

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

/**
 * Command to update palette color name/color.
 */
class UpdatePaletteColorCommand : public ICommand {
public:
    UpdatePaletteColorCommand(
        int tileId, 
        const std::string& newName, 
        const Color& newColor
    );
    
    void Execute(Model& model) override;
    void Undo(Model& model) override;
    std::string GetDescription() const override;
    
private:
    int m_tileId;
    std::string m_newName;
    Color m_newColor;
    std::string m_oldName;
    Color m_oldColor;
};

} // namespace Cartograph

