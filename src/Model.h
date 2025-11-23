#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

// Forward declarations
struct ImVec4;

namespace Cartograph {

// ============================================================================
// Color utilities
// ============================================================================

struct Color {
    float r, g, b, a;
    
    Color() : r(0), g(0), b(0), a(1) {}
    Color(float r, float g, float b, float a = 1.0f) 
        : r(r), g(g), b(b), a(a) {}
    
    // Parse from hex string "#RRGGBB" or "#RRGGBBAA"
    static Color FromHex(const std::string& hex);
    
    // Convert to hex string
    std::string ToHex(bool includeAlpha = true) const;
    
    // Convert to ImVec4
    ImVec4 ToImVec4() const;
    
    uint32_t ToU32() const;
};

// ============================================================================
// Grid configuration
// ============================================================================

struct GridConfig {
    int tileWidth = 16;   // pixels per tile width at 1:1 zoom
    int tileHeight = 16;  // pixels per tile height at 1:1 zoom
    int cols = 256;       // grid width in tiles
    int rows = 256;       // grid height in tiles
};

// ============================================================================
// Palette
// ============================================================================

struct TileType {
    int id;
    std::string name;
    Color color;
};

using Palette = std::vector<TileType>;

// ============================================================================
// Rooms
// ============================================================================

struct Rect {
    int x, y, w, h;  // All in tile coordinates
    
    bool Contains(int tx, int ty) const {
        return tx >= x && tx < x + w && ty >= y && ty < y + h;
    }
};

struct Room {
    std::string id;
    std::string name;
    Rect rect;
    Color color;
    std::string notes;
};

// ============================================================================
// Tiles (row-run encoding)
// ============================================================================

struct TileRun {
    int startX;
    int count;
    int tileId;
};

struct TileRow {
    std::string roomId;
    int y;  // Row index within room (0 = top)
    std::vector<TileRun> runs;
};

using TileData = std::vector<TileRow>;

// ============================================================================
// Doors
// ============================================================================

enum class DoorSide {
    North, South, East, West
};

enum class DoorType {
    Door,       // Regular door
    Elevator,   // Vertical transport
    Teleporter  // Long-distance warp
};

struct DoorEndpoint {
    std::string roomId;
    int x, y;  // Tile position
    DoorSide side;
};

struct Door {
    std::string id;
    DoorEndpoint a;
    DoorEndpoint b;
    DoorType type;
    std::vector<std::string> gate;  // Required abilities (not logic yet)
};

// ============================================================================
// Markers
// ============================================================================

struct Marker {
    std::string id;
    std::string roomId;
    int x, y;  // Tile position
    std::string kind;   // "save", "item", "boss", etc.
    std::string label;
    std::string icon;   // Icon name reference
    Color color;
};

// ============================================================================
// Keymap
// ============================================================================

struct KeyBinding {
    std::string action;
    std::string binding;  // e.g., "Ctrl+S", "Mouse1"
};

using Keymap = std::unordered_map<std::string, std::string>;

// ============================================================================
// Theme
// ============================================================================

struct Theme {
    std::string name;
    float uiScale = 1.0f;
    std::unordered_map<std::string, Color> mapColors;  // Override defaults
    
    // Predefined theme colors (set by ApplyTheme)
    Color background;
    Color gridLine;
    Color roomOutline;
    Color roomFill;
    Color doorColor;
    Color markerColor;
    Color textColor;
};

// ============================================================================
// Metadata
// ============================================================================

struct Metadata {
    std::string title;
    std::string author;
};

// ============================================================================
// Model - Complete project state
// ============================================================================

class Model {
public:
    Model();
    
    // Core data
    GridConfig grid;
    Palette palette;
    std::vector<Room> rooms;
    TileData tiles;
    std::vector<Door> doors;
    std::vector<Marker> markers;
    Keymap keymap;
    Theme theme;
    Metadata meta;
    
    // Dirty tracking
    bool dirty = false;
    void MarkDirty();
    void ClearDirty();
    
    // Helper queries
    Room* FindRoom(const std::string& id);
    const Room* FindRoom(const std::string& id) const;
    TileRow* FindTileRow(const std::string& roomId, int y);
    int GetTileAt(const std::string& roomId, int x, int y) const;
    void SetTileAt(const std::string& roomId, int x, int y, int tileId);
    bool HasDoorAt(const std::string& roomId, int x, int y) const;
    
    // Initialize defaults
    void InitDefaults();
    void InitDefaultPalette();
    void InitDefaultKeymap();
    void InitDefaultTheme(const std::string& name = "Dark");
};

} // namespace Cartograph

