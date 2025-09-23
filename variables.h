#ifndef VARIABLES_H
#define VARIABLES_H

#include <stddef.h>

typedef struct var_node {
    char *name;
    char *value;
    struct var_node *next;
} var_node;
void free_tokens(char **tokens);
const char *get_value(const char *name);
const char *get_value_helper(const char *token);
void set_variable(const char *assignment);
#endif
