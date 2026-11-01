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

static void draw_disc(void *pixels, u32 width, u32 height, u32 pitch, int center_x, int center_y, int radius, u32 color)
{
    int y;

    for (y = -radius; y <= radius; y++)
    {
        int x;

        for (x = -radius; x <= radius; x++)
            if (x * x + y * y <= radius * radius) put_pixel(pixels, width, height, pitch, center_x + x, center_y + y, color);
    }
}

static const u8 *glyph_for(char character)
{
    static const u8 question[7] = {0x0e, 0x11, 0x01, 0x06, 0x04, 0x00, 0x04};
    static const u8 colon[7] = {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00};
    static const u8 dash[7] = {0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00};
    static const u8 dot[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06};

    if (character >= '0' && character <= '9') return font_digits[character - '0'];
    if (character >= 'a' && character <= 'z') character -= 'a' - 'A';
    if (character >= 'A' && character <= 'Z') return font_upper[character - 'A'];
    if (character == ':') return colon;
    if (character == '-') return dash;
    if (character == '.') return dot;
    return question;
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
        draw_disc(pixels, width, height, pitch, x0, y0, radius, color);
        if (x0 == x1 && y0 == y1) break;
        if (2 * error >= dy)
        {
            error += dy;
            x0 += sx;
        }
        if (2 * error <= dx)
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
    int outer = radius + max_t(u32, thickness, 1) / 2;
    int inner = radius - max_t(u32, thickness, 1) / 2;
    int y;

    if (radius <= 0) return;
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
    int cursor_x = left;

    scale = max_t(u32, scale, 1);
    while (*text)
    {
        const u8 *glyph = glyph_for(*text++);
        int row;

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

void cpu_raster_demo(void *pixels, u32 width, u32 height, u32 pitch)
{
    cpu_raster_clear(pixels, width, height, pitch, 0xff101820U);
    cpu_raster_rect(pixels, width, height, pitch, 20, 20, width - 21, height - 21, 0xff30d5c8U, 4, false);
    cpu_raster_line(pixels, width, height, pitch, 40, 100, width - 40, height - 100, 0xffff453aU, 6);
    cpu_raster_rect(pixels, width, height, pitch, 70, 150, width - 70, height - 150, 0xffffd60aU, 4, false);
    cpu_raster_circle(pixels, width, height, pitch, width / 2, height / 2, min(width, height) / 5, 0xff64d2ffU, 6, false);
    cpu_raster_text(pixels, width, height, pitch, 48, height - 80, "LSDRIVER", 4, 0xffffffffU);
}