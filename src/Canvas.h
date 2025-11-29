#pragma once

#include "Model.h"
#include <vector>

namespace Cartograph {

class IRenderer;
class IconManager;
class GlRenderer;

/**
 * Canvas manages the view transformation and rendering of the map.
 * Handles pan, zoom, and coordinate transformations.
 */
class Canvas {
public:
    Canvas();
    
    // View transformation
    float offsetX = 0.0f;   // Pan offset in world coordinates
    float offsetY = 0.0f;
    float zoom = 2.5f;      // Internal zoom level (2.5 = 100% display)
    
    // Default zoom level (displayed as 100% to user)
    static constexpr float DEFAULT_ZOOM = 2.5f;
    
    bool showGrid = true;
    
    // Thumbnail cache - cached last render for thumbnails
    std::vector<uint8_t> cachedThumbnail;
    int cachedThumbnailWidth = 0;
    int cachedThumbnailHeight = 0;
    bool hasCachedThumbnail = false;
    
    // Update view (handle input, etc.)
    void Update(Model& model, float deltaTime);
    
    // Render the canvas to the current viewport
    void Render(
        IRenderer& renderer,
        const Model& model,
        IconManager* icons,
        int viewportX, int viewportY,
        int viewportW, int viewportH,
        const EdgeId* hoveredEdge = nullptr,
        bool showRoomOverlays = true,
        const Marker* selectedMarker = nullptr,
        const Marker* hoveredMarker = nullptr
    );
    
    // Coordinate transformations
    void ScreenToWorld(float sx, float sy, float* wx, float* wy) const;
    void WorldToScreen(float wx, float wy, float* sx, float* sy) const;
    void ScreenToTile(
        float sx, float sy, 
        int tileWidth, int tileHeight, 
        int* tx, int* ty
    ) const;
    void TileToWorld(
        int tx, int ty, 
        int tileWidth, int tileHeight, 
        float* wx, float* wy
    ) const;
    
    // View manipulation
    void SetZoom(float newZoom);
    void Pan(float dx, float dy);
    void FocusOnTile(int tx, int ty, int tileWidth, int tileHeight);
    
    // Queries
    bool IsVisible(const Rect& rect, int tileWidth, int tileHeight) const;
    
    // Get viewport coordinates (set during last Render() call)
    int GetViewportX() const { return m_vpX; }
    int GetViewportY() const { return m_vpY; }
    int GetViewportW() const { return m_vpW; }
    int GetViewportH() const { return m_vpH; }
    
    // Capture current canvas state for thumbnail generation
    // Must be called AFTER ImGui_ImplOpenGL3_RenderDrawData() to ensure
    // pixels are actually in the framebuffer
    void CaptureThumbnail(IRenderer& renderer, const Model& model,
                         int viewportX, int viewportY,
                         int viewportW, int viewportH);
    
private:
    // Render passes
    void RenderGrid(IRenderer& renderer, const GridConfig& grid);
    void RenderRooms(IRenderer& renderer, const Model& model);
    void RenderTiles(IRenderer& renderer, const Model& model);
    void RenderEdges(IRenderer& renderer, const Model& model,
                    const EdgeId* hoveredEdge);
    void RenderDoors(IRenderer& renderer, const Model& model);
    void RenderMarkers(IRenderer& renderer, const Model& model,
                      IconManager* icons, const Marker* selectedMarker,
                      const Marker* hoveredMarker);
    void RenderRoomOverlays(IRenderer& renderer, const Model& model);
    
    // Viewport state (set during Render)
    int m_vpX, m_vpY, m_vpW, m_vpH;
};

} // namespace Cartograph

