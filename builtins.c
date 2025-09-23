#include <string.h>
#include <stdbool.h>
#include "builtins.h"
#include "io_helpers.h"
#include <stdlib.h>
#include <stdio.h>
#include "io_helpers.h"
#include <sys/stat.h>
#include <dirent.h>
#include <stdbool.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "commands.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define MAX_CLIENTS 11
#define MAX_MESSAGE_LENGTH 1110
#include <sys/prctl.h>

// ====== Command execution =====

/* Return: index of builtin or -1 if cmd doesn't match a builtin
 */
bn_ptr check_builtin(const char *cmd) {
    ssize_t cmd_num = 0;
    while (cmd_num < BUILTINS_COUNT &&
           strncmp(BUILTINS[cmd_num], cmd, MAX_STR_LEN) != 0) {
        cmd_num += 1;
    }
    return BUILTINS_FN[cmd_num];
}


// ===== Builtins =====

/* Prereq: tokens is a NULL terminated sequence of strings.
 * Return 0 on success and -1 on error ... but there are no errors on echo.
 */
ssize_t bn_echo(char **tokens) {
    ssize_t token_index = 1;
    int total_size = 0;
    int buffer = 122;

    void print_token_with_space(char *token) {
        if (total_size > 0) display_message(" ");
        display_message(token);
    }

    while (tokens[token_index] != NULL) {
        int token_length = strlen(tokens[token_index]);

        if (total_size + token_length >= buffer) {
            int space = buffer - total_size;
            if (space <= 0) {
                display_message("\n");
                return 0;
            }

            char *temp_token = malloc(space + 1);
            if (!temp_token) return -1;

            strncpy(temp_token, tokens[token_index], space);
            temp_token[space] = '\0';

            print_token_with_space(temp_token);
            free(temp_token);

            display_message("\n");
            return 0;
        }

        print_token_with_space(tokens[token_index]);
        total_size += token_length + 1;
        token_index++;
        buffer--;
    }

    display_message("\n");
    return 0;
}

void bn_cat_helper(FILE *file) {
    char store[300];

    while (fgets(store, sizeof(store), file)) {
        display_message(store);
    }
}

ssize_t bn_cat(char **tokens) {
    char *input = tokens[1];
    int path_count = 0;

    while (tokens[path_count + 1] != NULL) {
        path_count++;
    }

    if (path_count > 1) {
        display_error("ERROR: Too many arguments: cat takes a single file", "");
        return -1;
    }

    if (input) {
        FILE *cat = fopen(input, "r");
        if (cat) {
            bn_cat_helper(cat);
            fclose(cat);
            return 0;
        } else {
            display_error("ERROR: Cannot open file ", input);
            return -1;
        }
    }
    else if (!isatty(fileno(stdin))) {
        bn_cat_helper(stdin);
        return 0;
    }
    else {
        display_error("ERROR: No input source provided ", "");
        return -1;
    }
}

void show_message(const char *what, int number) {
    char message[300];
    sprintf(message, "%s %d\n", what, number);
    display_message(message);
}

ssize_t bn_wc(char **tokens) {
    FILE *wc = NULL;
    int path_count = 0;

    while (tokens[path_count + 1] != NULL) {
        path_count++;
    }

    if (path_count > 1) {
        display_error("ERROR: Too many arguments: wc takes a single file", "");
        return -1;
    }

    if (tokens[1]) {
        wc = fopen(tokens[1], "r");
        if (!wc) {
            display_error("ERROR: Cannot open file", "");
            return -1;
        }
    } else {
        wc = stdin;
    }

    int counts[3] = {0, 0, 0};
    int tracker = ' ';
    char store[1000];
    size_t info;

    while ((info = fread(store, 1, sizeof(store), wc)) > 0) {
        for (size_t i = 0; i < info; i++) {
            char c_har = store[i];
            counts[1]++;

            if (c_har == '\n') {
                counts[2]++;
            }

            if (!isspace(c_har) && isspace(tracker)) {
                counts[0]++;
            }

            tracker = c_har;
        }
    }

    if (wc != stdin) {
        fclose(wc);
    }

    show_message("word count", counts[0]);
    show_message("character count", counts[1]);
    show_message("newline count", counts[2]);

    return 0;
}

