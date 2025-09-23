#ifndef COMMANDS_H
#define COMMANDS_H
#include <stddef.h>
#define limit 128

extern int process_num;

extern pid_t p_track[limit];
extern char cmd[limit][128];
extern int running[limit];
extern int count[limit];
extern char completed[limit][128];
extern int completed_track[limit];
extern int important_num;
extern int current_count;


void append_p(pid_t pid, char *command, char *duration);
void pipe_it(char **tokens, size_t token_count);
void handler2(int signal);
void watcher();
#endif
