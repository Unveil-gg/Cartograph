#pragma once

#include "Color.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace Cartograph {

class PixelBuffer {
public:
    PixelBuffer(int width, int height) 
        : m_width(width), m_height(height) {
        m_pixels.resize(width * height * 4, 0);
    }
    
    // Set pixel with bounds checking (overwrites existing pixel)
    void SetPixel(int x, int y, const Color& color) {
        if (x < 0 || x >= m_width || y < 0 || y >= m_height) return;
        
        int idx = (y * m_width + x) * 4;
        m_pixels[idx + 0] = static_cast<uint8_t>(color.r * 255);
        m_pixels[idx + 1] = static_cast<uint8_t>(color.g * 255);
        m_pixels[idx + 2] = static_cast<uint8_t>(color.b * 255);
        m_pixels[idx + 3] = static_cast<uint8_t>(color.a * 255);
    }
    
    // Blend pixel using standard "over" alpha compositing
    void BlendPixel(int x, int y, const Color& src) {
        if (x < 0 || x >= m_width || y < 0 || y >= m_height) return;
        
        int idx = (y * m_width + x) * 4;
        
        // Get existing pixel (destination)
        float dstR = m_pixels[idx + 0] / 255.0f;
        float dstG = m_pixels[idx + 1] / 255.0f;
        float dstB = m_pixels[idx + 2] / 255.0f;
        float dstA = m_pixels[idx + 3] / 255.0f;
        
        // Standard "over" compositing formula
        float outA = src.a + dstA * (1.0f - src.a);
        
        if (outA > 0.0f) {
            float outR = (src.r * src.a + dstR * dstA * (1.0f - src.a)) / outA;
            float outG = (src.g * src.a + dstG * dstA * (1.0f - src.a)) / outA;
            float outB = (src.b * src.a + dstB * dstA * (1.0f - src.a)) / outA;
            
            m_pixels[idx + 0] = static_cast<uint8_t>(outR * 255);
            m_pixels[idx + 1] = static_cast<uint8_t>(outG * 255);
            m_pixels[idx + 2] = static_cast<uint8_t>(outB * 255);
        }
        m_pixels[idx + 3] = static_cast<uint8_t>(outA * 255);
    }
    
    // Fill rectangle
    void FillRect(int x, int y, int w, int h, const Color& color) {
        for (int py = y; py < y + h; ++py) {
            for (int px = x; px < x + w; ++px) {
                SetPixel(px, py, color);
            }
        }
    }
    
    // Draw horizontal line with thickness
    void DrawHLine(int x1, int x2, int y, const Color& color, int thickness = 1) {
        int halfThick = thickness / 2;
        for (int t = -halfThick; t <= halfThick; ++t) {
            int py = y + t;
            if (py < 0 || py >= m_height) continue;
            
            int startX = std::max(0, std::min(x1, x2));
            int endX = std::min(m_width - 1, std::max(x1, x2));
            for (int x = startX; x <= endX; ++x) {
                SetPixel(x, py, color);
            }
        }
    }
    
    // Draw horizontal line with alpha blending
    void BlendHLine(int x1, int x2, int y, const Color& color) {
        int startX = std::max(0, std::min(x1, x2));
        int endX = std::min(m_width - 1, std::max(x1, x2));
        for (int x = startX; x <= endX; ++x) {
            BlendPixel(x, y, color);
        }
    }
    
    // Draw vertical line with thickness
    void DrawVLine(int x, int y1, int y2, const Color& color, int thickness = 1) {
        int halfThick = thickness / 2;
        for (int t = -halfThick; t <= halfThick; ++t) {
            int px = x + t;
            if (px < 0 || px >= m_width) continue;
            
            int startY = std::max(0, std::min(y1, y2));
            int endY = std::min(m_height - 1, std::max(y1, y2));
            for (int y = startY; y <= endY; ++y) {
                SetPixel(px, y, color);
            }
        }
    }
    
    // Draw vertical line with alpha blending
    void BlendVLine(int x, int y1, int y2, const Color& color) {
        int startY = std::max(0, std::min(y1, y2));
        int endY = std::min(m_height - 1, std::max(y1, y2));
        for (int y = startY; y <= endY; ++y) {
            BlendPixel(x, y, color);
        }
    }
    
    // Blit scaled icon with color tinting
    // iconData is RGBA8 format, destSize is the target size in pixels
    void BlitIconScaled(
        int destX, int destY, int destSize,
        const uint8_t* iconData, int iconW, int iconH,
        const Color& tint
    ) {
        int halfSize = destSize / 2;
        int startX = destX - halfSize;
        int startY = destY - halfSize;
        
        for (int dy = 0; dy < destSize; ++dy) {
            for (int dx = 0; dx < destSize; ++dx) {
                // Sample from source icon using nearest-neighbor
                int srcX = (dx * iconW) / destSize;
                int srcY = (dy * iconH) / destSize;
                
                // Clamp source coordinates
                srcX = std::max(0, std::min(srcX, iconW - 1));
                srcY = std::max(0, std::min(srcY, iconH - 1));
                
                int srcIdx = (srcY * iconW + srcX) * 4;
                
                // Get source pixel
                float srcR = iconData[srcIdx + 0] / 255.0f;
                float srcG = iconData[srcIdx + 1] / 255.0f;
                float srcB = iconData[srcIdx + 2] / 255.0f;
                float srcA = iconData[srcIdx + 3] / 255.0f;
                
                // Apply color tint (multiply blend)
                // If tint alpha is 0, use white (no tint)
                Color finalColor;
                if (tint.a > 0.0f) {
                    finalColor.r = srcR * tint.r;
                    finalColor.g = srcG * tint.g;
                    finalColor.b = srcB * tint.b;
                    finalColor.a = srcA;
                } else {
                    finalColor = Color(srcR, srcG, srcB, srcA);
                }
                
                BlendPixel(startX + dx, startY + dy, finalColor);
            }
        }
    }
    
    const uint8_t* Data() const { return m_pixels.data(); }
    int Width() const { return m_width; }
    int Height() const { return m_height; }
    
private:
    std::vector<uint8_t> m_pixels;
    int m_width, m_height;
};

}  // namespace Cartograph
