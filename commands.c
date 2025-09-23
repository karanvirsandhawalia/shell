#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include "commands.h"
#include "builtins.h"
#include "io_helpers.h"
#include "commands.h"
#include "variables.h"

pid_t p_track[limit];
char cmd[limit][128];
int running[limit];
int count[limit];
char completed[limit][128];
int completed_track[limit];
int process_num = 0;
int current_count = 1;
float p_time[limit];


void handler2(__attribute__((unused)) int signal) {
    int status;
    pid_t num;

    while ((num = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < limit; i++) {
            if (running[i] && p_track[i] == num) {
                if (strcmp(cmd[i], "sleep") == 0) {
                    if (p_time[i] == (int)p_time[i]) {
                        sprintf(completed[i], "[%d]+  Done %s %d\n", count[i], cmd[i], (int)p_time[i]);
                    } else {
                        sprintf(completed[i], "[%d]+  Done %s %.1f\n", count[i], cmd[i], p_time[i]);
                    }
                } else {
                    sprintf(completed[i], "[%d]+  Done %s\n", count[i], cmd[i]);
                }
                completed_track[i] = 1;
                running[i] = 0;
                process_num--;
                if (process_num == 0) {
                    current_count = 1;
                }
                break;
            }
        }
    }
}

void append_p_helper(int available_index, pid_t pid, char *command, char *duration) {
    p_track[available_index] = pid;
    strncpy(cmd[available_index], command, 128);
    running[available_index] = 1;
    count[available_index] = current_count++;
    process_num++;
    completed_track[available_index] = 0;

    if (duration != NULL) {
        p_time[available_index] = atof(duration);
    }
}

void append_p(pid_t pid, char *command, char *duration) {
    if (process_num >= limit) {
        return;
    }

    int available_index = -1;
    for (int i = 0; i < limit; i++) {
        if (running[i] == 0) {
            available_index = i;
            break;
        }
    }

    if (available_index != -1) {
        append_p_helper(available_index, pid, command, duration);
        char msg[150];
        snprintf(msg, sizeof(msg), "[%d] %d\n", count[available_index], pid);
        display_message(msg);
    }
}

void watcher() {
    for (int i = 0; i < limit; i++) {
        if (completed_track[i]) {
            display_message(completed[i]);
            completed_track[i] = 0;
        }
    }
}

void pipe_it_helper2(char **command) {
    execvp(command[0], command);
    char error_message[MAX_STR_LEN];
    snprintf(error_message, MAX_STR_LEN, "ERROR: Unknown command: %s", command[0]);
    display_error("", error_message);
    exit(1);
}

void pipe_it_helper1(char **tokens, int process1) {
    char error_message[MAX_STR_LEN];
    snprintf(error_message, MAX_STR_LEN, "ERROR: Builtin failed: %s", tokens[process1]);
    display_error("", error_message);
}

void pipe_it(char **tokens, size_t token_count) {
    int num[2];
    int num_before = -1;
    int process1 = 0;
    int process2 = 0;

    if (tokens[token_count - 1] != NULL && strcmp(tokens[token_count - 1], "&") == 0) {
        process2 = 1;
        tokens[token_count - 1] = NULL;
        token_count--;
    }

    for (size_t i = 0; i <= token_count; i++) {
        if (tokens[i] == NULL || strcmp(tokens[i], "|") == 0) {
            int final = (tokens[i] == NULL);

            if (!final && pipe(num) == -1) {
                exit(1);
            }

            pid_t pid = fork();
            if (pid == -1) {
                exit(1);
            } else if (pid == 0) {
                if (num_before != -1) {
                    dup2(num_before, STDIN_FILENO);
                    close(num_before);
                }
                if (!final) {
                    dup2(num[1], STDOUT_FILENO);
                    close(num[1]);
                }

                close(num[0]);

                tokens[i] = NULL;
                char **command = &tokens[process1];

                for (int j = 0; command[j] != NULL; j++) {
                    if (command[j][0] == '$') {
                        const char *expanded_value = get_value(command[j] + 1);
                        command[j] = strdup(expanded_value);
                    }
                }

                bn_ptr checking = check_builtin(command[0]);
                if (checking != NULL) {
                    exit(checking(command));
                } else {
                    pipe_it_helper2(command);
                }
            } else {
                if (process2) {
                    append_p(pid, tokens[process1], NULL);
                } else {
                    int status;
                    waitpid(pid, &status, 0);

                    if (WEXITSTATUS(status) != 0) {
                        pipe_it_helper1(tokens, process1);
                    }
                }

                close(num[1]);
                if (num_before != -1) {
                    close(num_before);
                }
                num_before = num[0];
                process1 = i + 1;
            }
        }
    }

    if (num_before != -1) {
        close(num_before);
    }

    while (process_num > 0) {
        watcher();
        usleep(100);
    }

    while (wait(NULL) > 0);
}
