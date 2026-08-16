#pragma once
#include "sdl.hpp"

#include <string_view>
// This file contains basic graphics functions various parts of SaveNX use.

namespace gfxutil
{
    /// @brief Renders a filled circle using the active SDL renderer.
    bool render_circle_fill(sdl::SharedTexture &target, int centerX, int centerY, int radius, sdl::Color color) noexcept;

    /// @brief Renders a filled rounded rectangle using the active SDL renderer.
    bool render_rounded_rect_fill(sdl::SharedTexture &target,
                                  int x,
                                  int y,
                                  int width,
                                  int height,
                                  int radius,
                                  sdl::Color color) noexcept;

    /// @brief Generates a generic icon for saves and titles that lack one.
    /// @param text Text to be centered and rendered to the icon.
    /// @param fontSize Size of the font to use.
    /// @param background Background color to use.
    /// @param foreground Color to use to render the text.
    /// @return sdl::SharedTexture of the icon.
    sdl::SharedTexture create_generic_icon(std::string_view text, int fontSize, sdl::Color background, sdl::Color foreground);
} // namespace gfxutil
