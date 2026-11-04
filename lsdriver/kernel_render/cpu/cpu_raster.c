/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/kernel.h>
#include <linux/string.h>

#include "cpu_raster.h"

static const u8 font_digits[10][7] = {
    {0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e}, {0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e}, {0x0e, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1f}, {0x1f, 0x02, 0x04, 0x06, 0x01, 0x11, 0x0e}, {0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02}, {0x1f, 0x10, 0x1e, 0x01, 0x01, 0x11, 0x0e}, {0x06, 0x08, 0x10, 0x1e, 0x11, 0x11, 0x0e}, {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e}, {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x02, 0x0c},
};

static const u8 font_upper[26][7] = {
    {0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}, {0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e}, {0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e}, {0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e}, {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f}, {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10}, {0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0f}, {0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}, {0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e}, {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0c}, {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f}, {0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11},
    {0x11, 0x19, 0x19, 0x15, 0x13, 0x13, 0x11}, {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}, {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10}, {0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d}, {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11}, {0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e}, {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}, {0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04}, {0x11, 0x11, 0x11, 0x15, 0x15, 0x1b, 0x11}, {0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11}, {0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04}, {0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f},
};

static inline u32 *pixel_at(void *pixels, u32 width, u32 height, u32 pitch, int x, int y)
{
    if (x < 0 || y < 0 || x >= width || y >= height) return NULL;
    return (u32 *)((u8 *)pixels + (u64)y * pitch) + x;
}
static inline u32 blend_pixel(u32 dst, u32 src)
{
    u32 alpha = src >> 24;
    u32 inverse = 255 - alpha;
    u32 red = (((src >> 16) & 0xff) * alpha + ((dst >> 16) & 0xff) * inverse) / 255;
    u32 green = (((src >> 8) & 0xff) * alpha + ((dst >> 8) & 0xff) * inverse) / 255;
    u32 blue = ((src & 0xff) * alpha + (dst & 0xff) * inverse) / 255;

    return 0xff000000U | (red << 16) | (green << 8) | blue;
}

static inline void put_pixel(void *pixels, u32 width, u32 height, u32 pitch, int x, int y, u32 color)
{
    u32 *pixel = pixel_at(pixels, width, height, pitch, x, y);

    if (pixel) *pixel = blend_pixel(*pixel, color);
}

void cpu_raster_clear(void *pixels, u32 width, u32 height, u32 pitch, u32 color)
{
    u32 y;

    for (y = 0; y < height; y++)
    {
        u32 *row = (u32 *)((u8 *)pixels + (u64)y * pitch);
        u32 x;

        for (x = 0; x < width; x++) row[x] = color;
        if (pitch > width * sizeof(u32)) memset((u8 *)row + width * sizeof(u32), 0, pitch - width * sizeof(u32));
    }
}
void cpu_raster_line(void *pixels, u32 width, u32 height, u32 pitch, int x0, int y0, int x1, int y1, u32 color, u32 thickness)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    int radius = max_t(u32, thickness, 1) / 2;

    for (;;)
    {
        int y;
        int twice_error;

        for (y = -radius; y <= radius; y++)
        {
            int x;

            for (x = -radius; x <= radius; x++)
                if (x * x + y * y <= radius * radius)
                    put_pixel(pixels, width, height, pitch, x0 + x, y0 + y, color);
        }
        if (x0 == x1 && y0 == y1) break;
        twice_error = 2 * error;
        if (twice_error >= dy)
        {
            error += dy;
            x0 += sx;
        }
        if (twice_error <= dx)
        {
            error += dx;
            y0 += sy;
        }
    }
}
void cpu_raster_rect(void *pixels, u32 width, u32 height, u32 pitch, int left, int top, int right, int bottom, u32 color, u32 thickness, bool filled)
{
    int x;
    int y;

    if (filled)
    {
        for (y = top; y <= bottom; y++)
            for (x = left; x <= right; x++) put_pixel(pixels, width, height, pitch, x, y, color);
        return;
    }
    cpu_raster_line(pixels, width, height, pitch, left, top, right, top, color, thickness);
    cpu_raster_line(pixels, width, height, pitch, right, top, right, bottom, color, thickness);
    cpu_raster_line(pixels, width, height, pitch, right, bottom, left, bottom, color, thickness);
    cpu_raster_line(pixels, width, height, pitch, left, bottom, left, top, color, thickness);
}

