#pragma once

#include "Model.h"
#include "ExportPng.h"
#include <string>
#include <vector>

// Forward declarations
typedef unsigned int ImGuiID;

namespace Cartograph {

class Canvas;
class History;
class IconManager;

/**
 * Toast notification.
 */
struct Toast {
    std::string message;
    float remainingTime;
    enum class Type {
        Info, Success, Warning, Error
    } type;
};

/**
 * UI manager.
 * Handles all ImGui panels and windows.
 */
class UI {
public:
    UI();
    
    /**
     * Setup ImGui docking layout (call once at startup).
     */
    void SetupDockspace();
    
    /**
     * Render all UI panels.
     * @param model Current model
     * @param canvas Canvas
     * @param history History
     * @param icons Icon manager
     * @param deltaTime Frame delta time
     */
    void Render(
        Model& model,
        Canvas& canvas,
        History& history,
        IconManager& icons,
        float deltaTime
    );
    
    /**
     * Show a toast notification.
     * @param message Message text
     * @param type Toast type
     * @param duration Duration in seconds
     */
    void ShowToast(
        const std::string& message, 
        Toast::Type type = Toast::Type::Info,
        float duration = 3.0f
    );
    
    // UI state
    bool showExportModal = false;
    ExportOptions exportOptions;
    
    // Selected palette tile
    int selectedTileId = 1;
    
    // Current tool
    enum class Tool {
        Move, Select, Paint, Erase, Fill, Rectangle, Door, Marker, Eyedropper
    } currentTool = Tool::Move;
    
    // Selection state (for Select tool)
    bool isSelecting = false;
    float selectionStartX = 0.0f;
    float selectionStartY = 0.0f;
    float selectionEndX = 0.0f;
    float selectionEndY = 0.0f;
    
private:
    void RenderMenuBar(Model& model, Canvas& canvas, History& history);
    void RenderPalettePanel(Model& model);
    void RenderToolsPanel();
    void RenderPropertiesPanel(Model& model);
    void RenderCanvasPanel(Model& model, Canvas& canvas);
    void RenderStatusBar(Model& model, Canvas& canvas);
    void RenderToasts(float deltaTime);
    void RenderExportModal(Model& model, Canvas& canvas);
    
    /**
     * Build the fixed docking layout (called once at startup).
     * @param dockspaceId ImGui dockspace ID
     */
    void BuildFixedLayout(ImGuiID dockspaceId);
    
    std::vector<Toast> m_toasts;
    float m_statusBarHeight = 24.0f;
    bool m_layoutInitialized = false;
};

} // namespace Cartograph

