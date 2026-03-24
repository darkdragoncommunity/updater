#include "texture_utils.h"
#include "utils.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <GLFW/glfw3.h>
#include <iostream>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace TextureUtils {
    unsigned int LoadTextureFromFile(const std::string& filename, int* out_width, int* out_height) {
        Utils::Log("[INFO] loading texture: " + filename);
        // Load from file
        int image_width = 0;
        int image_height = 0;
        unsigned char* image_data = stbi_load(filename.c_str(), &image_width, &image_height, NULL, 4);
        if (image_data == NULL) {
            Utils::Log("[ERROR] failed to load texture: " + filename);
            std::cerr << "Failed to load image: " << filename << std::endl;
            return 0;
        }

        // Create a OpenGL texture identifier
        GLuint image_texture;
        glGenTextures(1, &image_texture);
        glBindTexture(GL_TEXTURE_2D, image_texture);

        // Setup filtering parameters for display
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same

        // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
        stbi_image_free(image_data);

        if (out_width) *out_width = image_width;
        if (out_height) *out_height = image_height;

        Utils::Log("[INFO] loaded texture: " + filename + " size=" + std::to_string(image_width) + "x" + std::to_string(image_height));

        return (unsigned int)image_texture;
    }
}
