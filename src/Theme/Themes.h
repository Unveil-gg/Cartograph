#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "../Color.h"

namespace Cartograph {

// ============================================================================
// Theme structure
// ============================================================================

/**
 * Defines the visual appearance of the application.
 * Contains colors for canvas elements and UI components.
 */
struct Theme {
    std::string name;
    float uiScale = 1.0f;
    std::unordered_map<std::string, Color> mapColors;  // Override defaults
    
    // Canvas colors
    Color background;
    Color gridLine;
    Color roomOutline;
    Color roomFill;
    Color wallColor;       // Solid wall lines
    Color doorColor;       // Door (dashed) lines
    Color edgeHoverColor;  // Edge hover highlight
    Color markerColor;
    Color textColor;
    
    // Selection tool colors
    Color selectionFill;       // Selection rectangle fill
    Color selectionBorder;     // Selection rectangle border
    
    // Tool preview colors
    Color tilePreviewBorder;   // Paint tool cursor outline
    float tilePreviewBrightness = 1.3f;  // Brightness boost for preview
    
    // Paste preview colors
    Color pastePreviewFill;    // Paste ghost fill
    Color pastePreviewBorder;  // Paste ghost border
};

// ============================================================================
// Theme registry
// ============================================================================

/**
 * Returns list of all available theme names.
 * @return Vector of theme name strings.
 */
std::vector<std::string> GetAvailableThemes();

/**
 * Returns a brief description for a theme.
 * @param name Theme name.
 * @return Description string for UI display.
 */
std::string GetThemeDescription(const std::string& name);

/**
 * Initializes theme with predefined colors by name.
 * Preserves uiScale if already set.
 * @param theme Theme struct to initialize.
 * @param name Theme name (Dark, Print-Light, Loud-Yellow, Unveil).
 */
void InitTheme(Theme& theme, const std::string& name);

/**
 * Checks if a theme name is valid.
 * @param name Theme name to check.
 * @return True if theme exists.
 */
bool IsValidTheme(const std::string& name);

/**
 * Returns the default theme name.
 * @return "Dark"
 */
const std::string& GetDefaultThemeName();

} // namespace Cartograph

