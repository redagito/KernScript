#pragma once

#include <filesystem>
#include <memory>
#include <stdexcept>

#include <FL/Fl_Image_Surface.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/fl_draw.H>

namespace samples
{
inline void previewDamage(Fl_Widget &widget)
{
    widget.clear_damage(FL_DAMAGE_ALL);
    if (auto *group = widget.as_group())
        for (int i = 0; i < group->children(); ++i)
            previewDamage(*group->child(i));
}
// Render only this widget hierarchy into its own offscreen drawing surface.
// No screen, monitor, desktop or other application's pixels are read.
inline void renderPreview(Fl_Window &window, const std::filesystem::path &path)
{
    // Set only the widget flag, without creating or showing a native window.
    // FLTK skips hidden widget hierarchies even on an image surface.
    const bool wasVisible = window.visible();
    window.set_visible();
    struct RestoreVisibility
    {
        Fl_Window &window;
        bool wasVisible;
        ~RestoreVisibility()
        {
            if (!wasVisible)
                window.clear_visible();
        }
    } visibility{window, wasVisible};
    Fl_Image_Surface surface(window.w(), window.h());
    Fl_Surface_Device::push_current(&surface);
    struct Restore
    {
        ~Restore() { Fl_Surface_Device::pop_current(); }
    } restore;
    fl_color(window.color());
    fl_rectf(0, 0, window.w(), window.h());
    previewDamage(window);
    surface.draw(&window);
    std::unique_ptr<Fl_RGB_Image> image(surface.image());
    if (!image || fl_write_png(path.string().c_str(), image.get()) != 0)
        throw std::runtime_error("Could not write application preview: " + path.string());
}
} // namespace samples
