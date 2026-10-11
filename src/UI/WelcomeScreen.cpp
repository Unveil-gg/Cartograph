#include "WelcomeScreen.h"
#include "../UI.h"
#include "../App.h"
#include "../Model.h"
#include "../Canvas.h"
#include "../History.h"
#include "../Jobs.h"
#include "../Icons.h"
#include "../Keymap.h"
#include "../Package.h"
#include "../ProjectFolder.h"
#include "../Preferences.h"
#include "../platform/Paths.h"
#include "../platform/Fs.h"
#include "../platform/Time.h"
#include <imgui.h>
#include <filesystem>
#include <algorithm>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <unordered_set>
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

// minizip-ng for reading .cart thumbnails
#include "unzip.h"

// GLAD - cross-platform OpenGL loader
#include <glad/gl.h>

// STB image for loading thumbnails (implementation in Icons.cpp)
#include "../../third_party/stb/stb_image.h"

namespace Cartograph {


WelcomeScreen::WelcomeScreen(UI& ui) : m_ui(ui) {
    // Initialize welcome screen
}


WelcomeScreen::~WelcomeScreen() {
    UnloadThumbnailTextures();
}



} // namespace Cartograph
