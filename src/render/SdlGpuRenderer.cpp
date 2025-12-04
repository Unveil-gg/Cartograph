#include "SdlGpuRenderer.h"
#include "../Model.h"
#include <imgui.h>
#include <cstring>

namespace Cartograph {

// -----------------------------------------------------------------------------
// GpuRenderTarget implementation
// -----------------------------------------------------------------------------

GpuRenderTarget::GpuRenderTarget(SDL_GPUDevice* device, int w, int h)
    : m_device(device)
    , m_texture(nullptr)
    , m_width(w)
    , m_height(h)
{
    SDL_GPUTextureCreateInfo texInfo = {};
    texInfo.type = SDL_GPU_TEXTURETYPE_2D;
    texInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texInfo.width = w;
    texInfo.height = h;
    texInfo.layer_count_or_depth = 1;
    texInfo.num_levels = 1;
    texInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
    texInfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | 
                    SDL_GPU_TEXTUREUSAGE_SAMPLER;
    
    m_texture = SDL_CreateGPUTexture(device, &texInfo);
}

GpuRenderTarget::~GpuRenderTarget() {
    Cleanup();
}

GpuRenderTarget::GpuRenderTarget(GpuRenderTarget&& other) noexcept
    : m_device(other.m_device)
    , m_texture(other.m_texture)
    , m_width(other.m_width)
    , m_height(other.m_height)
{
    other.m_texture = nullptr;
}

GpuRenderTarget& GpuRenderTarget::operator=(GpuRenderTarget&& other) noexcept {
    if (this != &other) {
        Cleanup();
        m_device = other.m_device;
        m_texture = other.m_texture;
        m_width = other.m_width;
        m_height = other.m_height;
        other.m_texture = nullptr;
    }
    return *this;
}

void GpuRenderTarget::Cleanup() {
    if (m_texture && m_device) {
        SDL_ReleaseGPUTexture(m_device, m_texture);
        m_texture = nullptr;
    }
}

// -----------------------------------------------------------------------------
// SdlGpuRenderer implementation
// -----------------------------------------------------------------------------

SdlGpuRenderer::SdlGpuRenderer(SDL_Window* window, SDL_GPUDevice* device)
    : m_window(window)
    , m_device(device)
    , m_commandBuffer(nullptr)
    , m_swapchainTexture(nullptr)
    , m_swapchainFormat(SDL_GPU_TEXTUREFORMAT_INVALID)
    , m_currentTarget(nullptr)
    , m_customDrawList(nullptr)
    , m_clearColor{0.0f, 0.0f, 0.0f, 1.0f}
    , m_hasClearColor(false)
    , m_scissorX(0)
    , m_scissorY(0)
    , m_scissorW(0)
    , m_scissorH(0)
    , m_scissorEnabled(false)
{
    // Get swapchain format for ImGui init
    m_swapchainFormat = SDL_GetGPUSwapchainTextureFormat(device, window);
}

SdlGpuRenderer::~SdlGpuRenderer() {
    // Device cleanup handled by App
}

void SdlGpuRenderer::BeginFrame() {
    // Acquire command buffer for this frame
    m_commandBuffer = SDL_AcquireGPUCommandBuffer(m_device);
    if (!m_commandBuffer) {
        return;
    }
    
    // Acquire swapchain texture
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            m_commandBuffer, m_window, &m_swapchainTexture, nullptr, nullptr)) {
        m_swapchainTexture = nullptr;
    }
    
    m_hasClearColor = false;
    m_currentTarget = nullptr;
}

void SdlGpuRenderer::EndFrame() {
    // Submit command buffer (presents the frame)
    if (m_commandBuffer) {
        SDL_SubmitGPUCommandBuffer(m_commandBuffer);
        m_commandBuffer = nullptr;
    }
    m_swapchainTexture = nullptr;
}

void SdlGpuRenderer::SetRenderTarget(void* target) {
    m_currentTarget = static_cast<GpuRenderTarget*>(target);
}

void SdlGpuRenderer::SetScissor(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) {
        m_scissorEnabled = false;
    } else {
        m_scissorEnabled = true;
        m_scissorX = x;
        m_scissorY = y;
        m_scissorW = w;
        m_scissorH = h;
    }
}

