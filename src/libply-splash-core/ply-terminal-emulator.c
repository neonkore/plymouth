/* ply-terminal-emulator.c - Minimal Terminal Emulator
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

#include "ply-list.h"
#include "ply-terminal-emulator.h"
#include "ply-logger.h"
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

struct _ply_terminal_emulator
{
        ply_list_t *console_messages;
        int         line_count;
        int         cursor_row;
        int         cursor_column;
};

ply_terminal_emulator_t *
ply_terminal_emulator_new ()
{
        ply_terminal_emulator_t *terminal_emulator;

        terminal_emulator = calloc (1, sizeof(struct _ply_terminal_emulator));

        terminal_emulator->console_messages = ply_list_new ();
        terminal_emulator->line_count = 0;

        return terminal_emulator;
}

ply_list_t *
ply_terminal_emulator_get_console_lines (ply_terminal_emulator_t *terminal_emulator)
{
        return terminal_emulator->console_messages;
}

void
ply_terminal_emulator_parse_line (ply_terminal_emulator_t *terminal_emulator,
                                  char                    *text)
{
        int node_number;
        ply_list_node_t *node;

        terminal_emulator->cursor_row = terminal_emulator->line_count - 1;
        node_number = terminal_emulator->cursor_row;
        if (node_number < 0)
                node_number = 0;

        node = ply_list_get_nth_node (terminal_emulator->console_messages, node_number);
        ply_list_node_set_data (node, text);
}

void
ply_terminal_emulator_parse_lines (ply_terminal_emulator_t *terminal_emulator,
                                   char                    *text)
{
        char *text_substr, *saveptr;

        text_substr = strtok_r (text, "\n", &saveptr);
        while (text_substr != NULL) {
                ply_list_append_data (terminal_emulator->console_messages, NULL);
                terminal_emulator->line_count++;
                ply_terminal_emulator_parse_line (terminal_emulator, text_substr);
                text_substr = strtok_r (NULL, "\n", &saveptr);
        }
}

void
ply_terminal_emulator_convert_boot_buffer (ply_terminal_emulator_t *terminal_emulator,
                                           ply_buffer_t            *boot_buffer)
{
        char *text = strndup (ply_buffer_get_bytes (boot_buffer), ply_buffer_get_size (boot_buffer));

        ply_terminal_emulator_parse_lines (terminal_emulator, text);
}
