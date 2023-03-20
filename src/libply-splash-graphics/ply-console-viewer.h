/* ply-console-viewer.h - console message viewer
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
#ifndef PLY_CONSOLE_VIEWER_H
#define PLY_CONSOLE_VIEWER_H

#include "ply-key-file.h"
#include "ply-list.h"
#include "ply-event-loop.h"
#include "ply-pixel-display.h"

typedef struct _ply_console_viewer ply_console_viewer_t;

#define PLY_CONSOLE_VIEWER_LOG_TEXT_COLOR 0xffffffff   /* white */

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
ply_console_viewer_t *ply_console_viewer_new (ply_pixel_display_t *display,
                                              const char          *fontdesc);
void ply_console_viewer_free (ply_console_viewer_t *console_viewer);
void ply_console_viewer_show (ply_console_viewer_t *console_viewer,
                              ply_pixel_display_t  *display);
void ply_console_viewer_draw_area (ply_console_viewer_t *console_viewer,
                                   ply_pixel_buffer_t   *buffer,
                                   long                  x,
                                   long                  y,
                                   unsigned long         width,
                                   unsigned long         height);
void ply_console_viewer_hide (ply_console_viewer_t *console_viewer);
void ply_console_viewer_attach_console_messages (ply_console_viewer_t *console_viewer,
                                                 ply_list_t           *console_messages);
void ply_console_viewer_set_text_color (ply_console_viewer_t *console_viewer,
                                        uint32_t              hex_color);
#endif //PLY_HIDE_FUNCTION_DECLARATIONS

#endif //PLY_CONSOLE_VIEWER_H
