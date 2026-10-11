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



unsigned int WelcomeScreen::GeneratePlaceholderTexture() {
    // Generate a gradient/grid pattern for missing thumbnails
    const int width = 384;
    const int height = 216;
    const int gridSize = 16;  // Grid cell size in pixels
    
    std::vector<uint8_t> pixels(width * height * 4);
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = (y * width + x) * 4;
            
            // Create a subtle grid pattern
            bool onGridLine = (x % gridSize == 0) || (y % gridSize == 0);
            
            // Gradient from dark gray to darker gray
            float gradientFactor = 1.0f - (y / static_cast<float>(height)) * 0.3f;
            uint8_t baseColor = static_cast<uint8_t>(40 * gradientFactor);
            uint8_t gridColor = static_cast<uint8_t>(60 * gradientFactor);
            
            if (onGridLine) {
                pixels[idx + 0] = gridColor;      // R
                pixels[idx + 1] = gridColor;      // G
                pixels[idx + 2] = gridColor;      // B
            } else {
                pixels[idx + 0] = baseColor;      // R
                pixels[idx + 1] = baseColor;      // G
                pixels[idx + 2] = baseColor;      // B
            }
            pixels[idx + 3] = 255;                // A
        }
    }
    
    // Create OpenGL texture
    GLuint texId;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Upload pixel data
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels.data()
    );
    
    // Unbind
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return texId;
}



} // namespace Cartograph
