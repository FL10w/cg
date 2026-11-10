#include "texture_image.hpp"
#include <memory>
#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

TextureImage loadTextureImage(const std::string& path) {
    int width=0, height=0, channels=0;
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
        stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha), stbi_image_free);
    if(!pixels) {
        const char* reason=stbi_failure_reason();
        throw std::runtime_error("Cannot load texture '"+path+"': "+(reason ? reason : "unknown error"));
    }
    TextureImage image;
    image.width=static_cast<uint32_t>(width);
    image.height=static_cast<uint32_t>(height);
    const size_t bytes=static_cast<size_t>(width)*static_cast<size_t>(height)*4;
    image.pixels.assign(pixels.get(),pixels.get()+bytes);
    return image;
}
