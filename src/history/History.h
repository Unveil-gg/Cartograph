#pragma once

#include "ICommand.h"
#include <memory>
#include <vector>

namespace Cartograph {

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

} // namespace Cartograph

