#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct TextureImage {
    uint32_t width{}, height{};
    std::vector<uint8_t> pixels; // RGBA8; the file stores sRGB colour.
};

TextureImage loadTextureImage(const std::string& path);
