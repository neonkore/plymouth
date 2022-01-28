/* plugin.c - boot splash plugin
 *
 * Copyright (C) 2022 Red Hat, Inc.
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
 */
#include "config.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <values.h>
#include <unistd.h>

#include "ply-boot-splash-plugin.h"
#include "ply-buffer.h"
#include "ply-entry.h"
#include "ply-event-loop.h"
#include "ply-key-file.h"
#include "ply-label.h"
#include "ply-list.h"
#include "ply-logger.h"
#include "ply-image.h"
#include "ply-pixel-buffer.h"
#include "ply-pixel-display.h"
#include "ply-trigger.h"
#include "ply-utils.h"

typedef enum
{
        PLY_BOOT_SPLASH_DISPLAY_NORMAL,
        PLY_BOOT_SPLASH_DISPLAY_QUESTION_ENTRY,
        PLY_BOOT_SPLASH_DISPLAY_PASSWORD_ENTRY
} ply_boot_splash_display_type_t;

struct _ply_boot_splash_plugin
{
        ply_event_loop_t              *loop;
        char                         **command;
        pid_t                          command_pid;
        int                            command_pid_fd;
        ply_fd_watch_t                *command_pid_fd_watch;
        ply_boot_splash_mode_t         mode;
        ply_boot_splash_display_type_t state;

        uint32_t                       is_visible : 1;
};

ply_boot_splash_plugin_interface_t *ply_boot_splash_plugin_get_interface (void);
static void detach_from_event_loop (ply_boot_splash_plugin_t *plugin);

static int
pidfd_open (pid_t        pid,
            unsigned int flags)
{
        return syscall (__NR_pidfd_open, pid, flags);
}

static int
pidfd_send_signal(int           pid_fd,
                  int           signal_number,
                  siginfo_t    *info,
                  unsigned int  flags)
{
        return syscall (__NR_pidfd_send_signal, pid_fd, signal_number, info, flags);
}

static void
on_child_cloned (ply_boot_splash_plugin_t *plugin)
{
        prctl (PR_SET_PDEATHSIG, SIGTERM);
        execve (plugin->command[0],  plugin->command, NULL);
}

static void
on_command_finished (ply_boot_splash_plugin_t *plugin)
{
        int result;
        int status;

        result = waitpid (plugin->command_pid, &status, WNOHANG);

        if (result != 0)
                return;

        if (WIFEXITED (status))
                ply_trace ("command exited with status: %d", (int) WEXITSTATUS (status));
        else if (WIFSIGNALED (status))
                ply_trace ("command killed with signal: %d", (int) WTERMSIG (status));

        plugin->command_pid = 0;

        ply_event_loop_stop_watching_fd (plugin->loop, plugin->command_pid_fd_watch);
        plugin->command_pid_fd_watch = NULL;

        close (plugin->command_pid_fd);
        plugin->command_pid_fd = -1;
}

static void
start_command (ply_boot_splash_plugin_t *plugin)
{
        int fork_result, pidfd_open_result;

        fork_result = fork ();

        if (fork_result == 0) {
                on_child_cloned (plugin);
                _exit (1);
        }

        if (fork_result < 0) {
                ply_trace ("Could not clone child: %m");
                return;
        }

        plugin->command_pid = fork_result;

        pidfd_open_result = pidfd_open (plugin->command_pid, 0);

        if (pidfd_open_result < 0) {
                ply_trace ("Could not monitor cloned child: %m");
                return;
        }

        plugin->command_pid_fd = pidfd_open_result;

        plugin->command_pid_fd_watch =  ply_event_loop_watch_fd (plugin->loop,
                                                                 plugin->command_pid_fd,
                                                                 PLY_EVENT_LOOP_FD_STATUS_HAS_DATA,
                                                                 (ply_event_handler_t)
                                                                 on_command_finished,
                                                                 NULL,
                                                                 plugin);

}

static void
stop_command (ply_boot_splash_plugin_t *plugin)
{
        int status;

        if (plugin->command_pid_fd < 0)
                return;

        pidfd_send_signal (plugin->command_pid_fd, SIGTERM, NULL, 0);
        waitpid (plugin->command_pid, &status, 0);

        plugin->command_pid_fd  = -1;
        plugin->command_pid = 0;

        ply_event_loop_stop_watching_fd (plugin->loop, plugin->command_pid_fd_watch);
        plugin->command_pid_fd_watch = NULL;
}

