#pragma once

#include <cstdint>

namespace Cartograph {

struct Color;

/**
 * Abstract 2D renderer interface.
 * Provides simple primitives for drawing the canvas and UI overlays.
 * Text rendering is delegated to ImGui's font system.
 */
class IRenderer {
public:
    virtual ~IRenderer() = default;
    
    /**
     * Begin a new frame.
     */
    virtual void BeginFrame() = 0;
    
    /**
     * End the current frame and present.
     */
    virtual void EndFrame() = 0;
    
    /**
     * Set the render target (nullptr for default framebuffer).
     * @param fbo Framebuffer object handle (platform-specific)
     */
    virtual void SetRenderTarget(void* fbo) = 0;
    
    /**
     * Set scissor rectangle for clipping.
     * @param x, y Top-left corner
     * @param w, h Width and height (pass 0,0,0,0 to disable)
     */
    virtual void SetScissor(int x, int y, int w, int h) = 0;
    
    /**
     * Clear the current render target.
     * @param color Clear color
     */
    virtual void Clear(const Color& color) = 0;
    
    /**
     * Draw a filled rectangle.
     * @param x, y Top-left corner
     * @param w, h Width and height
     * @param color Fill color
     */
    virtual void DrawRect(
        float x, float y, float w, float h, 
        const Color& color
    ) = 0;
    
    /**
     * Draw a rectangle outline.
     * @param x, y Top-left corner
     * @param w, h Width and height
     * @param color Line color
     * @param thickness Line thickness in pixels
     */
    virtual void DrawRectOutline(
        float x, float y, float w, float h, 
        const Color& color, float thickness = 1.0f
    ) = 0;
    
    /**
     * Draw a line.
     * @param x1, y1 Start point
     * @param x2, y2 End point
     * @param color Line color
     * @param thickness Line thickness in pixels
     */
    virtual void DrawLine(
        float x1, float y1, float x2, float y2,
        const Color& color, float thickness = 1.0f
    ) = 0;
    
    /**
     * Get the drawable size (accounting for HiDPI).
     * @param outW, outH Output dimensions
     */
    virtual void GetDrawableSize(int* outW, int* outH) const = 0;
    
    /**
     * Get the window size (logical pixels).
     * @param outW, outH Output dimensions
     */
    virtual void GetWindowSize(int* outW, int* outH) const = 0;
};

} // namespace Cartograph

