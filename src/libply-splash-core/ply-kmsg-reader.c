/* ply-kmsg-reader.c - kernel log message reader
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
#include "ply-kmsg-reader.h"
#include "ply-event-loop.h"
#include "ply-logger.h"
#include "ply-utils.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>
#include <ctype.h>

#ifndef LOG_LINE_MAX
#define LOG_LINE_MAX 8192
#endif

#define from_hex(c)             (isdigit (c) ? c - '0' : tolower (c) - 'a' + 10)

size_t
unhexmangle_to_buffer (const char *s,
                       char       *buf,
                       size_t      len)
{
        size_t sz = 0;
        const char *buf0 = buf;

        if (!s)
                return 0;

        while (*s && sz < len - 1) {
                if (*s == '\\' && sz + 3 < len - 1 && s[1] == 'x' &&
                    isxdigit (s[2]) && isxdigit (s[3])) {
                        *buf++ = from_hex (s[2]) << 4 | from_hex (s[3]);
                        s += 4;
                        sz += 4;
                } else {
                        *buf++ = *s++;
                        sz++;
                }
        }
        *buf = '\0';
        return buf - buf0 + 1;
}

int
handle_kmsg_message (ply_kmsg_reader_t *kmsg_reader,
                     int                fd)
{
        ssize_t bytes_read;
        char read_buffer[LOG_LINE_MAX] = "";

        bytes_read = read (fd, read_buffer, sizeof(read_buffer) - 1);
        if (bytes_read > 0) {
                char *fields, *field_prefix, *field_sequence, *field_timestamp, *message, *message_substr, *msgptr, *saveptr;
                int prefix, priority, facility;
                uint64_t sequence;
                unsigned long long timestamp;
                kmsg_message_t *kmsg_message;

                fields = strtok_r (read_buffer, ";", &message);

                /* Messages end in \n, any following lines are machine readable. Actual multiline messages are expanded with unhexmangle_to_buffer*/
                msgptr = strchr (message, '\n');
                if (*msgptr && *msgptr != '\n')
                        msgptr--;

                unhexmangle_to_buffer (message, (char *) message, msgptr - message + 1);

                field_prefix = strtok_r (fields, ",", &fields);
                field_sequence = strtok_r (fields, ",", &fields);
                field_timestamp = strtok_r (fields, ",", &fields);

                prefix = atoi (field_prefix);
                sequence = strtoull (field_sequence, NULL, 0);
                timestamp = strtoull (field_timestamp, NULL, 0);

                if (prefix > 0) {
                        priority = prefix & 7;
                        facility = prefix >> 3;
                } else {
                        priority = LOG_INFO;
                        facility = LOG_USER;
                }

                message_substr = strtok_r (message, "\n", &saveptr);
                while (message_substr != NULL) {
                        kmsg_message = calloc (1, sizeof(kmsg_message_t));

                        kmsg_message->priority = priority;
                        kmsg_message->facility = facility;
                        kmsg_message->sequence = sequence;
                        kmsg_message->timestamp = timestamp;
                        kmsg_message->message = strndup (message_substr, strlen (message_substr));

                        ply_trigger_pull (kmsg_reader->kmsg_trigger, kmsg_message);

                        message_substr = strtok_r (NULL, "\n", &saveptr);
                }

                return 0;
        } else {
                ply_event_loop_stop_watching_fd (ply_event_loop_get_default (), kmsg_reader->fd_watch);
                close (kmsg_reader->kmsg_fd);
                return -1;
        }
}

void
ply_kmsg_reader_start (ply_kmsg_reader_t *kmsg_reader)
{
        kmsg_reader->fd_watch = ply_event_loop_watch_fd (ply_event_loop_get_default (), kmsg_reader->kmsg_fd, PLY_EVENT_LOOP_FD_STATUS_HAS_DATA,
                                                         (ply_event_handler_t) handle_kmsg_message,
                                                         NULL,
                                                         kmsg_reader);
}
