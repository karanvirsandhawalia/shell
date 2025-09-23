#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "commands.h"
#include "builtins.h"
#include "io_helpers.h"
#include "variables.h"
#include <sys/wait.h>

int bg_job_counter = 0;

void handler1(int code __attribute__((unused))) {
    display_message("\n");
}

void create_sig() {
    struct sigaction new;
    new.sa_handler = handler2;
    new.sa_flags = 0;
    sigemptyset(&new.sa_mask);
    sigaction(SIGCHLD, &new, NULL);

    struct sigaction newact;
    newact.sa_handler = handler1;
    newact.sa_flags = 0;
    sigemptyset(&newact.sa_mask);
    sigaction(SIGINT, &newact, NULL);
}

int main(__attribute__((unused)) int argc, __attribute__((unused)) char* argv[]) {
    char *prompt = "mysh$ ";
    char input_buf[MAX_STR_LEN + 1];
    input_buf[MAX_STR_LEN] = '\0';
    char input_copy[MAX_STR_LEN + 1];
    char *token_arr[MAX_STR_LEN] = {NULL};
    char *pipe_arr[MAX_STR_LEN] = {NULL};

    create_sig();
    while (1) {
        watcher();
        display_message(prompt);
        int ret = get_input(input_buf);
        strncpy(input_copy, input_buf, MAX_STR_LEN);
        input_copy[MAX_STR_LEN] = '\0';

        size_t token_count = tokenize_input(input_buf, token_arr);

        int has_pipe = 0;
	if (token_count > 0) {
		for (size_t i = 0; i < token_count; i++) {
			if (token_arr[i] && strchr(token_arr[i], '|') != NULL) {
				has_pipe = 1;
				break;
			}
		}
	}

        if (ret != -1 && (token_count > 0 && strncmp("exit", token_arr[0], 4) == 0) && token_arr[0][4] == '\0') {
            if (tracker > 0) {
		    kill(tracker, SIGTERM);  
		    waitpid(tracker, NULL, 0); 
		    tracker = -1;
	    }
	     free_tokens(token_arr);
            break;
        }

        if (ret == 0) {
            display_message("\n");
            free_tokens(token_arr);
            break;
        }

        if (ret == -1) {
            free_tokens(token_arr);
            continue;
        }
	if (token_count > 0) {
        if (token_count < 2 && strchr(token_arr[0], '=')) {
            char *assign = strchr(token_arr[0], '=');
            char *value = assign + 1;

            if (strchr(value, '$')) {
                if (strchr(value + 1, '$')) {
                    value = (char *)get_value_helper(value);
                } else {
                    value = (char *)get_value(value + 1);
                }
            }
            memmove(assign + 1, value, strlen(value) + 1);
            set_variable(token_arr[0]);
            free_tokens(token_arr);
            continue;
        }

        for (size_t i = 0; i < token_count; i++) {
            if (token_arr[i][0] == '$') {
                int dol = 0;
                for (size_t j = 1; token_arr[i][j] != '\0'; j++) {
                    if (token_arr[i][j] != '$') {
                        dol = 1;
                        break;
                    }
                }
                if (dol == 0) {
                    continue;
                }
                if (token_arr[i][1] == ' ' || token_arr[i][1] == '\0') {
                    continue;
                }

                const char *new_token;

                if (strchr(token_arr[i] + 1, '$')) {
                    new_token = get_value_helper(token_arr[i]);
                }
                else {
                    new_token = get_value(token_arr[i] + 1);
                }
                free(token_arr[i]);
                token_arr[i] = strdup(new_token);
            }
        }
	}

        if (has_pipe) {
            free_tokens(token_arr);
            size_t pipe_count = p_tok(input_copy, pipe_arr);
            if (pipe_count > 0) {
                pipe_it(pipe_arr, pipe_count);
            }
            fflush(stdout);
            continue;
        }

        if (token_count >= 1) {
            int process_check = 0;

            if (strcmp(token_arr[token_count - 1], "&") == 0) {
                process_check = 1;
                free(token_arr[token_count - 1]);
                token_arr[token_count - 1] = NULL;
                token_count--;
            }

            bn_ptr builtin_fn = check_builtin(token_arr[0]);
            if (builtin_fn != NULL) {
                if (process_check) {
                    pid_t pid = fork();
                    if (pid == -1) {
                        display_error("ERROR: Fork failed", "");
                    } else if (pid == 0) {
                        builtin_fn(token_arr);
                        display_message(prompt);
                        exit(0);
                    } else {
                        append_p(pid, token_arr[0], token_arr[1]);
                    }
                } else {
                    ssize_t err = builtin_fn(token_arr);
                    if (err == -1) {
                        display_error("ERROR: Builtin failed: ", token_arr[0]);
                    }
                }
            } else {
                pid_t pid = fork();
                if (pid == -1) {
                    display_error("ERROR: Fork failed", "");
                } else if (pid == 0) {
                    execvp(token_arr[0], token_arr);
                    display_error("ERROR: Unknown command: ", token_arr[0]);
                    exit(1);
                } else {
                    if (process_check) {
                        append_p(pid, token_arr[0], token_arr[1]);
                    } else {
                        int status;
                        waitpid(pid, &status, 0);
                    }
                }
            }
        }

        free_tokens(token_arr);
    }
    return 0;
}
