/* ply-console-view.c - console message viewer
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
 */

#include <stdlib.h>
#include <assert.h>

#include "ply-console-viewer.h"
#include "ply-label.h"
#include "ply-list.h"
#include "ply-pixel-display.h"
#include "ply-image.h"
#include "ply-kmsg-reader.h"

#define CONSOLE_POLLS_PER_SECOND 4

struct _ply_console_viewer
{
        ply_event_loop_t    *loop;

        ply_list_t          *console_messages;

        ply_pixel_display_t *display;
        ply_rectangle_t      area;

        ply_list_t          *message_labels;

        int                  is_hidden;

        char                *fontdesc;
        long                 font_height;
        long                 font_width;
        int                  line_max_chars;
        int                  messages_updating;

        uint32_t             text_color;
};

ply_console_viewer_t *
ply_console_viewer_new (ply_pixel_display_t *display,
                        const char          *fontdesc)
{
        ply_console_viewer_t *console_viewer;

        console_viewer = calloc (1, sizeof(struct _ply_console_viewer));

        console_viewer->messages_updating = false;
        console_viewer->message_labels = ply_list_new ();

        console_viewer->fontdesc = strdup (fontdesc);

        ply_label_t *console_message_label, *measure_label;
        int label_count;

        measure_label = ply_label_new ();
        ply_label_set_text (measure_label, " ");
        ply_label_set_font (measure_label, console_viewer->fontdesc);

        console_viewer->text_color = PLY_CONSOLE_VIEWER_LOG_TEXT_COLOR;

        console_viewer->font_height = ply_label_get_height (measure_label);
        console_viewer->font_width = ply_label_get_width (measure_label);
        /* Allow the label to be the size of how many characters can fit in the width of the screeen, minus one for larger fonts that have some size overhead */
        console_viewer->line_max_chars = ply_pixel_display_get_width (display) / console_viewer->font_width - 1;
        label_count = ply_pixel_display_get_height (display) / console_viewer->font_height;
        ply_label_free (measure_label);

        for (int label_index = 0; label_index < label_count; label_index++) {
                console_message_label = ply_label_new ();
                ply_label_set_font (console_message_label, console_viewer->fontdesc);
                ply_list_append_data (console_viewer->message_labels, console_message_label);
        }

        return console_viewer;
}

void
ply_console_viewer_free (ply_console_viewer_t *console_viewer)
{
        ply_list_node_t *node;
        ply_label_t *console_message_label;

        for (int i = 0; i < ply_list_get_length (console_viewer->message_labels); i++) {
                node = ply_list_get_nth_node (console_viewer->message_labels, i);
                console_message_label = ply_list_node_get_data (node);
                ply_label_free (console_message_label);
        }
        ply_list_free (console_viewer->message_labels);


        free (console_viewer->fontdesc);
        free (console_viewer);
}

void
ply_console_viewer_show (ply_console_viewer_t *console_viewer,
                         ply_pixel_display_t  *display)
{
        assert (console_viewer != NULL);

        console_viewer->display = display;
        console_viewer->is_hidden = false;

        ply_list_node_t *node;
        ply_label_t *console_message_label;
        int label_count, message_number, label_index, string_length_left, utf8_string_offset, utf8_string_range;
        char *console_message;
        uint32_t label_color;

        if (console_viewer->console_messages == NULL)
                return;

        label_index = 0;
        label_count = ply_list_get_length (console_viewer->message_labels) - 1;
        message_number = ply_list_get_length (console_viewer->console_messages) - 1;
        label_color = console_viewer->text_color;
        while (label_index <= label_count) {
                if (message_number >= 0) {
                        console_message = ply_list_node_get_data (ply_list_get_nth_node (console_viewer->console_messages, message_number));
                } else {
                        console_message = "";
                }

                string_length_left = ply_utf8_string_get_length (console_message, strlen (console_message));
                utf8_string_offset = string_length_left;
                while (string_length_left >= 0) {
                        node = ply_list_get_nth_node (console_viewer->message_labels, label_index);
                        console_message_label = ply_list_node_get_data (node);

                        utf8_string_range = utf8_string_offset % console_viewer->line_max_chars;
                        if (utf8_string_range == 0)
                                utf8_string_range = console_viewer->line_max_chars;

                        utf8_string_offset -= utf8_string_range;
                        string_length_left = utf8_string_offset - 1;

                        /* prevent underrun */
                        if (utf8_string_offset < 0)
                                utf8_string_offset = 0;

                        ply_label_set_text (console_message_label,
                                            ply_utf8_string_get_substring (console_message, utf8_string_offset, utf8_string_range));
                        ply_label_show (console_message_label, console_viewer->display, console_viewer->font_width / 2,
                                        (ply_pixel_display_get_height (console_viewer->display) - (console_viewer->font_height * label_index) - console_viewer->font_height));
                        ply_label_set_hex_color (console_message_label, label_color);

                        label_index++;
                        if (label_index > label_count)
                                break;

                        utf8_string_offset--;
                }
                message_number--;
                if (message_number < 0)
                        break;
        }
}

void
ply_console_viewer_draw_area (ply_console_viewer_t *console_viewer,
                              ply_pixel_buffer_t   *buffer,
                              long                  x,
                              long                  y,
                              unsigned long         width,
                              unsigned long         height)
{
        ply_list_node_t *node;
        ply_label_t *console_message_label;

        if (console_viewer->messages_updating == false) {
                console_viewer->messages_updating = true;

                for (int label_index = 0; label_index < ply_list_get_length (console_viewer->message_labels); label_index++) {
                        node = ply_list_get_nth_node (console_viewer->message_labels, label_index);
                        console_message_label = ply_list_node_get_data (node);
                        ply_label_draw_area (console_message_label, buffer, x, y, width, height);
                }
                console_viewer->messages_updating = false;
        }
}

void
ply_console_viewer_hide (ply_console_viewer_t *console_viewer)
{
        int label_count = ply_list_get_length (console_viewer->message_labels);
        ply_list_node_t *node;
        ply_label_t *console_message_label;

        if (console_viewer->is_hidden == true)
                return;

        console_viewer->is_hidden = true;

        console_viewer->messages_updating = true;

        for (int label_index = 0; label_index < label_count; label_index++) {
                node = ply_list_get_nth_node (console_viewer->message_labels, label_index); console_message_label = ply_list_node_get_data (node);
                ply_label_hide (console_message_label);
        }
        console_viewer->messages_updating = false;

        console_viewer->display = NULL;
}

void
ply_console_viewer_attach_console_messages (ply_console_viewer_t *console_viewer,
                                            ply_list_t           *console_messages)
{
        console_viewer->console_messages = console_messages;
}

void
ply_console_viewer_set_text_color (ply_console_viewer_t *console_viewer,
                                       uint32_t              hex_color)
{
        console_viewer->text_color = hex_color;
}
