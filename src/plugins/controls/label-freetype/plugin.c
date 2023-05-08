/* ply-label.c - label control
 *
 * Copyright (C) 2008 Red Hat, Inc.
 * Copyright (c) 2016 SUSE LINUX GmbH, Nuernberg, Germany.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by: Ray Strode <rstrode@redhat.com>
 * Written by: Fabian Vogt <fvogt@suse.com>
 */
#include "config.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "ply-pixel-buffer.h"
#include "ply-pixel-display.h"
#include "ply-utils.h"

#include "ply-label-plugin.h"

/* This is used if fontconfig (fc-match) is not available, like in the initrd. */
#define FONT_FALLBACK "/usr/share/fonts/Plymouth.ttf"

struct _ply_label_plugin_control
{
        ply_pixel_display_t  *display;
        ply_rectangle_t       area;

        ply_label_alignment_t alignment;
        long                  width;  /* For alignment (line wrapping?) */

        FT_Library            library;
        FT_Face               face;

        char                 *text;
        float                 red;
        float                 green;
        float                 blue;
        float                 alpha;

        uint32_t              is_hidden : 1;
};

ply_label_plugin_interface_t *ply_label_plugin_get_interface (void);

/* Query fontconfig, if available, for the default font. */
static const char *
query_fc_match ()
{
        FILE *fp;
        static char fc_match_out[PATH_MAX];

        fp = popen ("/usr/bin/fc-match -f %{file}", "r");
        if (!fp)
                return NULL;

        fgets (fc_match_out, sizeof(fc_match_out), fp);

        pclose (fp);

        return fc_match_out;
}

static ply_label_plugin_control_t *
create_control (void)
{
        FT_Error error;
        ply_label_plugin_control_t *label;
        const char *font_path;

        label = calloc (1, sizeof(ply_label_plugin_control_t));

        label->is_hidden = true;
        label->width = -1;
        label->text = NULL;

        error = FT_Init_FreeType (&label->library);
        if (error) {
                free (label);
                return NULL;
        }

        font_path = query_fc_match ();
        if (font_path)
                error = FT_New_Face (label->library, font_path, 0, &label->face);

        if (!font_path || error) {
                printf ("label-ft: trying font fallback\n");
                font_path = FONT_FALLBACK;
                error = FT_New_Face (label->library, font_path, 0, &label->face);

                if (error) {
                        FT_Done_FreeType (label->library);
                        free (label);
                        return NULL;
                }
        }

        /* 12pt/96dpi as default */
        error = FT_Set_Char_Size (label->face, 12 << 6, 0, 96, 0);
        if (error) {
                FT_Done_Face (label->face);
                FT_Done_FreeType (label->library);
                free (label);
                return NULL;
        }

        return label;
}

static void
destroy_control (ply_label_plugin_control_t *label)
{
        if (label == NULL)
                return;

        free (label->text);
        FT_Done_Face (label->face);
        FT_Done_FreeType (label->library);

        free (label);
}

static long
get_width_of_control (ply_label_plugin_control_t *label)
{
        return label->area.width;
}

static long
get_height_of_control (ply_label_plugin_control_t *label)
{
        return label->area.height;
}

static FT_Int
width_of_line (ply_label_plugin_control_t *label,
               const char                 *text)
{
        FT_Error error;
        FT_Int width = 0;
        FT_Int left_bearing = 0;

        while (*text != '\0' && *text != '\n') {
                error = FT_Load_Char (label->face, *text, FT_LOAD_RENDER | FT_LOAD_TARGET_LIGHT);

                if (!error) {
                        width += label->face->glyph->advance.x >> 6;
                        left_bearing = label->face->glyph->bitmap_left;
                        /* We don't "go back" when drawing, so when left bearing is
                         * negative (like for 'j'), we simply add to the width. */
                        if (left_bearing < 0)
                                width += -left_bearing;

                        ++text;
                }
        }

        return width;
}

static void
size_control (ply_label_plugin_control_t *label)
{
        FT_Int width;
        const char *text = label->text;

        label->area.width = 0;
        label->area.height = 0;

        /* Go through each line */
        while (text && *text) {
                width = width_of_line (label, text);
                if ((uint32_t) width > label->area.width)
                        label->area.width = width;

                label->area.height += (label->face->size->metrics.ascender
                                       - label->face->size->metrics.descender) >> 6;

                text = strchr (text, '\n');
                /* skip newline character */
                if (text)
                        ++text;
        }

        /* If centered, area.x is not the origin anymore */
        if ((long) label->area.width < label->width)
                label->area.width = label->width;
}

