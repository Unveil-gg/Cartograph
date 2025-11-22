#include "GlRenderer.h"
#include "../Model.h"
#include <imgui.h>
#include <imgui_impl_opengl3.h>

// OpenGL loader - using SDL's built-in function pointers
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

namespace Cartograph {

struct FboHandle {
    GLuint fbo;
    GLuint colorTexture;
    GLuint depthRenderbuffer;
    int width, height;
};

GlRenderer::GlRenderer(SDL_Window* window)
    : m_window(window)
    , m_currentFbo(nullptr)
{
    // OpenGL context is created by App, just store reference
    m_context = SDL_GL_GetCurrentContext();
}

GlRenderer::~GlRenderer() {
    // Context cleanup handled by App
}

void GlRenderer::BeginFrame() {
    // Nothing special needed - ImGui handles most state
}

void GlRenderer::EndFrame() {
    // Render happens via ImGui
}

void GlRenderer::SetRenderTarget(void* fbo) {
    m_currentFbo = fbo;
    
    if (fbo) {
        FboHandle* handle = static_cast<FboHandle*>(fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, handle->fbo);
        glViewport(0, 0, handle->width, handle->height);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        int w, h;
        SDL_GetWindowSizeInPixels(m_window, &w, &h);
        glViewport(0, 0, w, h);
    }
}

void GlRenderer::SetScissor(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) {
        glDisable(GL_SCISSOR_TEST);
    } else {
        glEnable(GL_SCISSOR_TEST);
        glScissor(x, y, w, h);
    }
}

void GlRenderer::Clear(const Color& color) {
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void GlRenderer::DrawRect(
    float x, float y, float w, float h, 
    const Color& color
) {
    // Use foreground draw list to draw on top of canvas background
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddRectFilled(
        ImVec2(x, y),
        ImVec2(x + w, y + h),
        color.ToU32()
    );
}

void GlRenderer::DrawRectOutline(
    float x, float y, float w, float h, 
    const Color& color, float thickness
) {
    // Use foreground draw list to draw on top of canvas background
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddRect(
        ImVec2(x, y),
        ImVec2(x + w, y + h),
        color.ToU32(),
        0.0f,  // No rounding
        0,     // No corner flags
        thickness
    );
}

void GlRenderer::DrawLine(
    float x1, float y1, float x2, float y2,
    const Color& color, float thickness
) {
    // Use foreground draw list to draw on top of canvas background
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddLine(
        ImVec2(x1, y1),
        ImVec2(x2, y2),
        color.ToU32(),
        thickness
    );
}

void GlRenderer::GetDrawableSize(int* outW, int* outH) const {
    SDL_GetWindowSizeInPixels(m_window, outW, outH);
}

void GlRenderer::GetWindowSize(int* outW, int* outH) const {
    SDL_GetWindowSize(m_window, outW, outH);
}

void* GlRenderer::CreateFramebuffer(int width, int height) {
    FboHandle* handle = new FboHandle();
    handle->width = width;
    handle->height = height;
    
    // Create FBO
    glGenFramebuffers(1, &handle->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, handle->fbo);
    
    // Create color texture
    glGenTextures(1, &handle->colorTexture);
    glBindTexture(GL_TEXTURE_2D, handle->colorTexture);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA8,
        width, height, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, nullptr
    );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, handle->colorTexture, 0
    );
    
    // Create depth renderbuffer (optional, but good practice)
    glGenRenderbuffers(1, &handle->depthRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, handle->depthRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
        GL_RENDERBUFFER, handle->depthRenderbuffer
    );
    
    // Check status
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        // Error handling
        DestroyFramebuffer(handle);
        return nullptr;
    }
    
    // Restore default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    return handle;
}

void GlRenderer::DestroyFramebuffer(void* fbo) {
    if (!fbo) return;
    
    FboHandle* handle = static_cast<FboHandle*>(fbo);
    
    if (handle->fbo) {
        glDeleteFramebuffers(1, &handle->fbo);
    }
    if (handle->colorTexture) {
        glDeleteTextures(1, &handle->colorTexture);
    }
    if (handle->depthRenderbuffer) {
        glDeleteRenderbuffers(1, &handle->depthRenderbuffer);
    }
    
    delete handle;
}

void GlRenderer::ReadPixels(
    int x, int y, int width, int height, 
    uint8_t* outData
) {
    glReadPixels(
        x, y, width, height,
        GL_RGBA, GL_UNSIGNED_BYTE,
        outData
    );
}

} // namespace Cartograph

