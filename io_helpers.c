#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include "io_helpers.h"


// ===== Output helpers =====

/* Prereq: str is a NULL terminated string
 */
void display_message(char *str) {
    write(STDOUT_FILENO, str, strnlen(str, MAX_STR_LEN));
}


/* Prereq: pre_str, str are NULL terminated string
 */
void display_error(char *pre_str, char *str) {
    write(STDERR_FILENO, pre_str, strnlen(pre_str, MAX_STR_LEN));
    write(STDERR_FILENO, str, strnlen(str, MAX_STR_LEN));
    write(STDERR_FILENO, "\n", 1);
}


// ===== Input tokenizing =====

/* Prereq: in_ptr points to a character buffer of size > MAX_STR_LEN
 * Return: number of bytes read
 */
ssize_t get_input(char *in_ptr) {
    int retval = read(STDIN_FILENO, in_ptr, MAX_STR_LEN+1); // Not a sanitizer issue since in_ptr is allocated as MAX_STR_LEN+1
    int read_len = retval;
    if (retval == -1) {
        read_len = 0;
    }
    if (read_len > MAX_STR_LEN) {
        read_len = 0;
        retval = -1;
        write(STDERR_FILENO, "ERROR: input line too long\n", strlen("ERROR: input line too long\n"));
        int junk = 0;
        while((junk = getchar()) != EOF && junk != '\n');
    }
    in_ptr[read_len] = '\0';
    return retval;
}

size_t tokenize_input(char *in_ptr, char **tokens) {
    char *curr_ptr = strtok(in_ptr, DELIMITERS);
    size_t token_count = 0;

    int num = 128 - 1;
    while (curr_ptr != NULL) {

       tokens[token_count] = malloc(128*sizeof(char));
        strncpy(tokens[token_count], curr_ptr, num);
        tokens[token_count][num] = '\0';

        curr_ptr = strtok(NULL, DELIMITERS);
        token_count++;                                                 
    }

    tokens[token_count] = NULL;

    return token_count;
}

void p_tok_helper(char *first, char *tokens[], size_t *i) {
    char *token = strtok(first, " ");
    while (token != NULL) {
        tokens[(*i)++] = token;
        token = strtok(NULL, " ");
    }
}

size_t p_tok(char *buffer, char *tokens[]) {
    if (!buffer || !tokens) return 0;

    size_t i = 0;
    buffer[strcspn(buffer, "\n")] = '\0';

    char *first = buffer;
    char *last;

    while ((last = strchr(first, '|')) != NULL) {
        *last = '\0';
        p_tok_helper(first, tokens, &i);
        tokens[i++] = "|";
        first = last + 1;
    }

    p_tok_helper(first, tokens, &i);
    tokens[i] = NULL;
    return i;
}

