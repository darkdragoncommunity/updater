#pragma once

#include <string>

namespace TextureUtils {
    // Loads an image file (JPG/PNG) into an OpenGL texture
    // Returns the texture ID (0 on failure)
    // Out width and height are optional
    unsigned int LoadTextureFromFile(const std::string& filename, int* out_width = nullptr, int* out_height = nullptr);
}
