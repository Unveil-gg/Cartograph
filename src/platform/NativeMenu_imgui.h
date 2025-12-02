#pragma once

#include "NativeMenu.h"
#include <map>

namespace Cartograph {

/**
 * ImGui-based menu implementation for Windows and Linux.
 * Renders menu bar using ImGui, similar to Phase 1 implementation.
 */
class NativeMenuImGui : public INativeMenu {
public:
    NativeMenuImGui();
    ~NativeMenuImGui() override;
    
    void Initialize() override;
    void Update(
        App& app,
        Model& model,
        Canvas& canvas,
        History& history,
        IconManager& icons,
        JobQueue& jobs
    ) override;
    void Render() override;
    bool IsNative() const override { return false; }
    void SetCallback(
        const std::string& action,
        MenuCallback callback
    ) override;
    
    void SetShowPropertiesPanel(bool* ptr) {
        m_showPropertiesPanel = ptr;
    }
    
private:
    void RenderMenuBar();
    
    // Callback map
    std::map<std::string, MenuCallback> m_callbacks;
    
    // Stored references for rendering
    App* m_app;
    Model* m_model;
    Canvas* m_canvas;
    History* m_history;
    IconManager* m_icons;
    JobQueue* m_jobs;
    
    // UI state for checkmarks
    bool* m_showPropertiesPanel;
    bool* m_showGrid;
};

} // namespace Cartograph