void cpu_raster_circle(void *pixels, u32 width, u32 height, u32 pitch, int center_x, int center_y, int radius, u32 color, u32 thickness, bool filled)
{
    int half_thickness;
    int outer;
    int inner;
    int y;

    if (radius <= 0) return;
    half_thickness = max_t(u32, thickness, 1) / 2;
    outer = radius + half_thickness;
    inner = radius - half_thickness;
    for (y = -outer; y <= outer; y++)
    {
        int x;

        for (x = -outer; x <= outer; x++)
        {
            int distance = x * x + y * y;

            if (filled ? distance <= radius * radius : distance <= outer * outer && distance >= inner * inner) put_pixel(pixels, width, height, pitch, center_x + x, center_y + y, color);
        }
    }
}

void cpu_raster_text(void *pixels, u32 width, u32 height, u32 pitch, int left, int top, const char *text, u32 scale, u32 color)
{
    static const u8 question[7] = {0x0e, 0x11, 0x01, 0x06, 0x04, 0x00, 0x04};
    static const u8 colon[7] = {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00};
    static const u8 dash[7] = {0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00};
    static const u8 dot[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06};
    int cursor_x = left;

    scale = max_t(u32, scale, 1);
    while (*text)
    {
        char character = *text++;
        const u8 *glyph;
        int row;

        if (character >= '0' && character <= '9') glyph = font_digits[character - '0'];
        else
        {
            if (character >= 'a' && character <= 'z') character -= 'a' - 'A';
            if (character >= 'A' && character <= 'Z') glyph = font_upper[character - 'A'];
            else if (character == ':') glyph = colon;
            else if (character == '-') glyph = dash;
            else if (character == '.') glyph = dot;
            else glyph = question;
        }

        for (row = 0; row < 7; row++)
        {
            int column;

            for (column = 0; column < 5; column++)
            {
                int sy;

                if (!(glyph[row] & BIT(4 - column))) continue;
                for (sy = 0; sy < scale; sy++)
                {
                    int sx;

                    for (sx = 0; sx < scale; sx++) put_pixel(pixels, width, height, pitch, cursor_x + column * scale + sx, top + row * scale + sy, color);
                }
            }
        }
        cursor_x += 6 * scale;
    }
}

static inline u32 swap_rb_color(u32 color)
{
    return (color & 0xff00ff00U) |
           ((color & 0x00ff0000U) >> 16) |
           ((color & 0x000000ffU) << 16);
}

void cpu_raster_blend_format(void *pixels, u32 width, u32 height, u32 pitch,
                             const void *overlay, u32 overlay_width,
                             u32 overlay_height, u32 overlay_pitch,
                             int left, int top, bool swap_channels)
{
    for (u32 y = 0; y < overlay_height; y++)
    {
        const u32 *source = (const u32 *)((const u8 *)overlay + (u64)y * overlay_pitch);
        int target_y = top + (int)y;

        if (target_y < 0 || target_y >= height) continue;
        for (u32 x = 0; x < overlay_width; x++)
        {
            int target_x = left + (int)x;
            u32 *target;

            if (target_x < 0 || target_x >= width || !(source[x] >> 24)) continue;
            target = pixel_at(pixels, width, height, pitch, target_x, target_y);
            u32 destination = swap_channels ? swap_rb_color(*target) : *target;
            u32 blended = blend_pixel(destination, source[x]);

            *target = swap_channels ? swap_rb_color(blended) : blended;
        }
    }
}
