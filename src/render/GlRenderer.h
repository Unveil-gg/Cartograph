#pragma once

#include "Renderer.h"
#include <SDL3/SDL.h>
#include <imgui.h>
#include <memory>

// OpenGL types (forward declarations)
typedef unsigned int GLuint;
typedef unsigned int GLenum;

namespace Cartograph {

/**
 * RAII wrapper for OpenGL framebuffer objects.
 * Automatically cleans up GL resources on destruction.
 */
class FboHandle {
public:
    FboHandle(int w, int h);
    ~FboHandle();
    
    // Disable copy (OpenGL resources can't be copied)
    FboHandle(const FboHandle&) = delete;
    FboHandle& operator=(const FboHandle&) = delete;
    
    // Enable move
    FboHandle(FboHandle&& other) noexcept;
    FboHandle& operator=(FboHandle&& other) noexcept;
    
    GLuint GetFBO() const { return fbo; }
    GLuint GetColorTexture() const { return colorTexture; }
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }
    
    // Public for CreateFramebuffer to initialize
    GLuint fbo;
    GLuint colorTexture;
    GLuint depthRenderbuffer;
    int width, height;
    
private:
    void Cleanup();
};

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
    void SetRenderTarget(void* fbo) override;  // Accepts FboHandle* (non-owning)
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
     * @return RAII-managed FBO handle (automatically cleans up GL resources)
     */
    std::unique_ptr<FboHandle> CreateFramebuffer(int width, int height);
    
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