static void
trigger_redraw (ply_label_plugin_control_t *label,
                bool                        adjust_size)
{
        ply_rectangle_t dirty_area = label->area;

        if (label->is_hidden || label->display == NULL)
                return;

        if (adjust_size)
                size_control (label);

        ply_pixel_display_draw_area (label->display,
                                     dirty_area.x, dirty_area.y,
                                     dirty_area.width, dirty_area.height);
}

static void
draw_bitmap (ply_label_plugin_control_t *label,
             uint32_t                   *target,
             ply_rectangle_t             target_size,
             FT_Bitmap                  *source,
             FT_Int                      x_start,
             FT_Int                      y_start)
{
        FT_Int x, y, xs, ys;
        FT_Int x_end = MIN (x_start + source->width, target_size.width);
        FT_Int y_end = MIN (y_start + source->rows, target_size.height);

        if ((uint32_t) x_start >= target_size.width ||
            (uint32_t) y_start >= target_size.height)
                return;

        uint8_t rs, gs, bs, rd, gd, bd, ad;
        rs = 255 * label->red;
        gs = 255 * label->green;
        bs = 255 * label->blue;

        for (y = y_start, ys = 0; y < y_end; ++y, ++ys) {
                for (x = x_start, xs = 0; x < x_end; ++x, ++xs) {
                        float alpha = label->alpha *
                                      (source->buffer[xs + source->pitch * ys] / 255.0f);
                        float invalpha = 1.0f - alpha;
                        uint32_t dest = target[x + target_size.width * y];

                        /* Separate colors */
                        rd = dest >> 16;
                        gd = dest >> 8;
                        bd = dest;

                        /* Alpha blending */
                        rd = invalpha * rd + alpha * rs;
                        gd = invalpha * gd + alpha * gs;
                        bd = invalpha * bd + alpha * bs;
                        /* Semi-correct: Disregard the target alpha */
                        ad = alpha * 255;

                        target[x + target_size.width * y] =
                                (ad << 24) | (rd << 16) | (gd << 8) | bd;
                }
        }
}

static void
draw_control (ply_label_plugin_control_t *label,
              ply_pixel_buffer_t         *pixel_buffer,
              long                        x,
              long                        y,
              unsigned long               width,
              unsigned long               height)
{
        FT_Error error;
        FT_Vector pen;
        FT_GlyphSlot slot;
        const char *cur_c;
        uint32_t *target;
        ply_rectangle_t target_size;

        if (label->is_hidden)
                return;

        /* Check for overlap.
         * TODO: Don't redraw everything if only a part should be drawn! */
        if (label->area.x > x + (long) width || label->area.y > y + (long) height
            || label->area.x + (long) label->area.width < x
            || label->area.y + (long) label->area.height < y)
                return;

        slot = label->face->glyph;

        cur_c = label->text;

        target = ply_pixel_buffer_get_argb32_data (pixel_buffer);
        ply_pixel_buffer_get_size (pixel_buffer, &target_size);

        if (target_size.height == 0)
                return; /* This happens sometimes. */

        /* 64ths of a pixel */
        pen.y = label->area.y << 6;

        /* Make sure that the first row fits */
        pen.y += label->face->size->metrics.ascender;

        /* Go through each line */
        while (*cur_c) {
                pen.x = label->area.x << 6;

                /* Start at start position (alignment) */
                if (label->alignment == PLY_LABEL_ALIGN_CENTER)
                        pen.x += (label->area.width - width_of_line (label, cur_c)) << 5;
                else if (label->alignment == PLY_LABEL_ALIGN_RIGHT)
                        pen.x += (label->area.width - width_of_line (label, cur_c)) << 6;

                while (*cur_c && *cur_c != '\n') {
                        FT_Int extraAdvance = 0, positiveBearingX = 0;
                        /* TODO: Unicode support. */
                        error = FT_Load_Char (label->face, *cur_c,
                                              FT_LOAD_RENDER | FT_LOAD_TARGET_LIGHT);
                        if (error)
                                continue;

                        /* We consider negative left bearing an increment in size,
                         * as we draw full character boxes and don't "go back" in
                         * this plugin. Positive left bearing is treated as usual.
                         * For definitions see
                         * https://freetype.org/freetype2/docs/glyphs/glyphs-3.html
                         */
                        if (slot->bitmap_left < 0) {
                                extraAdvance = -slot->bitmap_left;
                        } else {
                                positiveBearingX = slot->bitmap_left;
                        }
                        draw_bitmap (label, target, target_size, &slot->bitmap,
                                     (pen.x >> 6) + positiveBearingX,
                                     (pen.y >> 6) - slot->bitmap_top);

                        pen.x += slot->advance.x + extraAdvance;
                        pen.y += slot->advance.y;

                        ++cur_c;
                }
                /* skip newline character */
                if (*cur_c)
                        ++cur_c;

                /* Next line */
                pen.y += label->face->size->metrics.height;
        }
}