void SdlGpuRenderer::Clear(const Color& color) {
    m_clearColor[0] = color.r;
    m_clearColor[1] = color.g;
    m_clearColor[2] = color.b;
    m_clearColor[3] = color.a;
    m_hasClearColor = true;
}

void SdlGpuRenderer::DrawRect(
    float x, float y, float w, float h, 
    const Color& color
) {
    ImDrawList* dl = m_customDrawList ? m_customDrawList : 
                     ImGui::GetWindowDrawList();
    dl->AddRectFilled(
        ImVec2(x, y),
        ImVec2(x + w, y + h),
        color.ToU32()
    );
}

void SdlGpuRenderer::DrawRectOutline(
    float x, float y, float w, float h, 
    const Color& color, float thickness
) {
    ImDrawList* dl = m_customDrawList ? m_customDrawList : 
                     ImGui::GetWindowDrawList();
    dl->AddRect(
        ImVec2(x, y),
        ImVec2(x + w, y + h),
        color.ToU32(),
        0.0f,
        0,
        thickness
    );
}

void SdlGpuRenderer::DrawLine(
    float x1, float y1, float x2, float y2,
    const Color& color, float thickness
) {
    ImDrawList* dl = m_customDrawList ? m_customDrawList : 
                     ImGui::GetWindowDrawList();
    dl->AddLine(
        ImVec2(x1, y1),
        ImVec2(x2, y2),
        color.ToU32(),
        thickness
    );
}

void SdlGpuRenderer::GetDrawableSize(int* outW, int* outH) const {
    SDL_GetWindowSizeInPixels(m_window, outW, outH);
}

void SdlGpuRenderer::GetWindowSize(int* outW, int* outH) const {
    SDL_GetWindowSize(m_window, outW, outH);
}

std::unique_ptr<GpuRenderTarget> SdlGpuRenderer::CreateRenderTarget(
    int width, int height
) {
    auto target = std::make_unique<GpuRenderTarget>(m_device, width, height);
    if (!target->IsValid()) {
        return nullptr;
    }
    return target;
}

bool SdlGpuRenderer::ReadPixels(GpuRenderTarget* target, uint8_t* outData) {
    if (!target || !target->IsValid() || !outData) {
        return false;
    }
    
    int width = target->GetWidth();
    int height = target->GetHeight();
    size_t bufferSize = width * height * 4;
    
    // Create transfer buffer for download
    SDL_GPUTransferBufferCreateInfo transferInfo = {};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    transferInfo.size = static_cast<Uint32>(bufferSize);
    
    SDL_GPUTransferBuffer* transferBuffer = 
        SDL_CreateGPUTransferBuffer(m_device, &transferInfo);
    if (!transferBuffer) {
        return false;
    }
    
    // Create copy pass
    SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(m_device);
    if (!cmdBuf) {
        SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);
        return false;
    }
    
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);
    
    SDL_GPUTextureRegion srcRegion = {};
    srcRegion.texture = target->GetTexture();
    srcRegion.w = width;
    srcRegion.h = height;
    srcRegion.d = 1;
    
    SDL_GPUTextureTransferInfo dstInfo = {};
    dstInfo.transfer_buffer = transferBuffer;
    dstInfo.offset = 0;
    dstInfo.pixels_per_row = width;
    dstInfo.rows_per_layer = height;
    
    SDL_DownloadFromGPUTexture(copyPass, &srcRegion, &dstInfo);
    SDL_EndGPUCopyPass(copyPass);
    
    // Submit and wait using fence
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmdBuf);
    SDL_WaitForGPUFences(m_device, true, &fence, 1);
    SDL_ReleaseGPUFence(m_device, fence);
    
    // Map and copy data
    void* mappedData = SDL_MapGPUTransferBuffer(m_device, transferBuffer, false);
    if (mappedData) {
        memcpy(outData, mappedData, bufferSize);
        SDL_UnmapGPUTransferBuffer(m_device, transferBuffer);
    }
    
    SDL_ReleaseGPUTransferBuffer(m_device, transferBuffer);
    
    return mappedData != nullptr;
}

void SdlGpuRenderer::SetCustomDrawList(ImDrawList* drawList) {
    m_customDrawList = drawList;
}

} // namespace Cartograph