static ply_boot_splash_plugin_t *
create_plugin (ply_key_file_t *key_file)
{
        ply_boot_splash_plugin_t *plugin;

        plugin = calloc (1, sizeof (ply_boot_splash_plugin_t));

        plugin->command = calloc (2, sizeof (char *));
        plugin->command[0] = ply_key_file_get_value (key_file, "external-command", "Command");

        plugin->state = PLY_BOOT_SPLASH_DISPLAY_NORMAL;

        return plugin;
}

static void
destroy_plugin (ply_boot_splash_plugin_t *plugin)
{
        if (plugin == NULL)
                return;

        free (plugin->command[0]);
        free (plugin->command);

        stop_command (plugin);

        if (plugin->loop != NULL) {
                ply_event_loop_stop_watching_for_exit (plugin->loop, (ply_event_loop_exit_handler_t)
                                                       detach_from_event_loop,
                                                       plugin);
                detach_from_event_loop (plugin);
        }

        free (plugin);
}

static void
detach_from_event_loop (ply_boot_splash_plugin_t *plugin)
{
        plugin->loop = NULL;
}

static void
add_pixel_display (ply_boot_splash_plugin_t *plugin,
                   ply_pixel_display_t      *display)
{
}

static void
remove_pixel_display (ply_boot_splash_plugin_t *plugin,
                      ply_pixel_display_t      *display)
{
}

static bool
show_splash_screen (ply_boot_splash_plugin_t *plugin,
                    ply_event_loop_t         *loop,
                    ply_buffer_t             *boot_buffer,
                    ply_boot_splash_mode_t    mode)
{
        assert (plugin != NULL);

        plugin->loop = loop;
        plugin->mode = mode;

        ply_event_loop_watch_for_exit (loop, (ply_event_loop_exit_handler_t)
                                       detach_from_event_loop,
                                       plugin);

        start_command (plugin);

        plugin->is_visible = true;

        return true;
}

static void
update_status (ply_boot_splash_plugin_t *plugin,
               const char               *status)
{
        assert (plugin != NULL);
}

static void
on_boot_progress (ply_boot_splash_plugin_t *plugin,
                  double                    duration,
                  double                    fraction_done)
{
}

static void
hide_splash_screen (ply_boot_splash_plugin_t *plugin,
                    ply_event_loop_t         *loop)
{
        assert (plugin != NULL);

        if (plugin->loop != NULL) {
                ply_event_loop_stop_watching_for_exit (plugin->loop, (ply_event_loop_exit_handler_t)
                                                       detach_from_event_loop,
                                                       plugin);
                detach_from_event_loop (plugin);
        }

        stop_command (plugin);

        plugin->is_visible = false;
}

static void
on_root_mounted (ply_boot_splash_plugin_t *plugin)
{
}

static void
become_idle (ply_boot_splash_plugin_t *plugin,
             ply_trigger_t            *idle_trigger)
{
        ply_trigger_pull (idle_trigger, NULL);
}

static void
display_normal (ply_boot_splash_plugin_t *plugin)
{
        plugin->state = PLY_BOOT_SPLASH_DISPLAY_NORMAL;
}

static void
display_password (ply_boot_splash_plugin_t *plugin,
                  const char               *prompt,
                  int                       bullets)
{
        plugin->state = PLY_BOOT_SPLASH_DISPLAY_PASSWORD_ENTRY;
}

static void
display_question (ply_boot_splash_plugin_t *plugin,
                  const char               *prompt,
                  const char               *entry_text)
{
        plugin->state = PLY_BOOT_SPLASH_DISPLAY_QUESTION_ENTRY;
}

static void
display_message (ply_boot_splash_plugin_t *plugin,
                 const char               *message)
{
}

ply_boot_splash_plugin_interface_t *
ply_boot_splash_plugin_get_interface (void)
{
        static ply_boot_splash_plugin_interface_t plugin_interface =
        {
                .create_plugin        = create_plugin,
                .destroy_plugin       = destroy_plugin,
                .add_pixel_display    = add_pixel_display,
                .remove_pixel_display = remove_pixel_display,
                .show_splash_screen   = show_splash_screen,
                .update_status        = update_status,
                .on_boot_progress     = on_boot_progress,
                .hide_splash_screen   = hide_splash_screen,
                .on_root_mounted      = on_root_mounted,
                .become_idle          = become_idle,
                .display_normal       = display_normal,
                .display_password     = display_password,
                .display_question     = display_question,
                .display_message      = display_message,
        };

        return &plugin_interface;
}

/* vim: set ts=4 sw=4 expandtab autoindent cindent cino={.5s,(0: */
