#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "io_helpers.h"
#define MAX_VAR_NAME 128
#include <ctype.h>

void free_tokens(char **tokens) {
    if (tokens == NULL) return;
    size_t i = 0;
    while (tokens[i] != NULL) {
        free(tokens[i]);
        i++;
    }
}


typedef struct node {
    char *name;
    char *value;
    struct node *next;
} node;
static node *vars = NULL;

const char *get_value(const char *name) {
    node *curr = vars;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            return curr->value;
        }
        curr = curr->next;
    }
    return "";
}

const char *get_value_helper(const char *token) {
    static char result[MAX_VAR_NAME];  
    size_t result_index = 0;
    size_t result_size = sizeof(result) - 1; 

    token++; 

    while (*token != '\0') {
        const char *var_end = strchr(token, '$');
        size_t var_len = (var_end) ? (size_t)(var_end - token) : strlen(token); 

        char var_name[MAX_VAR_NAME];
        strncpy(var_name, token, var_len);
        var_name[var_len] = '\0'; 

        const char *value = get_value(var_name);

        while (*value && result_index < result_size) {
            result[result_index++] = *value++;
        }

        if (var_end) {
            token = var_end + 1;  
        } else {
            break;  
        }
    }

    result[result_index] = '\0'; 

    return result;
}
void set_variable(const char *assignment) {
    char *assign = strchr(assignment, '=');
    if (!assign || assign == assignment) return;

    size_t name_len = assign - assignment;
    char name[MAX_VAR_NAME];
    strncpy(name, assignment, name_len);
    name[name_len] = '\0';

    const char *value = assign + 1;

    node *curr = vars;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            free(curr->value);
            curr->value = strdup(value); 
            return;
        }
        curr = curr->next;
    }

    node *new_node = malloc(sizeof(node));
    new_node->name = strdup(name);
    new_node->value = strdup(value); 
    new_node->next = vars;
    vars = new_node;
}