void determine_route(char *route_temp, char *new_route) {
    new_route[0] = '\0';
    char *token = strtok(route_temp, "/");
    int j = 0; 

    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
        } else if (strcmp(token, "..") == 0) {
            if (j > 0) {
                new_route[strlen(new_route) - j] = '\0';
                j = 0; 
            } else {
                strcat(new_route, "../");
            }
        } else if (strcmp(token, "...") == 0) {
            strcat(new_route, "../../");
        } else if (strcmp(token, "....") == 0) {
            strcat(new_route, "../../../");
        } else {
            strcat(new_route, token);
            strcat(new_route, "/");
        }
        token = strtok(NULL, "/");
    }

    size_t len = strlen(new_route);
    if (len > 0 && new_route[len - 1] == '/') {
        new_route[len - 1] = '\0';
    }
}

ssize_t bn_cd(char **tokens) {
    if (tokens == NULL || tokens[1] == NULL) {
        display_error("ERROR: Invalid path", "");
        return -1;
    }

    int path_count = 0;
    while (tokens[path_count + 1] != NULL) {
        path_count++;
    }

    if (path_count > 1) {
        display_error("ERROR: Too many arguments: cd takes a single path", "");
        return -1;
    }

    char *route = tokens[1];
    char *route2 = NULL;
    char *new_route = NULL;

    if (strcmp(route, ".") == 0) {
        return 0;
    }

    if (route[0] == '.') {
        char *route_temp = strdup(route);
        if (!route_temp) {
            return -1;
        }

        new_route = malloc(900);
        if (!new_route) {
            free(route_temp);
            return -1;
        }

        determine_route(route_temp, new_route);
        route2 = new_route;
        free(route_temp);
    } else {
        route2 = route;  
    }

    if (chdir(route2) != 0) {
        display_error("ERROR: Invalid path", route2);
        if (new_route) {
            free(new_route);
        }
        return -1;
    }

    if (new_route) {
        free(new_route);
    }

    return 0;
}

void f_f(char *files[], int tracker) {
    for (int i = 0; i < tracker; i++) {
        free(files[i]);
    }
}

int bn_ls_recursive_helper(DIR *folder, char *f, char *files[]) {
    struct dirent *file;
    int tracker = 0;

    while ((file = readdir(folder)) != NULL) {
        if (strcmp(file->d_name, ".") == 0 || strcmp(file->d_name, "..") == 0) {
            if (!f) {
                display_message(file->d_name);
                display_message("\n");
            }
            continue;
        }
        if (file->d_name[0] == '.') {
            continue;
        }
        if (f && !strstr(file->d_name, f)) {
            continue;
        }
        if (tracker < 1000) {
            files[tracker] = strdup(file->d_name);
            if (!files[tracker]) {
                continue;
            }
            tracker++;
        }
    }

    return tracker;
}

void bn_ls_recursive(const char *complete_route, const char *f, int level, int peak) {
    if (peak != -1 && level >= peak) {
        return;
    }
    DIR *folder = opendir(complete_route);
    if (folder) {
        char *files[1000];
        int tracker = bn_ls_recursive_helper(folder, (char *)f, files);
        closedir(folder);

        int i = 0;
        while (i < tracker) {
            display_message(files[i]);
            display_message("\n");

            char entire_route[900];
            snprintf(entire_route, sizeof(entire_route), "%s/%s", complete_route, files[i]);

            struct stat st;
            if (stat(entire_route, &st) == 0 && S_ISDIR(st.st_mode)) {
                if (peak == -1 || level + 1 < peak) {
                    bn_ls_recursive(entire_route, f, level + 1, peak);
                }
            }
            i++;
        }

        f_f(files, tracker);
    } else {
        display_error("ERROR: Invalid path\n", (char *)complete_route);
    }
}

int bn_ls_helper(DIR *folder, char *files[], const char *f) {
    struct dirent *file;
    int tracker = 0;

    while ((file = readdir(folder)) != NULL) {
        if (f && !(file->d_type == DT_DIR) && !strstr(file->d_name, f)) {
            continue;
        }
        if (file->d_name[0] == '.' && file->d_name[1] != '\0' && !(file->d_name[1] == '.' && file->d_name[2] == '\0')) {
            continue;
        }

        if (tracker < 1000) {
            files[tracker] = strdup(file->d_name);
            if (!files[tracker]) {
                continue;
            }
            tracker++;
        }
    }
    return tracker;
}

