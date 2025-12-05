#include "Themes.h"
#include "../Color.h"

namespace Cartograph {

// ============================================================================
// Theme registry
// ============================================================================

static const std::string kDefaultTheme = "Dark";

static const std::vector<std::string> kThemeNames = {
    "Dark",
    "Print-Light",
    "Loud-Yellow",
    "Unveil",
    "Aeterna",
    "Hornet",
    "Soma"
};

static const std::unordered_map<std::string, std::string> kThemeDescriptions = {
    {"Dark", "Best for extended editing sessions"},
    {"Print-Light", "Clean theme for export/print preview"},
    {"Loud-Yellow", "Bold industrial aesthetic with high visibility"},
    {"Unveil", "Sleek dark purple with cyan accents"},
    {"Aeterna", "Mystical void with divine golden accents"},
    {"Hornet", "Bone-white elegance with crimson silk accents"},
    {"Soma", "Gothic silver with subtle GBA-era soul blue"}
};

std::vector<std::string> GetAvailableThemes() {
    return kThemeNames;
}

std::string GetThemeDescription(const std::string& name) {
    auto it = kThemeDescriptions.find(name);
    if (it != kThemeDescriptions.end()) {
        return it->second;
    }
    return "";
}

bool IsValidTheme(const std::string& name) {
    for (const auto& t : kThemeNames) {
        if (t == name) return true;
    }
    return false;
}

const std::string& GetDefaultThemeName() {
    return kDefaultTheme;
}

// ============================================================================
// Theme definitions
// ============================================================================

/**
 * Initializes the Dark theme.
 * Neutral dark colors optimized for long editing sessions.
 */
static void InitDarkTheme(Theme& theme) {
    theme.background = Color(0.1f, 0.1f, 0.1f, 1.0f);
    theme.gridLine = Color(0.2f, 0.2f, 0.2f, 1.0f);
    theme.roomOutline = Color(0.8f, 0.8f, 0.8f, 1.0f);
    theme.roomFill = Color(0.15f, 0.15f, 0.15f, 0.8f);
    theme.wallColor = Color(0.0f, 0.0f, 0.0f, 1.0f);
    theme.doorColor = Color(0.4f, 0.4f, 0.4f, 1.0f);
    theme.edgeHoverColor = Color(0.0f, 1.0f, 0.0f, 0.6f);
    theme.markerColor = Color(0.3f, 0.8f, 0.3f, 1.0f);
    theme.textColor = Color(1.0f, 1.0f, 1.0f, 1.0f);
    
    // Selection colors (blue tint)
    theme.selectionFill = Color(0.3f, 0.6f, 1.0f, 0.25f);
    theme.selectionBorder = Color(0.4f, 0.7f, 1.0f, 0.9f);
    
    // Tool preview colors
    theme.tilePreviewBorder = Color(1.0f, 1.0f, 1.0f, 0.8f);
    theme.tilePreviewBrightness = 1.3f;
    
    // Paste preview colors (green tint)
    theme.pastePreviewFill = Color(0.2f, 0.8f, 0.4f, 0.25f);
    theme.pastePreviewBorder = Color(0.3f, 0.9f, 0.5f, 0.9f);
}

/**
 * Initializes the Print-Light theme.
 * Clean light theme for export/print preview with improved contrast.
 */
static void InitPrintLightTheme(Theme& theme) {
    // Off-white background (easier on eyes than pure white)
    theme.background = Color(0.98f, 0.98f, 0.97f, 1.0f);  // #FAFAF8
    
    // Grid lines with better contrast (~30% vs ~15% before)
    theme.gridLine = Color(0.70f, 0.70f, 0.70f, 1.0f);    // #B3B3B3
    
    theme.roomOutline = Color(0.2f, 0.2f, 0.2f, 1.0f);
    
    // Room fill more visible (was nearly invisible at 0.95)
    theme.roomFill = Color(0.88f, 0.88f, 0.88f, 0.8f);    // #E0E0E0
    
    theme.wallColor = Color(0.0f, 0.0f, 0.0f, 1.0f);
    
    // Door color darker for better visibility
    theme.doorColor = Color(0.45f, 0.45f, 0.45f, 1.0f);   // #737373
    
    theme.edgeHoverColor = Color(0.0f, 0.7f, 0.0f, 0.6f);
    theme.markerColor = Color(0.15f, 0.5f, 0.15f, 1.0f);
    theme.textColor = Color(0.0f, 0.0f, 0.0f, 1.0f);
    
    // Selection colors (darker blue for light theme)
    theme.selectionFill = Color(0.2f, 0.4f, 0.8f, 0.2f);
    theme.selectionBorder = Color(0.2f, 0.5f, 0.9f, 0.9f);
    
    // Tool preview colors
    theme.tilePreviewBorder = Color(0.0f, 0.0f, 0.0f, 0.6f);
    theme.tilePreviewBrightness = 1.2f;
    
    // Paste preview colors (darker green for light theme)
    theme.pastePreviewFill = Color(0.1f, 0.6f, 0.3f, 0.2f);
    theme.pastePreviewBorder = Color(0.2f, 0.7f, 0.4f, 0.9f);
}

/**
 * Initializes the Loud-Yellow theme.
 * Bold industrial aesthetic with high-visibility yellow accents.
 */
static void InitLoudYellowTheme(Theme& theme) {
    // Near-black with warm undertone
    theme.background = Color(0.10f, 0.10f, 0.08f, 1.0f);  // #1A1A14
    
    // Muted gold grid lines
    theme.gridLine = Color(0.28f, 0.26f, 0.12f, 1.0f);    // #47421F
    
    // Bright electric yellow outline
    theme.roomOutline = Color(1.0f, 0.92f, 0.0f, 1.0f);   // #FFEB00
    
    // Warm dark gray fill
    theme.roomFill = Color(0.16f, 0.15f, 0.10f, 0.85f);   // #29261A
    
    // Near-black walls
    theme.wallColor = Color(0.05f, 0.05f, 0.03f, 1.0f);   // #0D0D08
    
    // Amber/gold doors
    theme.doorColor = Color(0.75f, 0.60f, 0.15f, 1.0f);   // #BF9926
    
    // Orange-yellow hover
    theme.edgeHoverColor = Color(1.0f, 0.7f, 0.0f, 0.6f); // #FFB300
    
    // Yellow-lime markers
    theme.markerColor = Color(0.9f, 0.95f, 0.3f, 1.0f);   // #E6F24D
    
    // Warm off-white text
    theme.textColor = Color(1.0f, 0.98f, 0.90f, 1.0f);    // #FFFAE6
    
    // Selection colors (orange tint)
    theme.selectionFill = Color(0.9f, 0.6f, 0.1f, 0.25f);
    theme.selectionBorder = Color(1.0f, 0.75f, 0.2f, 0.9f);  // #FFBF33
    
    // Tool preview colors
    theme.tilePreviewBorder = Color(1.0f, 0.95f, 0.6f, 0.8f);
    theme.tilePreviewBrightness = 1.4f;
    
    // Paste preview colors (amber tint)
    theme.pastePreviewFill = Color(0.8f, 0.6f, 0.1f, 0.25f);
    theme.pastePreviewBorder = Color(0.9f, 0.7f, 0.2f, 0.9f);
}

/**
 * Initializes the Unveil theme.
 * Sleek dark purple aesthetic with magenta/cyan accents.
 * Inspired by Unveil Engine branding.
 */
static void InitUnveilTheme(Theme& theme) {
    // Deep purple-black
    theme.background = Color(0.07f, 0.05f, 0.11f, 1.0f);  // #120D1C
    
    // Muted purple grid
    theme.gridLine = Color(0.16f, 0.12f, 0.22f, 1.0f);    // #291F38
    
    // Bright lavender outline
    theme.roomOutline = Color(0.68f, 0.50f, 0.88f, 1.0f); // #AE80E0
    
    // Dark purple fill
    theme.roomFill = Color(0.11f, 0.09f, 0.16f, 0.85f);   // #1C1729
    
    // Near-black violet walls
    theme.wallColor = Color(0.04f, 0.02f, 0.07f, 1.0f);   // #0A0512
    
    // Medium purple doors
    theme.doorColor = Color(0.42f, 0.32f, 0.52f, 1.0f);   // #6B5285
    
    // Magenta/pink hover accent
    theme.edgeHoverColor = Color(0.90f, 0.25f, 0.65f, 0.6f);  // #E640A6
    
    // Cyan accent markers
    theme.markerColor = Color(0.35f, 0.80f, 0.90f, 1.0f); // #59CCE6
    
    // Light lavender text
    theme.textColor = Color(0.90f, 0.86f, 0.96f, 1.0f);   // #E6DBF5
    
    // Selection colors (purple tint)
    theme.selectionFill = Color(0.6f, 0.3f, 0.8f, 0.25f);
    theme.selectionBorder = Color(0.7f, 0.4f, 0.9f, 0.9f);    // #B366E6
    
    // Tool preview colors
    theme.tilePreviewBorder = Color(0.8f, 0.6f, 0.95f, 0.8f);
    theme.tilePreviewBrightness = 1.35f;
    
    // Paste preview colors (cyan tint)
    theme.pastePreviewFill = Color(0.3f, 0.7f, 0.8f, 0.25f);
    theme.pastePreviewBorder = Color(0.4f, 0.8f, 0.9f, 0.9f); // #66CCE6
}

/**
 * Initializes the Aeterna theme.
 * Mystical void aesthetic inspired by Aeterna Noctis.
 * Deep purple-black with electric violet and divine gold accents.
 */
static void InitAeternaTheme(Theme& theme) {
    // Void black with purple undertone
    theme.background = Color(0.04f, 0.03f, 0.06f, 1.0f);      // #0A0810
    
    // Shadow purple grid
    theme.gridLine = Color(0.12f, 0.08f, 0.16f, 1.0f);        // #1E1428
    
    // Electric violet outline
    theme.roomOutline = Color(0.61f, 0.30f, 1.0f, 1.0f);      // #9B4DFF
    
    // Deep void fill
    theme.roomFill = Color(0.07f, 0.06f, 0.10f, 0.85f);       // #12101A
    
    // True black walls
    theme.wallColor = Color(0.02f, 0.01f, 0.03f, 1.0f);       // #050308
    
    // Dusk purple doors
    theme.doorColor = Color(0.35f, 0.24f, 0.48f, 1.0f);       // #5A3D7A
    
    // Divine gold hover
    theme.edgeHoverColor = Color(1.0f, 0.84f, 0.0f, 0.6f);    // #FFD700
    
    // Golden amber markers
    theme.markerColor = Color(1.0f, 0.67f, 0.0f, 1.0f);       // #FFAA00
    
    // Pale violet text
    theme.textColor = Color(0.91f, 0.88f, 0.94f, 1.0f);       // #E8E0F0
    
    // Selection colors (violet glow)
    theme.selectionFill = Color(0.48f, 0.18f, 1.0f, 0.25f);
    theme.selectionBorder = Color(0.48f, 0.18f, 1.0f, 0.9f);  // #7B2FFF
    
    // Tool preview colors
    theme.tilePreviewBorder = Color(0.75f, 0.55f, 1.0f, 0.8f);
    theme.tilePreviewBrightness = 1.4f;
    
    // Paste preview colors (gold tint)
    theme.pastePreviewFill = Color(1.0f, 0.84f, 0.0f, 0.25f);
    theme.pastePreviewBorder = Color(1.0f, 0.84f, 0.0f, 0.9f); // #FFD700
}

/**
 * Initializes the Hornet theme.
 * Elegant aesthetic inspired by Hollow Knight: Silksong.
 * Bone-white on charcoal with crimson silk accents.
 */
static void InitHornetTheme(Theme& theme) {
    // Deep charcoal with warm undertone
    theme.background = Color(0.08f, 0.08f, 0.09f, 1.0f);      // #141416
    
    // Ash gray grid
    theme.gridLine = Color(0.16f, 0.16f, 0.19f, 1.0f);        // #282830
    
    // Bone white outline
    theme.roomOutline = Color(0.94f, 0.93f, 0.91f, 1.0f);     // #F0EDE8
    
    // Shadow gray fill
    theme.roomFill = Color(0.11f, 0.10f, 0.12f, 0.85f);       // #1C1A1E
    
    // Ink black walls
    theme.wallColor = Color(0.03f, 0.03f, 0.03f, 1.0f);       // #080808
    
    // Stone gray doors
    theme.doorColor = Color(0.35f, 0.34f, 0.38f, 1.0f);       // #5A5860
    
    // Crimson silk hover
    theme.edgeHoverColor = Color(0.91f, 0.19f, 0.19f, 0.6f);  // #E83030
    
    // Scarlet markers
    theme.markerColor = Color(1.0f, 0.25f, 0.25f, 1.0f);      // #FF4040
    
    // Shell white text
    theme.textColor = Color(0.97f, 0.96f, 0.94f, 1.0f);       // #F8F4F0
    
    // Selection colors (blood red)
    theme.selectionFill = Color(0.78f, 0.13f, 0.13f, 0.25f);
    theme.selectionBorder = Color(0.78f, 0.13f, 0.13f, 0.9f); // #C82020
    
    // Tool preview colors
    theme.tilePreviewBorder = Color(0.98f, 0.90f, 0.88f, 0.8f);
    theme.tilePreviewBrightness = 1.3f;
    
    // Paste preview colors (pale red)
    theme.pastePreviewFill = Color(1.0f, 0.38f, 0.38f, 0.25f);
    theme.pastePreviewBorder = Color(1.0f, 0.38f, 0.38f, 0.9f); // #FF6060
}

/**
 * Initializes the Soma theme.
 * Gothic silver aesthetic inspired by Castlevania: Aria of Sorrow.
 * GBA-era muted colors with subtle royal blue soul accents.
 */
static void InitSomaTheme(Theme& theme) {
    // Castle stone gray
    theme.background = Color(0.10f, 0.10f, 0.12f, 1.0f);      // #1A1A1E
    
    // Iron gray grid
    theme.gridLine = Color(0.18f, 0.18f, 0.21f, 1.0f);        // #2E2E36
    
    // Moonsilver outline (restrained, not bright white)
    theme.roomOutline = Color(0.70f, 0.72f, 0.76f, 1.0f);     // #B3B8C2
    
    // Shadow stone fill
    theme.roomFill = Color(0.13f, 0.13f, 0.15f, 0.80f);       // #212126
    
    // Obsidian walls
    theme.wallColor = Color(0.05f, 0.05f, 0.07f, 1.0f);       // #0D0D12
    
    // Pewter doors
    theme.doorColor = Color(0.40f, 0.42f, 0.46f, 1.0f);       // #666B75
    
    // Soul blue hover (muted royal blue)
    theme.edgeHoverColor = Color(0.35f, 0.50f, 0.75f, 0.6f);  // #5980BF
    
    // Azure soul markers
    theme.markerColor = Color(0.45f, 0.60f, 0.85f, 1.0f);     // #7399D9
    
    // Platinum text (GBA-style muted white)
    theme.textColor = Color(0.82f, 0.84f, 0.88f, 1.0f);       // #D1D6E0
    
    // Selection colors (steel blue)
    theme.selectionFill = Color(0.30f, 0.40f, 0.60f, 0.25f);
    theme.selectionBorder = Color(0.35f, 0.48f, 0.70f, 0.9f); // #597AB3
    
    // Tool preview colors
    theme.tilePreviewBorder = Color(0.65f, 0.68f, 0.75f, 0.7f);
    theme.tilePreviewBrightness = 1.25f;
    
    // Paste preview colors (frost blue)
    theme.pastePreviewFill = Color(0.40f, 0.55f, 0.75f, 0.25f);
    theme.pastePreviewBorder = Color(0.45f, 0.60f, 0.80f, 0.85f); // #7399CC
}

// ============================================================================
// Public API
// ============================================================================

void InitTheme(Theme& theme, const std::string& name) {
    float savedUiScale = theme.uiScale;  // Preserve UI scale
    theme.name = name;
    theme.mapColors.clear();
    
    if (name == "Dark") {
        InitDarkTheme(theme);
    } else if (name == "Print-Light") {
        InitPrintLightTheme(theme);
    } else if (name == "Loud-Yellow") {
        InitLoudYellowTheme(theme);
    } else if (name == "Unveil") {
        InitUnveilTheme(theme);
    } else if (name == "Aeterna") {
        InitAeternaTheme(theme);
    } else if (name == "Hornet") {
        InitHornetTheme(theme);
    } else if (name == "Soma") {
        InitSomaTheme(theme);
    } else {
        // Fallback to Dark theme for unknown names
        InitDarkTheme(theme);
        theme.name = "Dark";
    }
    
    // Restore UI scale if it was set
    if (savedUiScale > 0.0f) {
        theme.uiScale = savedUiScale;
    } else {
        theme.uiScale = 1.0f;
    }
}

} // namespace Cartograph

