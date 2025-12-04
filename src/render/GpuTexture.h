#pragma once

#include <SDL3/SDL_gpu.h>
#include <imgui.h>
#include <cstdint>

namespace Cartograph {

/**
 * GPU texture utilities for SDL_GPU.
 * Provides helpers for creating and uploading textures for ImGui.
 */
namespace GpuTexture {

/**
 * Create and upload a texture from RGBA pixel data.
 * @param device SDL_GPU device
 * @param pixels RGBA8 pixel data
 * @param width Texture width
 * @param height Texture height
 * @return SDL_GPUTexture* on success, nullptr on failure
 *         Caller owns the returned texture.
 */
SDL_GPUTexture* CreateFromPixels(
    SDL_GPUDevice* device,
    const uint8_t* pixels,
    int width,
    int height
);

/**
 * Convert SDL_GPUTexture* to ImTextureID for ImGui rendering.
 * @param texture SDL_GPU texture pointer
 * @return ImTextureID suitable for ImGui::Image()
 */
inline ImTextureID ToImTextureID(SDL_GPUTexture* texture) {
    return reinterpret_cast<ImTextureID>(texture);
}

/**
 * Convert ImTextureID back to SDL_GPUTexture*.
 * @param texId ImTextureID from ImGui
 * @return SDL_GPUTexture* pointer
 */
inline SDL_GPUTexture* FromImTextureID(ImTextureID texId) {
    return reinterpret_cast<SDL_GPUTexture*>(texId);
}

/**
 * Release a GPU texture.
 * @param device SDL_GPU device
 * @param texture Texture to release (can be nullptr)
 */
inline void Release(SDL_GPUDevice* device, SDL_GPUTexture* texture) {
    if (texture && device) {
        SDL_ReleaseGPUTexture(device, texture);
    }
}

/**
 * Release a GPU texture from ImTextureID.
 * @param device SDL_GPU device
 * @param texId ImTextureID to release
 */
inline void ReleaseFromImTextureID(SDL_GPUDevice* device, ImTextureID texId) {
    Release(device, FromImTextureID(texId));
}

} // namespace GpuTexture
} // namespace Cartograph

