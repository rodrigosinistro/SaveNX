#include "graphics/gfxutil.hpp"

#include <algorithm>
#include <cmath>

namespace
{
    /// @brief Width of generic icons in pixels.
    constexpr int SIZE_ICON_WIDTH = 256;

    /// @brief Height of generic icons in pixels.
    constexpr int SIZE_ICON_HEIGHT = 256;
} // namespace

bool gfxutil::render_circle_fill(sdl::SharedTexture &target,
                                 int centerX,
                                 int centerY,
                                 int radius,
                                 sdl::Color color) noexcept
{
    if (radius <= 0 || !sdl::set_render_target(target) || !sdl::set_render_draw_color(color)) { return false; }

    SDL_Renderer *renderer = sdl::get_renderer();
    for (int offsetY = -radius; offsetY <= radius; ++offsetY)
    {
        const int halfWidth = static_cast<int>(std::sqrt((radius * radius) - (offsetY * offsetY)));
        if (SDL_RenderDrawLine(renderer,
                               centerX - halfWidth,
                               centerY + offsetY,
                               centerX + halfWidth,
                               centerY + offsetY) != 0)
        {
            return false;
        }
    }
    return true;
}

bool gfxutil::render_rounded_rect_fill(sdl::SharedTexture &target,
                                       int x,
                                       int y,
                                       int width,
                                       int height,
                                       int radius,
                                       sdl::Color color) noexcept
{
    if (width <= 0 || height <= 0 || !sdl::set_render_target(target) || !sdl::set_render_draw_color(color)) { return false; }

    const int safeRadius = std::clamp(radius, 0, std::min(width, height) / 2);
    SDL_Renderer *renderer = sdl::get_renderer();
    if (safeRadius == 0)
    {
        const SDL_Rect rect{.x = x, .y = y, .w = width, .h = height};
        return SDL_RenderFillRect(renderer, &rect) == 0;
    }

    const SDL_Rect middle{.x = x, .y = y + safeRadius, .w = width, .h = height - (safeRadius * 2)};
    const SDL_Rect center{.x = x + safeRadius, .y = y, .w = width - (safeRadius * 2), .h = height};
    if (SDL_RenderFillRect(renderer, &middle) != 0 || SDL_RenderFillRect(renderer, &center) != 0) { return false; }

    const int cornerRadiusSquared = safeRadius * safeRadius;
    for (int offsetY = 0; offsetY < safeRadius; ++offsetY)
    {
        const int distanceY = safeRadius - offsetY;
        const int halfWidth = static_cast<int>(std::sqrt(cornerRadiusSquared - (distanceY * distanceY)));
        const int lineX = x + safeRadius - halfWidth;
        const int lineWidth = width - ((safeRadius - halfWidth) * 2);
        const SDL_Rect topLine{.x = lineX, .y = y + offsetY, .w = lineWidth, .h = 1};
        const SDL_Rect bottomLine{.x = lineX, .y = y + height - offsetY - 1, .w = lineWidth, .h = 1};
        if (SDL_RenderFillRect(renderer, &topLine) != 0 || SDL_RenderFillRect(renderer, &bottomLine) != 0) { return false; }
    }
    return true;
}

sdl::SharedTexture gfxutil::create_generic_icon(std::string_view text,
                                                int fontSize,
                                                sdl::Color background,
                                                sdl::Color foreground)
{
    // Create base icon texture.
    sdl::SharedTexture icon = sdl::TextureManager::load(text, SIZE_ICON_WIDTH, SIZE_ICON_HEIGHT, SDL_TEXTUREACCESS_TARGET);

    // Get the centered X and Y coordinates.
    const int textX = (SIZE_ICON_WIDTH / 2) - (sdl::text::get_width(fontSize, text) / 2);
    const int textY = (SIZE_ICON_HEIGHT / 2) - (fontSize / 2);

    icon->clear(background);
    sdl::text::render(icon, textX, textY, fontSize, sdl::text::NO_WRAP, foreground, text);

    return icon;
}
