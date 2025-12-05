#pragma once

#include <string>
#include <cstdint>

// Forward declarations
struct ImVec4;

namespace Cartograph {

// ============================================================================
// Color utilities
// ============================================================================

/**
 * RGBA color representation with float components (0.0-1.0).
 * Supports hex string parsing and conversion.
 */
struct Color {
    float r, g, b, a;
    
    Color() : r(0), g(0), b(0), a(1) {}
    Color(float r, float g, float b, float a = 1.0f) 
        : r(r), g(g), b(b), a(a) {}
    
    // Parse from hex string "#RRGGBB" or "#RRGGBBAA"
    static Color FromHex(const std::string& hex);
    
    // Convert to hex string
    std::string ToHex(bool includeAlpha = true) const;
    
    // Convert to ImVec4
    ImVec4 ToImVec4() const;
    
    uint32_t ToU32() const;
};

} // namespace Cartograph