void bn_ls_helper2(const char *route, char *files[], int tracker, const char *f, int peak) {
        int k = 0;
        while (k < tracker) {
                char complete_route[900];
                snprintf(complete_route, sizeof(complete_route), "%s/%s", route, files[k]);
                struct stat st;
                if (stat(complete_route, &st) == 0 && S_ISDIR(st.st_mode)) {
                        if (strcmp(files[k], ".") != 0 && strcmp(files[k], "..") != 0) {
                                bn_ls_recursive(complete_route, f, 1, peak);
                        }
                }
                k++;
        }
}

ssize_t bn_ls(char **tokens) {
    const char *route = ".";
    const char *f = NULL;
    int r = 0;
    int peak = -1;

    int path_count = 0;
    int i = 1;
    char *paths[100];  

    while (tokens[i] != NULL) {
        if (strcmp(tokens[i], "--f") == 0 && tokens[i + 1] != NULL) {
            f = tokens[++i];
        }
        else if (strcmp(tokens[i], "--rec") == 0) {
            r = 1;
        }
        else if (strcmp(tokens[i], "--d") == 0 && tokens[i + 1] != NULL) {
            peak = atoi(tokens[++i]);
        }
        else {
            paths[path_count] = tokens[i];
            path_count++;
        }
        i++;
    }

    if (path_count > 1) {
        for (int j = 0; j < path_count - 1; j++) {
            if (strcmp(paths[j], paths[j + 1]) != 0) {
                display_error("ERROR: Too many arguments: ls takes a single path", "");
                return -1;
            }
        }
    }

    if (path_count == 0) {
        route = ".";  
    } else {
        route = paths[0];  
    }

    if (peak != -1 && !r) {
        display_error("ERROR: --d must be used with --rec", "");
        return -1;
    }

    DIR *folder = opendir(route);
    if (folder) {
        char *files[1000];
        int tracker = bn_ls_helper(folder, files, f);
        closedir(folder);

        int j = 0;
        while (j < tracker) {
            if (!f || strstr(files[j], f)) {
                display_message(files[j]);
                display_message("\n");
            }
            j++;
        }

        if (r) {
            bn_ls_helper2(route, files, tracker, f, peak);
        }
        f_f(files, tracker);
        return 0;
    }

    else {
        display_error("ERROR: Invalid path", (char *)route);
        return -1;
    }
}

ssize_t bn_kill(char **tokens) {
    if (tokens[1] == NULL) {
        display_error("ERROR: Usage: kill [pid] [signum]", "");
        return -1;
    }

    pid_t pid = atoi(tokens[1]);
    if (pid <= 0) {
        display_error("ERROR: The process does not exist", "");
        return -1;
    }

    int signum = SIGTERM;
    if (tokens[2] != NULL) {
        signum = atoi(tokens[2]);
        if (signum <= 0 || signum > 64) {
            display_error("ERROR: Invalid signal specified", "");
            return -1;
        }
    }
    if (kill(pid, signum) == -1) {
        display_error("ERROR: The process does not exist", "");
        return -1;
    }
    int status;
    if (waitpid(pid, &status, WNOHANG) > 0) {
        char msg[50];
        sprintf(msg, "[%d]+ Done %s\n", pid, tokens[1]);
        display_message(msg);
    }

    return 0;
}

ssize_t bn_ps(char **tokens) {
    (void)tokens;
    char msg[256];
    for (int i = 0; i < process_num; i++) {
        if (running[i]) {
            snprintf(msg, sizeof(msg), "%s %d\n", cmd[i], p_track[i]);
            display_message(msg);
        }
    }
    return 0;
}

int create_helper2(struct listen_sock *yo) {
    if (yo->s >= 0) {
        close(yo->s);
    }
    return -1;
}

struct sockaddr_in _create_helper1(int port) {
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = INADDR_ANY
    };
    memset(&addr.sin_zero, 0, 8);
    return addr;
}

