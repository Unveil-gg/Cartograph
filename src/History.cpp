#include "History.h"
#include "Model.h"
#include "platform/Time.h"

namespace Cartograph {

// Coalescing thresholds
static const uint64_t COALESCE_TIME_MS = 150;
static const float COALESCE_DIST_SQ = 16.0f;  // 4 tiles squared

History::History()
    : m_lastCommandTime(0)
{
}

void History::AddCommand(std::unique_ptr<ICommand> cmd, Model& model, 
                         bool execute) {
    uint64_t now = Platform::GetTimestampMs();
    uint64_t timeDelta = now - m_lastCommandTime;
    m_lastCommandTime = now;
    
    // Try to coalesce with the last command
    if (!m_undoStack.empty() && timeDelta < COALESCE_TIME_MS) {
        if (m_undoStack.back()->TryCoalesce(cmd.get(), timeDelta, 
                                            COALESCE_DIST_SQ)) {
            // Coalesced successfully, re-execute if needed
            if (execute) {
                m_undoStack.back()->Execute(model);
            }
            return;
        }
    }
    
    // Execute the command if requested
    if (execute) {
        cmd->Execute(model);
    }
    
    // Add to undo stack
    m_undoStack.push_back(std::move(cmd));
    
    // Clear redo stack (branching)
    m_redoStack.clear();
    
    // Limit history size (optional)
    const size_t MAX_HISTORY = 100;
    if (m_undoStack.size() > MAX_HISTORY) {
        m_undoStack.erase(m_undoStack.begin());
    }
}

void History::Undo(Model& model) {
    if (!CanUndo()) return;
    
    auto cmd = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    
    cmd->Undo(model);
    
    m_redoStack.push_back(std::move(cmd));
    model.MarkDirty();
}

void History::Redo(Model& model) {
    if (!CanRedo()) return;
    
    auto cmd = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    
    cmd->Execute(model);
    
    m_undoStack.push_back(std::move(cmd));
    model.MarkDirty();
}

void History::Clear() {
    m_undoStack.clear();
    m_redoStack.clear();
}

bool History::CanUndo() const {
    return !m_undoStack.empty();
}

bool History::CanRedo() const {
    return !m_redoStack.empty();
}

std::string History::GetUndoDescription() const {
    if (m_undoStack.empty()) {
        return "";
    }
    return m_undoStack.back()->GetDescription();
}

std::string History::GetRedoDescription() const {
    if (m_redoStack.empty()) {
        return "";
    }
    return m_redoStack.back()->GetDescription();
}

// ============================================================================
// PaintTilesCommand
// ============================================================================

PaintTilesCommand::PaintTilesCommand(const std::vector<TileChange>& changes)
    : m_changes(changes)
{
}

void PaintTilesCommand::Execute(Model& model) {
    for (const auto& change : m_changes) {
        model.SetTileAt(change.roomId, change.x, change.y, change.newTileId);
    }
}

void PaintTilesCommand::Undo(Model& model) {
    for (const auto& change : m_changes) {
        model.SetTileAt(change.roomId, change.x, change.y, change.oldTileId);
    }
}

std::string PaintTilesCommand::GetDescription() const {
    return "Paint Tiles";
}

bool PaintTilesCommand::TryCoalesce(
    ICommand* other, 
    uint64_t timeDelta, 
    float distanceSq
) {
    // Only coalesce if within time/distance thresholds
    if (timeDelta > COALESCE_TIME_MS) {
        return false;
    }
    
    PaintTilesCommand* otherPaint = dynamic_cast<PaintTilesCommand*>(other);
    if (!otherPaint) {
        return false;
    }
    
    // Merge the changes
    // (Simple implementation - just append, more sophisticated logic could dedupe)
    m_changes.insert(
        m_changes.end(),
        otherPaint->m_changes.begin(),
        otherPaint->m_changes.end()
    );
    
    return true;
}

} // namespace Cartograph

