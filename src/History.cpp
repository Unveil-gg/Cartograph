#include "History.h"
#include "Model.h"
#include "Icons.h"
#include "platform/Time.h"

namespace Cartograph {

// Coalescing thresholds
static const uint64_t COALESCE_TIME_MS = 150;
static const float COALESCE_DIST_SQ = 16.0f;  // 4 tiles squared
static const uint64_t PROPERTY_COALESCE_TIME_MS = 300;  // Longer for typing

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

// ============================================================================
// FillTilesCommand
// ============================================================================

FillTilesCommand::FillTilesCommand(const std::vector<TileChange>& changes)
    : m_changes(changes)
{
}

void FillTilesCommand::Execute(Model& model) {
    for (const auto& change : m_changes) {
        model.SetTileAt(change.roomId, change.x, change.y, change.newTileId);
    }
}

void FillTilesCommand::Undo(Model& model) {
    for (const auto& change : m_changes) {
        model.SetTileAt(change.roomId, change.x, change.y, change.oldTileId);
    }
}

std::string FillTilesCommand::GetDescription() const {
    return "Fill Tiles";
}

// ============================================================================
// ModifyEdgesCommand
// ============================================================================

ModifyEdgesCommand::ModifyEdgesCommand(
    const std::vector<EdgeChange>& changes
) : m_changes(changes) {
}

void ModifyEdgesCommand::Execute(Model& model) {
    for (const auto& change : m_changes) {
        model.SetEdgeState(change.edgeId, change.newState);
    }
}

void ModifyEdgesCommand::Undo(Model& model) {
    for (const auto& change : m_changes) {
        model.SetEdgeState(change.edgeId, change.oldState);
    }
}

std::string ModifyEdgesCommand::GetDescription() const {
    return "Modify Edges";
}

bool ModifyEdgesCommand::TryCoalesce(
    ICommand* other,
    uint64_t timeDelta,
    float distanceSq
) {
    // Only coalesce if within time threshold
    if (timeDelta > COALESCE_TIME_MS) {
        return false;
    }
    
    ModifyEdgesCommand* otherEdge = dynamic_cast<ModifyEdgesCommand*>(other);
    if (!otherEdge) {
        return false;
    }
    
    // Merge the changes (append new changes)
    m_changes.insert(
        m_changes.end(),
        otherEdge->m_changes.begin(),
        otherEdge->m_changes.end()
    );
    
    return true;
}

// ============================================================================
// CreateRoomCommand
// ============================================================================

CreateRoomCommand::CreateRoomCommand(const Room& room)
    : m_room(room) {
}

void CreateRoomCommand::Execute(Model& model) {
    // Add room to model
    model.rooms.push_back(m_room);
    model.MarkDirty();
}

void CreateRoomCommand::Undo(Model& model) {
    // Remove the room we created
    auto it = std::find_if(
        model.rooms.begin(),
        model.rooms.end(),
        [this](const Room& r) { return r.id == m_room.id; }
    );
    if (it != model.rooms.end()) {
        model.rooms.erase(it);
    }
    model.MarkDirty();
}

std::string CreateRoomCommand::GetDescription() const {
    return "Create Room: " + m_room.name;
}

// ============================================================================
// DeleteRoomCommand
// ============================================================================

DeleteRoomCommand::DeleteRoomCommand(const std::string& roomId)
    : m_roomId(roomId) {
}

void DeleteRoomCommand::Execute(Model& model) {
    // Find and save room data (only first time)
    if (m_savedRoom.id.empty()) {
        const Room* room = model.FindRoom(m_roomId);
        if (room) {
            m_savedRoom = *room;
        }
        
        // Save cell assignments for this room
        for (const auto& [cell, assignedRoomId] : model.cellRoomAssignments) {
            if (assignedRoomId == m_roomId) {
                m_savedCellAssignments.push_back(cell);
            }
        }
    }
    
    // Clear all cell assignments for this room
    model.ClearAllCellsForRoom(m_roomId);
    
    // Remove room from model
    auto it = std::find_if(
        model.rooms.begin(),
        model.rooms.end(),
        [this](const Room& r) { return r.id == m_roomId; }
    );
    if (it != model.rooms.end()) {
        model.rooms.erase(it);
    }
    
    model.MarkDirty();
}

void DeleteRoomCommand::Undo(Model& model) {
    // Restore the room
    model.rooms.push_back(m_savedRoom);
    
    // Restore cell assignments
    for (const auto& cell : m_savedCellAssignments) {
        model.cellRoomAssignments[cell] = m_roomId;
    }
    
    // Invalidate room cell cache
    model.InvalidateRoomCellCache(m_roomId);
    model.MarkDirty();
}

std::string DeleteRoomCommand::GetDescription() const {
    return "Delete Room: " + m_savedRoom.name;
}

// ============================================================================
// ModifyRoomPropertiesCommand
// ============================================================================

ModifyRoomPropertiesCommand::ModifyRoomPropertiesCommand(
    const std::string& roomId,
    const RoomPropertiesSnapshot& oldProps,
    const RoomPropertiesSnapshot& newProps
) : m_roomId(roomId), m_oldProps(oldProps), m_newProps(newProps) {
}

void ModifyRoomPropertiesCommand::Execute(Model& model) {
    Room* room = model.FindRoom(m_roomId);
    if (room) {
        room->name = m_newProps.name;
        room->color = m_newProps.color;
        room->notes = m_newProps.notes;
        room->tags = m_newProps.tags;
        model.MarkDirty();
    }
}

void ModifyRoomPropertiesCommand::Undo(Model& model) {
    Room* room = model.FindRoom(m_roomId);
    if (room) {
        room->name = m_oldProps.name;
        room->color = m_oldProps.color;
        room->notes = m_oldProps.notes;
        room->tags = m_oldProps.tags;
        model.MarkDirty();
    }
}

std::string ModifyRoomPropertiesCommand::GetDescription() const {
    return "Modify Room: " + m_newProps.name;
}

bool ModifyRoomPropertiesCommand::TryCoalesce(
    ICommand* other,
    uint64_t timeDelta,
    float /*distanceSq*/
) {
    // Only coalesce within time threshold
    if (timeDelta > PROPERTY_COALESCE_TIME_MS) {
        return false;
    }
    
    auto* otherCmd = dynamic_cast<ModifyRoomPropertiesCommand*>(other);
    if (!otherCmd) {
        return false;
    }
    
    // Only coalesce if same room
    if (m_roomId != otherCmd->m_roomId) {
        return false;
    }
    
    // Keep original oldProps, update to latest newProps
    m_newProps = otherCmd->m_newProps;
    return true;
}

// ============================================================================
// ModifyRoomAssignmentsCommand
// ============================================================================

ModifyRoomAssignmentsCommand::ModifyRoomAssignmentsCommand(
    const std::vector<CellAssignment>& assignments
) : m_assignments(assignments) {
}

void ModifyRoomAssignmentsCommand::Execute(Model& model) {
    for (const auto& assignment : m_assignments) {
        model.SetCellRoom(assignment.x, assignment.y, assignment.newRoomId);
    }
}

void ModifyRoomAssignmentsCommand::Undo(Model& model) {
    for (const auto& assignment : m_assignments) {
        model.SetCellRoom(assignment.x, assignment.y, assignment.oldRoomId);
    }
}

std::string ModifyRoomAssignmentsCommand::GetDescription() const {
    return "Assign Room Cells";
}

// ============================================================================
// PlaceMarkerCommand
// ============================================================================

PlaceMarkerCommand::PlaceMarkerCommand(const Marker& marker, bool isNew)
    : m_marker(marker), m_isNew(isNew)
{
    // If modifying existing marker, store old state for undo
    if (!isNew) {
        m_oldMarker = marker;  // Will be updated in Execute
    }
}

void PlaceMarkerCommand::Execute(Model& model) {
    if (m_isNew) {
        // Add new marker
        model.AddMarker(m_marker);
    } else {
        // Modify existing marker - save old state first
        const Marker* existing = model.FindMarker(m_marker.id);
        if (existing) {
            m_oldMarker = *existing;
        }
        
        // Update marker
        Marker* toUpdate = model.FindMarker(m_marker.id);
        if (toUpdate) {
            *toUpdate = m_marker;
            model.MarkDirty();
        }
    }
}

void PlaceMarkerCommand::Undo(Model& model) {
    if (m_isNew) {
        // Remove the marker we added
        model.RemoveMarker(m_marker.id);
    } else {
        // Restore old marker state
        Marker* toRestore = model.FindMarker(m_marker.id);
        if (toRestore) {
            *toRestore = m_oldMarker;
            model.MarkDirty();
        }
    }
}

std::string PlaceMarkerCommand::GetDescription() const {
    return m_isNew ? "Place Marker" : "Modify Marker";
}

// ============================================================================
// DeleteMarkerCommand
// ============================================================================

DeleteMarkerCommand::DeleteMarkerCommand(
    const std::vector<std::string>& markerIds
) {
    m_markerIds = markerIds;
}

DeleteMarkerCommand::DeleteMarkerCommand(const std::string& markerId) {
    m_markerIds.push_back(markerId);
}

void DeleteMarkerCommand::Execute(Model& model) {
    // Capture marker state before deleting (for undo, only first time)
    if (m_deletedMarkers.empty()) {
        // First execution - capture state of markers to delete
        for (const auto& id : m_markerIds) {
            const Marker* marker = model.FindMarker(id);
            if (marker) {
                m_deletedMarkers.push_back(*marker);
            }
        }
    }
    
    // Delete markers
    for (const auto& id : m_markerIds) {
        model.RemoveMarker(id);
    }
}

void DeleteMarkerCommand::Undo(Model& model) {
    // Restore deleted markers
    for (const auto& marker : m_deletedMarkers) {
        model.AddMarker(marker);
    }
}

std::string DeleteMarkerCommand::GetDescription() const {
    if (m_deletedMarkers.size() == 1) {
        return "Delete Marker";
    } else {
        return "Delete " + std::to_string(m_deletedMarkers.size()) + 
               " Markers";
    }
}

// ============================================================================
// MoveMarkersCommand
// ============================================================================

MoveMarkersCommand::MoveMarkersCommand(const std::vector<MarkerMove>& moves)
    : m_moves(moves)
{
}

MoveMarkersCommand::MoveMarkersCommand(
    const std::string& markerId,
    float oldX, float oldY,
    float newX, float newY
) {
    m_moves.push_back({markerId, oldX, oldY, newX, newY});
}

void MoveMarkersCommand::Execute(Model& model) {
    for (const auto& move : m_moves) {
        Marker* marker = model.FindMarker(move.markerId);
        if (marker) {
            marker->x = move.newX;
            marker->y = move.newY;
        }
    }
    model.MarkDirty();
}

void MoveMarkersCommand::Undo(Model& model) {
    for (const auto& move : m_moves) {
        Marker* marker = model.FindMarker(move.markerId);
        if (marker) {
            marker->x = move.oldX;
            marker->y = move.oldY;
        }
    }
    model.MarkDirty();
}

std::string MoveMarkersCommand::GetDescription() const {
    if (m_moves.size() == 1) {
        return "Move Marker";
    } else {
        return "Move " + std::to_string(m_moves.size()) + " Markers";
    }
}

// ============================================================================
// ModifyMarkerPropertiesCommand
// ============================================================================

ModifyMarkerPropertiesCommand::ModifyMarkerPropertiesCommand(
    const std::string& markerId,
    const MarkerPropertiesSnapshot& oldProps,
    const MarkerPropertiesSnapshot& newProps
) : m_markerId(markerId), m_oldProps(oldProps), m_newProps(newProps) {
}

void ModifyMarkerPropertiesCommand::Execute(Model& model) {
    Marker* marker = model.FindMarker(m_markerId);
    if (marker) {
        marker->label = m_newProps.label;
        marker->icon = m_newProps.icon;
        marker->color = m_newProps.color;
        marker->showLabel = m_newProps.showLabel;
        model.MarkDirty();
    }
}

void ModifyMarkerPropertiesCommand::Undo(Model& model) {
    Marker* marker = model.FindMarker(m_markerId);
    if (marker) {
        marker->label = m_oldProps.label;
        marker->icon = m_oldProps.icon;
        marker->color = m_oldProps.color;
        marker->showLabel = m_oldProps.showLabel;
        model.MarkDirty();
    }
}

std::string ModifyMarkerPropertiesCommand::GetDescription() const {
    return "Modify Marker";
}

bool ModifyMarkerPropertiesCommand::TryCoalesce(
    ICommand* other,
    uint64_t timeDelta,
    float /*distanceSq*/
) {
    // Only coalesce within time threshold
    if (timeDelta > PROPERTY_COALESCE_TIME_MS) {
        return false;
    }
    
    auto* otherCmd = dynamic_cast<ModifyMarkerPropertiesCommand*>(other);
    if (!otherCmd) {
        return false;
    }
    
    // Only coalesce if same marker
    if (m_markerId != otherCmd->m_markerId) {
        return false;
    }
    
    // Keep original oldProps, update to latest newProps
    m_newProps = otherCmd->m_newProps;
    return true;
}

// ============================================================================
// DeleteIconCommand
// ============================================================================

DeleteIconCommand::DeleteIconCommand(const std::string& iconName, 
                                     bool removeMarkers,
                                     IconManager& iconManager)
    : m_iconName(iconName)
    , m_removeMarkers(removeMarkers)
    , m_iconManager(iconManager)
    , m_savedWidth(0)
    , m_savedHeight(0)
    , m_iconCaptured(false)
{
}

void DeleteIconCommand::CaptureIconState() {
    if (!m_iconCaptured) {
        m_iconManager.GetIconData(m_iconName, m_savedPixels, m_savedWidth, 
                                 m_savedHeight, m_savedCategory);
        m_iconCaptured = true;
    }
}

void DeleteIconCommand::Execute(Model& model) {
    // Capture marker state before deleting (only first time)
    if (m_removeMarkers && m_deletedMarkers.empty()) {
        // Get list of markers using this icon
        std::vector<std::string> markerIds = 
            model.GetMarkersUsingIcon(m_iconName);
        
        // Save marker state for undo
        for (const auto& id : markerIds) {
            const Marker* marker = model.FindMarker(id);
            if (marker) {
                m_deletedMarkers.push_back(*marker);
            }
        }
    }
    
    // Delete markers if requested
    if (m_removeMarkers) {
        model.RemoveMarkersUsingIcon(m_iconName);
    }
    
    // Note: Icon deletion is handled by IconManager, not Model
    // The UI layer must call icons.DeleteIcon() separately
}

void DeleteIconCommand::Undo(Model& model) {
    // Restore icon to IconManager
    if (m_iconCaptured && !m_savedPixels.empty()) {
        m_iconManager.AddIconFromMemory(
            m_iconName,
            m_savedPixels.data(),
            m_savedWidth,
            m_savedHeight,
            m_savedCategory
        );
        m_iconManager.BuildAtlas();
    }
    
    // Restore deleted markers
    for (const auto& marker : m_deletedMarkers) {
        model.AddMarker(marker);
    }
}

std::string DeleteIconCommand::GetDescription() const {
    if (m_removeMarkers && !m_deletedMarkers.empty()) {
        return "Delete Icon (+ " + std::to_string(m_deletedMarkers.size()) + 
               " markers)";
    }
    return "Delete Icon";
}

// ============================================================================
// AddPaletteColorCommand
// ============================================================================

AddPaletteColorCommand::AddPaletteColorCommand(
    const std::string& name, 
    const Color& color
) : m_name(name), m_color(color), m_tileId(-1) {
}

void AddPaletteColorCommand::Execute(Model& model) {
    if (m_tileId == -1) {
        // First execution - add color and capture ID
        m_tileId = model.AddPaletteColor(m_name, m_color);
    } else {
        // Redo - restore with same ID
        model.palette.push_back({m_tileId, m_name, m_color});
        model.MarkDirty();
    }
}

void AddPaletteColorCommand::Undo(Model& model) {
    // Remove the color we added
    model.RemovePaletteColor(m_tileId);
}

std::string AddPaletteColorCommand::GetDescription() const {
    return "Add Color: " + m_name;
}

// ============================================================================
// RemovePaletteColorCommand
// ============================================================================

RemovePaletteColorCommand::RemovePaletteColorCommand(
    int tileId, 
    int replacementTileId
) : m_tileId(tileId), 
    m_replacementTileId(replacementTileId),
    m_savedTile{} {
}

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

// ============================================================================
// UpdatePaletteColorCommand
// ============================================================================

UpdatePaletteColorCommand::UpdatePaletteColorCommand(
    int tileId,
    const std::string& newName,
    const Color& newColor
) : m_tileId(tileId), 
    m_newName(newName), 
    m_newColor(newColor),
    m_oldName(""),
    m_oldColor() {
}

void UpdatePaletteColorCommand::Execute(Model& model) {
    // Save old state on first execution
    if (m_oldName.empty()) {
        const TileType* tile = model.FindPaletteEntry(m_tileId);
        if (tile) {
            m_oldName = tile->name;
            m_oldColor = tile->color;
        }
    }
    
    // Apply new values
    model.UpdatePaletteColor(m_tileId, m_newName, m_newColor);
}

void UpdatePaletteColorCommand::Undo(Model& model) {
    // Restore old values
    model.UpdatePaletteColor(m_tileId, m_oldName, m_oldColor);
}

std::string UpdatePaletteColorCommand::GetDescription() const {
    return "Update Color: " + m_newName;
}

} // namespace Cartograph

