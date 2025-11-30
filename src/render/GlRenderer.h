#pragma once

#include "Renderer.h"
#include <SDL3/SDL.h>
#include <imgui.h>

namespace Cartograph {

/**
 * OpenGL 3.3 renderer implementation.
 * Uses immediate-mode style API backed by ImGui's draw list for simplicity.
 */
class GlRenderer : public IRenderer {
public:
    GlRenderer(SDL_Window* window);
    ~GlRenderer() override;
    
    void BeginFrame() override;
    void EndFrame() override;
    void SetRenderTarget(void* fbo) override;
    void SetScissor(int x, int y, int w, int h) override;
    void Clear(const Color& color) override;
    void DrawRect(
        float x, float y, float w, float h, 
        const Color& color
    ) override;
    void DrawRectOutline(
        float x, float y, float w, float h, 
        const Color& color, float thickness
    ) override;
    void DrawLine(
        float x1, float y1, float x2, float y2,
        const Color& color, float thickness
    ) override;
    void GetDrawableSize(int* outW, int* outH) const override;
    void GetWindowSize(int* outW, int* outH) const override;
    
    /**
     * Create an offscreen framebuffer for export rendering.
     * @param width, height Framebuffer dimensions
     * @return FBO handle (platform-specific, cast to GLuint internally)
     */
    void* CreateFramebuffer(int width, int height);
    
    /**
     * Destroy a framebuffer created by CreateFramebuffer.
     * @param fbo Framebuffer handle
     */
    void DestroyFramebuffer(void* fbo);
    
    /**
     * Read pixels from the current framebuffer.
     * @param x, y Top-left corner
     * @param width, height Read dimensions
     * @param outData Output buffer (RGBA8, must be preallocated)
     */
    void ReadPixels(int x, int y, int width, int height, uint8_t* outData);
    
    /**
     * Set custom draw list for offscreen rendering.
     * If nullptr, uses window draw list (default).
     * @param drawList Custom ImGui draw list for export, or nullptr
     */
    void SetCustomDrawList(ImDrawList* drawList);
    
private:
    SDL_Window* m_window;
    SDL_GLContext m_context;
    void* m_currentFbo;  // Current render target
    ImDrawList* m_customDrawList;  // Custom draw list for offscreen
};

} // namespace Cartograph

