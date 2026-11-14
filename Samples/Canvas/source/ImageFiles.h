#pragma once

#include "Document.h"
#include <FL/Fl_PNG_Image.H>
#include <chrono>
#include <filesystem>
#include <string>

namespace canvas
{
inline Image readPng(const std::filesystem::path &path)
{
    Fl_RGB_Image::max_size(2048u * 2048u * 4u);
    Fl_PNG_Image png(path.string().c_str());
    if (png.fail() || !png.data() || !png.data()[0] || png.d() < 1 || png.d() > 4)
        throw std::runtime_error("Could not read PNG image: " + path.string());
    auto image = Image::blank(png.data_w(), png.data_h());
    const int channels = png.d();
    const int stride = png.ld() ? png.ld() : image.width * channels;
    const auto *data = reinterpret_cast<const unsigned char *>(png.data()[0]);
    for (int y = 0; y < image.height; ++y)
    {
        for (int x = 0; x < image.width; ++x)
        {
            const auto *p = data + y * stride + x * channels;
            const int alpha = channels == 2 || channels == 4 ? p[channels - 1] : 255;
            const int red = p[0] * alpha / 255;
            const int green = p[channels >= 3 ? 1 : 0] * alpha / 255;
            const int blue = p[channels >= 3 ? 2 : 0] * alpha / 255;
            image.pixels[y * image.width + x] = red * 65536 + green * 256 + blue;
        }
    }
    return image;
}

inline void writePng(const std::filesystem::path &path, const Image &image)
{
    std::vector<unsigned char> rgb(image.pixels.size() * 3);
    for (std::size_t i = 0; i < image.pixels.size(); ++i)
    {
        rgb[i * 3] = static_cast<unsigned char>((image.pixels[i] >> 16) & 255);
        rgb[i * 3 + 1] = static_cast<unsigned char>((image.pixels[i] >> 8) & 255);
        rgb[i * 3 + 2] = static_cast<unsigned char>(image.pixels[i] & 255);
    }
    // Write beside the destination so a failed encode never truncates a user's
    // existing image, and the final rename stays on the same filesystem.
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto temporary = std::filesystem::path(path.string() + ".canvas-" + std::to_string(stamp) + ".tmp");
    try
    {
        if (fl_write_png(temporary.string().c_str(), rgb.data(), image.width, image.height, 3) != 0)
            throw std::runtime_error("Could not encode PNG image: " + path.string());
        std::filesystem::rename(temporary, path);
    }
    catch (...)
    {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}
} // namespace canvas
