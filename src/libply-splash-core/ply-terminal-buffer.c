#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define PLY_TERMINAL_COLOR_ATTRIBUTE_OFFSET 30

typedef struct
{
        ply_terminal_color_t color;
        size_t               start_index;
        size_t               span_length;
} ply_text_attribute_t;

struct _ply_terminal_buffer
{
        ply_buffer_t *text_buffer;
        ply_list_t   *attributes;
};

typedef enum
{
        PLY_TERMINAL_BUFFER_PARSER_STATE_UNESCAPED,
        PLY_TERMINAL_BUFFER_PARSER_STATE_ESCAPED,
        PLY_TERMINAL_BUFFER_PARSER_STATE_CONTROL_SEQUENCE,
        PLY_TERMINAL_BUFFER_PARSER_STATE_CONTROL_SEQUENCE_PARAMETER
} ply_terminal_buffer_parser_state_t;


ply_terminal_buffer_t *
ply_terminal_buffer_new (void)
{
        ply_terminal_buffer_t *terminal_buffer = calloc (1, sizeof(ply_terminal_buffer_t));

        terminal_buffer->text_buffer = ply_buffer_new ();

        return buffer;
}

void
ply_terminal_buffer_free (ply_terminal_buffer_t *terminal_buffer)
{
        ply_list_node_t *node;

        ply_buffer_free (terminal_buffer->text_buffer);
        ply_list_foreach (terminal_buffer->attributes, node) {
                ply_text_attribute_t *attribute;
                attribute = ply_list_node_get_data (node);
                free (attribute);
        }
        ply_list_free (terminal_buffer->attributes);
        free (terminal_buffer);
}

void
ply_terminal_buffer_inject (ply_terminal_buffer_t *buffer,
                            const char            *input,
                            size_t                 length)
{
        ply_terminal_buffer_parser_state_t state = PLY_TERMINAL_BUFFER_PARSER_STATE_UNESCAPED;
        ply_terminal_color_t color = PLY_TERMINAL_COLOR_WHITE;
        size_t i = 0, end_index;
        size_t parameter_start = 0;

        start_index = ply_buffer_get_size (buffer->text_buffer);
        end_index = start_index;

        while (i < length) {
                switch (state) {
                case PLY_TERMINAL_BUFFER_PARSER_STATE_UNESCAPED:
                        if (input[i] == '\033') {
                                state = PLY_TERMINAL_BUFFER_PARSER_STATE_ESCAPED;
                        } else {
                                ply_buffer_append (buffer->text_buffer, "%c", input[i]);
                                end_index++;
                        }
                        break;
                case PLY_TERMINAL_BUFFER_PARSER_STATE_ESCAPED:
                        if (input[i] == '[') {
                                state = PLY_TERMINAL_BUFFER_PARSER_STATE_CONTROL_SEQUENCE;
                        }
                        break;
                case PLY_TERMINAL_BUFFER_PARSER_STATE_CONTROL_SEQUENCE:
                        if (isdigit (input[i])) {
                                parameter_start = i;
                                state = PLY_TERMINAL_BUFFER_PARSER_STATE_CONTROL_SEQUENCE_PARAMETER;
                        }
                        break;
                case PLY_TERMINAL_BUFFER_PARSER_STATE_CONTROL_SEQUENCE_PARAMETER:
                        if (input[i] == ';' || input[i] == 'm') {
                                int parameter_value;

                                parameter_value = atoi (&input[parameter_start]) - PLY_TERMINAL_COLOR_ATTRIBUTE_OFFSET;
                                if (parameter_value >= PLY_TERMINAL_COLOR_BLACK && parameter_value <= PLY_TERMINAL_COLOR_WHITE) {
                                        color = (ply_terminal_color_t) (parameter_value);
                                }
                        }

                        if (input[i] == ';') {
                                parameter_start = i + 1;
                        } else if (input[i] == 'm') {
                                if (end_index > start_index) {
                                        ply_text_attribute_t *attribute;

                                        attribute = calloc (1, sizeof(ply_text_attribute_t));
                                        attribute->color = color;
                                        attribute->start_index = start_index;
                                        attribute->span_length = end_index - start_index;

                                        ply_list_append_data (buffer->attributes, attribute);
                                }
                                state = PLY_TERMINAL_BUFFER_PARSER_STATE_UNESCAPED;
                        }
                        break;
                }
                i++;
        }

        if (state == PLY_TERMINAL_BUFFER_PARSER_STATE_CONTROL_SEQUENCE_PARAMETER &&
            end_index > start_index) {
                ply_text_attribute_t *attribute;

                attribute = calloc (1, sizeof(ply_text_attribute_t));
                attribute->color = color;
                attribute->start_index = start_index;
                attribute->span_length = end_index - start_index;

                ply_list_append_data (buffer->attributes, attribute);
        }
}

void
ply_terminal_buffer_iter_init (ply_terminal_buffer_iter_t *iter,
                               ply_terminal_buffer_t      *buffer)
{
        iter->buffer = buffer;
        iter->node = ply_list_get_First_node (buffer->attributes);
}

int
ply_terminal_buffer_iter_next (ply_terminal_buffer_iter_t *iter,
                               ply_text_attribute_t      **attribute,
                               const char                **text,
                               size_t                     *start_index,
                               size_t                     *end_index)
{
        ply_terminal_buffer_t *buffer;
        ply_text_attribute_t *current_attribute, *next_attribute;
        const char *bytes;

        if (iter->node == NULL) {
                return false;
        }

        buffer = iter->buffer;
        bytes = ply_terminal_buffer_get_bytes (buffer);

        current_attribute = ply_list_node_get_data (iter->node);
        *text = bytes + current_attribute->start_index;
        *attribute = current_attribute;
        *start_index = current_attribute->start_index;

        iter->node = ply_list_get_next_node (buffer->attributes, iter->node);

        if (iter->node != NULL) {
                next_attribute = ply_list_node_get_data (iter->node);
                *end_index = next_attribute->start_index - 1;
        } else {
                size_t last_index = ply_buffer_get_size (buffer) - 1;
                *end_index = last_index;
        }

        return true;
}
