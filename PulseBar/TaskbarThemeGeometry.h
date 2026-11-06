#pragma once

#include <algorithm>

namespace TaskbarThemeGeometry
{
    struct AccentBar
    {
        int left{};
        int top{};
        int width{};
        int height{};
    };

    // Requested radius is in device pixels. A capsule ignores the requested value.
    inline int ResolveCornerRadius(int width, int height, int requested_radius, bool capsule) noexcept
    {
        const int max_radius = (std::max)(0, (std::min)(width, height) / 2);
        if (max_radius == 0)
            return 0;
        if (capsule)
            return max_radius;
        return (std::max)(0, (std::min)(requested_radius, max_radius));
    }

    // The setting is defined as 1..100 percent; keep invalid runtime values bounded too.
    inline int PanelAlphaFromPercent(int opacity_percent) noexcept
    {
        const int percent = (std::max)(1, (std::min)(opacity_percent, 100));
        return percent * 255 / 100;
    }

    inline int AccentGradientEndAlpha(int panel_alpha) noexcept
    {
        return (std::max)(0, (std::min)(panel_alpha, 255)) * 120 / 255;
    }

    // Return a bottom bar wholly inside the panel, or false when no positive bar fits.
    inline bool MakeBottomAccentBar(
        int width,
        int height,
        int radius,
        int bar_height,
        int bottom_margin,
        AccentBar& out) noexcept
    {
        if (width <= 0 || height <= 0 || bar_height <= 0 || bottom_margin < 0)
            return false;

        const int safe_radius = (std::max)(0, (std::min)(radius, (std::min)(width, height) / 2));
        const int left = safe_radius;
        const int right = width - safe_radius;
        const int bottom = (std::min)(height, (std::max)(0, height - bottom_margin));
        const int top = (std::max)(0, bottom - bar_height);
        if (right <= left || bottom <= top)
            return false;

        out = { left, top, right - left, bottom - top };
        return true;
    }

    // Return a left bar clipped to the panel bounds, or false when no positive bar fits.
    inline bool MakeLeftAccentBar(
        int width,
        int height,
        int inset_x,
        int inset_y,
        int requested_bar_width,
        AccentBar& out) noexcept
    {
        if (width <= 0 || height <= 0 || requested_bar_width <= 0 || inset_x < 0 || inset_y < 0)
            return false;

        const int left = (std::min)(inset_x, width);
        const int top = (std::min)(inset_y, height);
        const int bottom = (std::max)(top, height - inset_y);
        const long long desired_right = static_cast<long long>(left) + requested_bar_width;
        const long long clipped_right = desired_right < width ? desired_right : width;
        const int right = static_cast<int>(clipped_right);
        if (right <= left || bottom <= top)
            return false;

        out = { left, top, right - left, bottom - top };
        return true;
    }
}
