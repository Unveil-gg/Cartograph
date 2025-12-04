#pragma once

#include "Renderer.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <imgui.h>
#include <memory>
#include <vector>

namespace Cartograph {

/**
 * RAII wrapper for SDL_GPU render target textures.
 * Used for offscreen rendering (PNG export, thumbnails).
 */
class GpuRenderTarget {
public:
    GpuRenderTarget(SDL_GPUDevice* device, int w, int h);
    ~GpuRenderTarget();
    
    // Disable copy
    GpuRenderTarget(const GpuRenderTarget&) = delete;
    GpuRenderTarget& operator=(const GpuRenderTarget&) = delete;
    
    // Enable move
    GpuRenderTarget(GpuRenderTarget&& other) noexcept;
    GpuRenderTarget& operator=(GpuRenderTarget&& other) noexcept;
    
    SDL_GPUTexture* GetTexture() const { return m_texture; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    bool IsValid() const { return m_texture != nullptr; }
    
private:
    void Cleanup();
    
    SDL_GPUDevice* m_device;
    SDL_GPUTexture* m_texture;
    int m_width;
    int m_height;
};

/**
 * SDL_GPU renderer implementation.
 * Uses SDL3's cross-platform GPU API (Metal/Vulkan/D3D12).
 */
class SdlGpuRenderer : public IRenderer {
public:
    SdlGpuRenderer(SDL_Window* window, SDL_GPUDevice* device);
    ~SdlGpuRenderer() override;
    
    void BeginFrame() override;
    void EndFrame() override;
    void SetRenderTarget(void* target) override;
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
     * Create an offscreen render target for export rendering.
     * @param width, height Target dimensions
     * @return RAII-managed render target, nullptr on failure
     */
    std::unique_ptr<GpuRenderTarget> CreateRenderTarget(int width, int height);
    
    /**
     * Read pixels from a render target.
     * @param target The render target to read from
     * @param outData Output buffer (RGBA8, must be preallocated)
     * @return true on success
     */
    bool ReadPixels(GpuRenderTarget* target, uint8_t* outData);
    
    /**
     * Set custom draw list for offscreen rendering.
     * @param drawList Custom ImGui draw list, or nullptr for window list
     */
    void SetCustomDrawList(ImDrawList* drawList);
    
    /**
     * Get the GPU device (for texture creation).
     */
    SDL_GPUDevice* GetDevice() const { return m_device; }
    
    /**
     * Get current command buffer (valid during frame).
     */
    SDL_GPUCommandBuffer* GetCommandBuffer() const { return m_commandBuffer; }
    
    /**
     * Get swapchain texture format.
     */
    SDL_GPUTextureFormat GetSwapchainFormat() const { return m_swapchainFormat; }
    
private:
    SDL_Window* m_window;
    SDL_GPUDevice* m_device;
    SDL_GPUCommandBuffer* m_commandBuffer;
    SDL_GPUTexture* m_swapchainTexture;
    SDL_GPUTextureFormat m_swapchainFormat;
    
    GpuRenderTarget* m_currentTarget;
    ImDrawList* m_customDrawList;
    
    float m_clearColor[4];  // RGBA
    bool m_hasClearColor;
    
    // Scissor state
    int m_scissorX, m_scissorY, m_scissorW, m_scissorH;
    bool m_scissorEnabled;
};

} // namespace Cartograph

