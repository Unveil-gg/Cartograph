#pragma once

#include <string>
#include <vector>

// Forward declaration for SDL_GPU
struct SDL_GPUDevice;

// Forward declarations
namespace Cartograph {
    class App;
    class Model;
    class Canvas;
    class History;
    class IconManager;
    class JobQueue;
    class KeymapManager;
    class UI;
}

namespace Cartograph {

/**
 * Recent project entry for welcome screen.
 */
struct RecentProject {
    std::string path;
    std::string name;
    std::string description;         // Project description from project.json
    std::string lastModified;
    std::string thumbnailPath;       // Path to thumbnail image file
    unsigned int thumbnailTextureId; // GPU texture ID (0 if not loaded)
    bool thumbnailLoaded;            // Whether texture has been loaded
    
    RecentProject() 
        : thumbnailTextureId(0), thumbnailLoaded(false) {}
};

/**
 * Welcome screen UI - shown when no project is open.
 * Handles project selection, recent projects, and project creation.
 */
class WelcomeScreen {
public:
    WelcomeScreen(UI& ui);
    ~WelcomeScreen();
    
    /**
     * Render the welcome screen.
     * @param app Application instance
     * @param model Current model
     * @param canvas Canvas instance
     * @param history History manager
     * @param jobs Job queue
     * @param icons Icon manager
     * @param keymap Keymap manager
     */
    void Render(
        App& app,
        Model& model,
        Canvas& canvas,
        History& history,
        JobQueue& jobs,
        IconManager& icons,
        KeymapManager& keymap
    );
    
    /**
     * Load recent projects from persistent storage.
     */
    void LoadRecentProjects();
    
    /**
     * Add a project to the recent projects list.
     * @param path Project file or folder path
     */
    void AddRecentProject(const std::string& path);
    
    /**
     * Unload all thumbnail textures (cleanup when leaving welcome screen).
     */
    void UnloadThumbnailTextures();
    
    /**
     * Load thumbnail texture for a project.
     * @param project Project to load thumbnail for
     */
    void LoadThumbnailTexture(RecentProject& project);
    
    /**
     * Set the GPU device for texture creation.
     * @param device SDL_GPU device pointer
     */
    void SetGPUDevice(SDL_GPUDevice* device) { m_gpuDevice = device; }
    
    // Recent projects list
    std::vector<RecentProject> recentProjects;
    
    // Placeholder texture for missing thumbnails
    unsigned int placeholderTexture = 0;
    
private:
    /**
     * Render the list of recent projects.
     * @param app Application instance
     */
    void RenderRecentProjectsList(App& app);
    
    /**
     * Generate a placeholder texture for missing thumbnails.
     * @return GPU texture ID
     */
    unsigned int GeneratePlaceholderTexture();
    
    UI& m_ui;  // Reference to UI for accessing UI utilities
    SDL_GPUDevice* m_gpuDevice = nullptr;  // Non-owning pointer to GPU device
};

} // namespace Cartograph

