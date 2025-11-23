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
     * @param cmd Command to add (takes ownership)
     * @param model Model to operate on
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
    bool TryCoalesce(ICommand* other, uint64_t timeDelta, float distanceSq) override;
    
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

} // namespace Cartograph

