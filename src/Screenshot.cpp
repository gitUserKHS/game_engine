#include "engine/Editor.hpp"

#define STBIW_WINDOWS_UTF8
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <algorithm>
#include <fstream>

namespace engine {

bool writePngRgba(
    const std::filesystem::path& path,
    int width,
    int height,
    std::span<const unsigned char> pixels,
    bool originBottomLeft,
    std::string* error
) {
    if (width <= 0 || height <= 0 ||
        pixels.size() != static_cast<std::size_t>(width * height * 4)) {
        if (error != nullptr) {
            *error = "RGBA pixel count does not match the image size.";
        }
        return false;
    }

    std::error_code directoryError;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(
            path.parent_path(),
            directoryError
        );
    }
    if (directoryError) {
        if (error != nullptr) {
            *error = "Could not create screenshot directory.";
        }
        return false;
    }

    std::vector<unsigned char> output(pixels.begin(), pixels.end());
    if (originBottomLeft) {
        const std::size_t rowBytes = static_cast<std::size_t>(width) * 4;
        for (int row = 0; row < height / 2; ++row) {
            auto first = output.begin() + row * rowBytes;
            auto last = output.begin() + (height - 1 - row) * rowBytes;
            std::swap_ranges(first, first + rowBytes, last);
        }
    }

    const std::u8string utf8Path = path.u8string();
    const std::string narrowPath{
        reinterpret_cast<const char*>(utf8Path.data()),
        utf8Path.size(),
    };
    const int written = stbi_write_png(
        narrowPath.c_str(),
        width,
        height,
        4,
        output.data(),
        width * 4
    );
    if (written == 0 && error != nullptr) {
        *error = "stb_image_write could not write the PNG file.";
    }
    return written != 0;
}

} // namespace engine
