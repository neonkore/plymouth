/* ply-terminal-emulator.h - Minimal Terminal Emulator
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
#ifndef PLY_TERMINAL_EMULATOR_H
#define PLY_TERMINAL_EMULATOR_H

#include "ply-boot-splash.h"
#include <sys/syslog.h>

typedef struct _ply_terminal_emulator ply_terminal_emulator_t;

#ifndef PLY_HIDE_FUNCTION_DECLARATIONS
ply_terminal_emulator_t *ply_terminal_emulator_new ();
ply_list_t *ply_terminal_emulator_get_console_lines (ply_terminal_emulator_t *terminal_emulator);
void ply_terminal_emulator_parse_lines (ply_terminal_emulator_t *terminal_emulator,
                                        char                    *text);
void ply_terminal_emulator_convert_boot_buffer (ply_terminal_emulator_t *terminal_emulator,
                                                ply_buffer_t            *boot_buffer);
#endif //PLY_HIDE_FUNCTION_DECLARATIONS
#endif //PLY_TERMINAL_EMULATOR_H
