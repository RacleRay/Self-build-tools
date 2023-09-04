/*************************************************************************
    > File Name: myshell.c
    > Author: racle
    > Mail: racleray@qq.com
    > Created Time:
 ************************************************************************/

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


#define MAXPIPES 20
#define PIPEIN  1
#define PIPEOUT 0

#define MAX_ARGV 100

#define REDIRECT_FILE_MASK 0664


char* strtrim(char* str);
void do_work(char *buf);


int main() {
    char buf[1024] = {0};

    // signal background write to shell
    signal(SIGTTOU, SIG_IGN);

    while (1) {
        bzero(buf, 1024);

        printf("\033[32muser\033[37m@\033[31mname\033[0m $");
        scanf("%[^\n]s", buf);
        getchar();

        if (!strcmp(buf, "exit")) {  // exit input
            break;
        } else if (!buf[0]) {  // empty input
            continue;
        }

        // read the commands piped.
        int n_cmd = 0;
        char* cmdarr[MAXPIPES];
        cmdarr[n_cmd++] = strtok(buf, "|");
        while ((cmdarr[n_cmd] = strtok(NULL, "|"))) {
            n_cmd++;
        }

        pid_t pid, pgrp;
        // no pipe input
        if (!cmdarr[1]) {
            if ((pid = fork()) < 0) {
                perror("fork");
                exit(1);
            }
            if (pid == 0) { // child
                do_work(buf);
                exit(0);
            } else {       // parent
                wait(NULL);
                continue;
            }
        } else { // with piped commands
            int pipefd[MAXPIPES][2];   // pipe for data stream
            for (int i = 0; i < n_cmd - 1; ++i) {
                // write to pipefd[1], read from pipefd[0]
                if (pipe(pipefd[i]) < 0) {
                    perror("pipe failed");
                    exit(-1);
                }
            }

            // run cmd
            for (int i = 0; i < n_cmd; ++i) {
                if ((pid = fork()) < 0) {
                    perror("fork failed");
                    exit(-2);
                }
                if (pid == 0) {
                    if (i == 0) {
                        // get data from parent
                        dup2(pipefd[i][PIPEIN], STDOUT_FILENO);  // curren out to first pipe in
                    } else if (i == n_cmd - 1)  {
                        dup2(pipefd[i - 1][PIPEIN], STDIN_FILENO);  // last pipefd out to stdin
                    } else {
                        // current stdfd connect to pipe before and pipe after
                        dup2(pipefd[i - 1][PIPEOUT], STDIN_FILENO);  // last pipe out to current stdin
                        dup2(pipefd[i][PIPEIN], STDOUT_FILENO);      // current stdin to next pipe in
                    }

                    do_work(cmdarr[i]);

                } else {
                    if (i == 0) {
                        pgrp = pid;  // first child id is group id
                    }
                    setpgid(pid, pgrp);  // set a group of process
                    tcsetpgrp(0, pgrp);  // set foreground group id
                }
            }

            for (int j = 0; j < n_cmd - 1; ++j) {
                close(pipefd[j][PIPEOUT]);
                close(pipefd[j][PIPEOUT]);
            }

            for (int j = 0; j < n_cmd; ++j) {
                wait(NULL);
            }

            // back to parent foreground process group
            tcsetpgrp(0, getpgid(getpid()));
        }
    }

    return 0;
}


void do_work(char* buf) {
    int tarfd = -1;
    if (strstr(buf, "<")) {
        tarfd = STDIN_FILENO;
    } else if (strstr(buf, ">")) {
        tarfd = STDOUT_FILENO;
    }

    char* cmd = NULL;
    char* file = NULL;

    // without redirection
    if (tarfd == -1) {
        cmd = strtrim(buf);
    } else {
        cmd = strtrim(strtok(buf, "<>"));
        file = strtrim(strtok(NULL, "<>"));

        int fd;
        if (tarfd == STDIN_FILENO) {
            fd = open(file, O_RDONLY);
        } else {
            fd = open(file, O_RDWR | O_CREAT | O_TRUNC, REDIRECT_FILE_MASK);
        }

        close(tarfd);
        dup2(fd, tarfd);
        close(fd);
    }

#ifdef _DEBUG
    printf("cmd=[%s], file=[%s]\n", cmd, file);
#endif

    char* argv[MAX_ARGV];
    int i = 0;
    argv[i++] = strtok(cmd, " ");
#ifdef _DEBUG
    printf("%s ", argv[i - 1]);
#endif

    while ((argv[i++] = strtok(NULL, " "))) {
        printf("%s ", argv[i - 1]);
    }

    if (execvp(cmd, argv) < 0) {
        perror("execvp");
        exit(-3);
    }

    return;
}


char* strtrim(char *str) {
    int head = 0, tail = strlen(str) - 1;

    while (isspace(str[head])) {
        head++;
    }
    while (isspace(str[tail])) {
        str[tail--] = '\0';
    }

    return str + head;
}
