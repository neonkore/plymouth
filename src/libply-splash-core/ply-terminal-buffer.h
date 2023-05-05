/* ply-terminal-buffer.h - APIs for handling terminal text
 *
 * Copyright (C) 2023 Red Hat, Inc.
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
 * Written By: Ray Strode <rstrode@redhat.com>
 */
#ifndef PLY_TERMINAL_BUFFER_H
#define PLY_TERMINAL_BUFFER_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#include "ply-buffer.h"
#include "ply-event-loop.h"

typedef struct _ply_terminal_buffer ply_terminal_buffer_t;

typedef struct
{
        ply_terminal_buffer_t *buffer;
        ply_list_node_t       *node;
} ply_terminal_buffer_iter_t;

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
ply_terminal_buffer_t *ply_terminal_buffer_new (void) :
        void ply_terminal_buffer_free (ply_terminal_buffer_t *terminal_buffer);

void ply_terminal_buffer_inject (ply_terminal_buffer_t *buffer,
                                 const char            *input,
                                 size_t                 length);
void ply_terminal_buffer_iter_init (ply_terminal_buffer_iter_t *iter,
                                    ply_terminal_buffer_t      *buffer);
int ply_terminal_buffer_iter_next (ply_terminal_buffer_iter_t *iter,
                                   ply_text_attribute_t      **attribute,
                                   const char                **text,
                                   size_t                     *start_index,
                                   size_t                     *end_index);

#endif

#endif /* PLY_TERMINAL_BUFFER_H */