static void
set_alignment_for_control (ply_label_plugin_control_t *label,
                           ply_label_alignment_t       alignment)
{
        if (label->alignment != alignment) {
                label->alignment = alignment;
                trigger_redraw (label, true);
        }
}

static void
set_width_for_control (ply_label_plugin_control_t *label,
                       long                        width)
{
        if (label->width != width) {
                label->width = width;
                trigger_redraw (label, true);
        }
}

static void
set_text_for_control (ply_label_plugin_control_t *label,
                      const char                 *text)
{
        if (label->text != text) {
                free (label->text);
                label->text = strdup (text);
                size_control (label);
                trigger_redraw (label, true);
        }
}

static void
set_font_for_control (ply_label_plugin_control_t *label,
                      const char                 *fontdesc)
{
        /* Only able to set size */

        char *size_str_after;
        const char *size_str;
        unsigned long size;
        bool size_in_pixels;

        size = 25; /* Default, if not set. */
        size_in_pixels = false;

        /* Format is "Family 1[,Family 2[,..]] [25[px]]" .
         * [] means optional. */
        size_str = strrchr (fontdesc, ' ');

        if (size_str) {
                size = strtoul (size_str, &size_str_after, 10);
                if (size_str_after == size_str)
                        size = 25; /* Not a number */
                else if (strcmp (size_str_after, "px") == 0)
                        size_in_pixels = true;
        }

        if (size_in_pixels)
                FT_Set_Pixel_Sizes (label->face, 0, size);
        else
                FT_Set_Char_Size (label->face, size << 6, 0, 72, 0);

        /* Ignore errors, to keep the current size. */

        trigger_redraw (label, true);
}

static void
set_color_for_control (ply_label_plugin_control_t *label,
                       float                       red,
                       float                       green,
                       float                       blue,
                       float                       alpha)
{
        label->red = red;
        label->green = green;
        label->blue = blue;
        label->alpha = alpha;

        trigger_redraw (label, false);
}

static bool
show_control (ply_label_plugin_control_t *label,
              ply_pixel_display_t        *display,
              long                        x,
              long                        y)
{
        ply_rectangle_t dirty_area;

        dirty_area = label->area;
        label->display = display;
        label->area.x = x;
        label->area.y = y;

        label->is_hidden = false;

        size_control (label);

        if (!label->is_hidden && label->display != NULL)
                ply_pixel_display_draw_area (label->display,
                                             dirty_area.x, dirty_area.y,
                                             dirty_area.width, dirty_area.height);

        label->is_hidden = false;

        return true;
}

static void
hide_control (ply_label_plugin_control_t *label)
{
        label->is_hidden = true;
        if (label->display != NULL)
                ply_pixel_display_draw_area (label->display,
                                             label->area.x, label->area.y,
                                             label->area.width, label->area.height);

        label->display = NULL;
}

static bool
is_control_hidden (ply_label_plugin_control_t *label)
{
        return label->is_hidden;
}

ply_label_plugin_interface_t *
ply_label_plugin_get_interface (void)
{
        static ply_label_plugin_interface_t plugin_interface =
        {
                .create_control            = create_control,
                .destroy_control           = destroy_control,
                .show_control              = show_control,
                .hide_control              = hide_control,
                .draw_control              = draw_control,
                .is_control_hidden         = is_control_hidden,
                .set_text_for_control      = set_text_for_control,
                .set_alignment_for_control = set_alignment_for_control,
                .set_width_for_control     = set_width_for_control,
                .set_font_for_control      = set_font_for_control,
                .set_color_for_control     = set_color_for_control,
                .get_width_of_control      = get_width_of_control,
                .get_height_of_control     = get_height_of_control
        };

        return &plugin_interface;
}

/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s, (0: */