int create(struct listen_sock *yo, int port) {
    yo->s = socket(AF_INET, SOCK_STREAM, 0);
    if (yo->s < 0) {
        return create_helper2(yo);
    }

    int connect = 1;
    int status = setsockopt(yo->s, SOL_SOCKET, SO_REUSEADDR,
                          (const char *)&connect, sizeof(connect));
    if (status < 0) {
        return create_helper2(yo);
    }

    struct sockaddr_in addr = _create_helper1(port);

    if (bind(yo->s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        display_error("ERROR: No port provided", "");
        return create_helper2(yo);
    }

    if (listen(yo->s, 5) < 0) {
        return create_helper2(yo);
    }

    return 0;
}

pid_t tracker = -1;

void shutdown_server(int sig) {
    (void)sig; 
    if (tracker > 0) {
        kill(tracker, SIGTERM);
        waitpid(tracker, NULL, 0);  
        tracker = -1;  
    }
    exit(0);
}


ssize_t bn_close_server(__attribute__((unused)) char **tokens) {
	if (tracker <= 0) {
        display_error("ERROR: No server running", "");
        return -1;
    }
    if (kill(tracker, SIGTERM) == -1) {
        return -1;
    }
    int status;
    waitpid(tracker, &status, 0);
    tracker = -1; 

    return 0;
}


struct sockaddr_in helper1(int connection, const char *host) {
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(connection);
    if (inet_pton(AF_INET, host, &serv_addr.sin_addr) <= 0) {
        serv_addr.sin_port = 0;
    }
    return serv_addr;
}

ssize_t send_helper1(int result, char *buffer, size_t buffer_size) {
    ssize_t n = read(result, buffer, buffer_size);
    if (n > 0) {
        buffer[n] = '\0';
    }
    return n;
}

ssize_t bn_send(char **tokens) {
    if (!tokens[1] || !tokens[2]) {
        return -1;
    }
    int num = atoi(tokens[1]);
    char *num2 = tokens[2];

    char content[128] = "";
    for (int i = 3; tokens[i]; i++) {
        strcat(content, tokens[i]);
        if (tokens[i + 1]) strcat(content, " ");
    }

    struct sockaddr_in serv_addr = helper1(num, num2);
    if (!serv_addr.sin_port) {
        return -1;
    }

    int result = socket(AF_INET, SOCK_STREAM, 0);
    if (result < 0) {
        return -1;
    }

    if (connect(result, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(result);
        return -1;
    }

    write(result, content, strlen(content));

    char read_this[16];
    ssize_t n = send_helper1(result, read_this, sizeof(read_this) - 1);

    close(result);
    return n;
}


int start_client_helper1(int result, char *reply, fd_set *analyze) {
    if (FD_ISSET(result, analyze)) {
        memset(reply, 0, 128);  
        ssize_t bytes_received = read(result, reply, 128 - 1);
        if (bytes_received > 0) {
            reply[bytes_received] = '\0';
            display_message(reply);
            display_message("\n");
        } else {
            return -1;
        }
    }
    return 0;
}

int start_client_helper2(int result, char *content, fd_set *analyze) {
    if (FD_ISSET(STDIN_FILENO, analyze)) {
        memset(content, 0, 128);  
        if (fgets(content, 128, stdin) == NULL) {
            return -1;
        }
        content[strcspn(content, "\n")] = '\0';
        if (strcmp(content, "exit") == 0) {
            return -1;
        }
        if (strlen(content) == 0) {
            return 0;
        }
        write(result, content, strlen(content));
    }
    return 0;
}

ssize_t bn_start_client(char **tokens) {
    if (!tokens[1]) {
        display_error("ERROR: No port provided", "");
        return -1;
    }

    if (!tokens[2]) {
        display_error("ERROR: No hostname provided", "");
        return -1;
    }

    int num = atoi(tokens[1]);
    char *num2 = tokens[2];

    struct sockaddr_in num3 = helper1(num, num2);
    if (!num3.sin_port) {
        return -1;
    }

    int result = socket(AF_INET, SOCK_STREAM, 0);
    if (result < 0) {
        return -1;
    }

    if (connect(result, (struct sockaddr *)&num3, sizeof(num3)) < 0) {
        close(result);
        return -1;
    }

    fd_set analyze;
    char content[128];
    char reply[128];
    while (1) {
        FD_ZERO(&analyze);
        FD_SET(result, &analyze);
        FD_SET(STDIN_FILENO, &analyze);

        int reflex = select(result + 1, &analyze, NULL, NULL, NULL);
        if (reflex < 0) {
            break;
        }

        if (start_client_helper1(result, reply, &analyze) < 0) {
            break;
        }

        if (start_client_helper2(result, content, &analyze) < 0) {
            break;
        }
    }

    close(result);
    return 0;
}

struct client_sock *receivers = NULL;
struct listen_sock acceptor = {0};

int start_server_helper1(struct listen_sock *acceptor, int *num, struct client_sock **receivers, fd_set *analyze) {
    if (FD_ISSET(acceptor->s, analyze)) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(acceptor->s, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            perror("accept");
            return 0;
        }

        struct client_sock *new_client = malloc(sizeof(struct client_sock));
        if (!new_client) {
            perror("malloc");
            close(client_fd);
            return 0;
        }

        new_client->s = client_fd;
        snprintf(new_client->str, sizeof(new_client->str), "client%d", ++(*num));
        new_client->next = *receivers;
        *receivers = new_client;
    }
    return 1;
}


void start_server2_helper(struct client_sock **receivers, struct client_sock *sender, const char *content) {
    char client_header[64];
    snprintf(client_header, sizeof(client_header), "%s: ", sender->str);

    display_message(client_header);
    display_message((char *)content);
    display_message("\n");

    char full_message[128];
    snprintf(full_message, sizeof(full_message), "%s%s", client_header, content);

    struct client_sock *now = *receivers;
    while (now != NULL) {
        if (write(now->s, full_message, strlen(full_message)) < 0) {
            perror("write");
        }
        now = now->next;
    }
}

int start_server_helper2(struct client_sock **receivers, fd_set *analyze) {
    struct client_sock **p = receivers;
    while (*p != NULL) {
        if (FD_ISSET((*p)->s, analyze)) {
            char content[128];
            memset(content, 0, sizeof(content));
            ssize_t count = read((*p)->s, content, sizeof(content) - 1);
            if (count <= 0) {
                close((*p)->s);
                struct client_sock *to_free = *p;
                *p = (*p)->next;
                free(to_free);
                continue;
            }

            content[count] = '\0';

            if (strcmp(content, "\\connected") == 0) {
                int num = 0;
                struct client_sock *now = *receivers;
                while (now != NULL) {
                    num++;
                    now = now->next;
                }

                char num_msg[64];
                snprintf(num_msg, sizeof(num_msg), "%d", num);
                write((*p)->s, num_msg, strlen(num_msg));
                continue;
            }

            start_server2_helper(receivers, *p, content);
        }
        p = &(*p)->next;
    }
    return 1;
}


ssize_t bn_start_server(char **tokens) {
    int num = 0;
    if (!tokens[1]) {
        display_error("ERROR: No port provided", "");
        return -1;
    }
    int num2 = atoi(tokens[1]);
    if (num2 <= 0) {
        return -1;
    }

    pid_t num3 = fork();
    if (num3 == 0) {
	//Learnt about prctl here:    
	//https://stackoverflow.com/questions/65271336/how-does-prctlpr-set-pdeathsig-sigkill-do
	prctl(PR_SET_PDEATHSIG, SIGKILL);
        signal(SIGTERM, shutdown_server);

        struct listen_sock acceptor;
        int result = create(&acceptor, num2);

        if (result == -1) {
            return -1;
        }

        receivers = NULL;

        fd_set analyze;
        int analyze2 = acceptor.s;

        while (1) {
            FD_ZERO(&analyze);
            FD_SET(acceptor.s, &analyze);

            struct client_sock *now = receivers;
            while (now != NULL) {
                FD_SET(now->s, &analyze);
                if (now->s > analyze2) {
                    analyze2 = now->s;
                }
                now = now->next;
            }

            int reflex = select(analyze2 + 1, &analyze, NULL, NULL, NULL);
            if (reflex < 0) {
                perror("select");
                continue;
            }

            start_server_helper1(&acceptor, &num, &receivers, &analyze);
            start_server_helper2(&receivers, &analyze);
        }
    } else if (num3 > 0) {
        tracker = num3;
        return 0;
    } else {
        return -1;
    }
    return 0;
}

