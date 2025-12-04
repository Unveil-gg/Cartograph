#include "GpuTexture.h"
#include <cstring>

namespace Cartograph {
namespace GpuTexture {

SDL_GPUTexture* CreateFromPixels(
    SDL_GPUDevice* device,
    const uint8_t* pixels,
    int width,
    int height
) {
    if (!device || !pixels || width <= 0 || height <= 0) {
        return nullptr;
    }
    
    // Create texture
    SDL_GPUTextureCreateInfo texInfo = {};
    texInfo.type = SDL_GPU_TEXTURETYPE_2D;
    texInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texInfo.width = width;
    texInfo.height = height;
    texInfo.layer_count_or_depth = 1;
    texInfo.num_levels = 1;
    texInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
    texInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    
    SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &texInfo);
    if (!texture) {
        return nullptr;
    }
    
    // Create transfer buffer for upload
    size_t dataSize = static_cast<size_t>(width) * height * 4;
    
    SDL_GPUTransferBufferCreateInfo transferInfo = {};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = static_cast<Uint32>(dataSize);
    
    SDL_GPUTransferBuffer* transferBuffer = 
        SDL_CreateGPUTransferBuffer(device, &transferInfo);
    if (!transferBuffer) {
        SDL_ReleaseGPUTexture(device, texture);
        return nullptr;
    }
    
    // Map and copy pixel data
    void* mappedData = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
    if (!mappedData) {
        SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
        SDL_ReleaseGPUTexture(device, texture);
        return nullptr;
    }
    
    memcpy(mappedData, pixels, dataSize);
    SDL_UnmapGPUTransferBuffer(device, transferBuffer);
    
    // Upload to GPU
    SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(device);
    if (!cmdBuf) {
        SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
        SDL_ReleaseGPUTexture(device, texture);
        return nullptr;
    }
    
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);
    
    SDL_GPUTextureTransferInfo srcInfo = {};
    srcInfo.transfer_buffer = transferBuffer;
    srcInfo.offset = 0;
    srcInfo.pixels_per_row = width;
    srcInfo.rows_per_layer = height;
    
    SDL_GPUTextureRegion dstRegion = {};
    dstRegion.texture = texture;
    dstRegion.w = width;
    dstRegion.h = height;
    dstRegion.d = 1;
    
    SDL_UploadToGPUTexture(copyPass, &srcInfo, &dstRegion, false);
    SDL_EndGPUCopyPass(copyPass);
    
    // Submit and wait
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmdBuf);
    SDL_WaitForGPUFences(device, true, &fence, 1);
    SDL_ReleaseGPUFence(device, fence);
    
    // Cleanup transfer buffer
    SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
    
    return texture;
}

} // namespace GpuTexture
} // namespace Cartograph

