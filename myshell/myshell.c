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
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAXPIPES 20

char* strip(char* str) {
    int head = 0, tail = strlen(str) - 1;
    while (isspace(str[head])) {
        head++;
    }
    while (isspace(str[tail])) {
        str[tail--] = '\0';
    }
    return str + head;
}

void do_work(char* buf) {
    // redirect
    int tarfd = -1;
    if (strstr(buf, "<")) {
        tarfd = 0;
    }
    else if (strstr(buf, ">")) {
        tarfd = 1;
    }

    char* cmd  = NULL;
    char* file = NULL;
    if (tarfd == -1) {
        cmd = strip(buf);
    }
    else {
        cmd  = strip(strtok(buf, "><"));
        file = strip(strtok(NULL, "><"));

        // redirect
        int fd;
        if (tarfd == 0) {
            fd = open(file, O_RDONLY);
        }
        else {
            fd = open(file, O_RDWR | O_CREAT | O_TRUNC, 0664);
        }
        close(tarfd);
        dup2(fd, tarfd);
        close(fd);
    }

    printf("cmd=[%s], file=[%s]\n", cmd, file);

    char* argv[100];
    int   i = 0;

    argv[i++] = strtok(cmd, " ");
    printf("%s\n", argv[i - 1]);
    while ((argv[i++] = strtok(NULL, " "))) {
        printf("%s\n", argv[i - 1]);
    }
    if (execvp(cmd, argv) < 0) {
        perror("execvp");
        exit(1);
    }

    return;
}

int main() {
    char buf[1024] = {0};

    signal(SIGTTOU, SIG_IGN);

    while (1) {
        bzero(buf, 1024);

        printf("\033[32muser\033[37m@\033[31mname\033[0m $");
        scanf("%[^\n]s", buf);
        getchar();
        if (!strcmp(buf, "exit")) {
            break;
        }
        else if (!buf[0]) {
            continue;
        }

        int   i = 0;
        char* cmdarr[MAXPIPES];
        cmdarr[i++] = strtok(buf, "|");
        while ((cmdarr[i] = strtok(NULL, "|"))) {
            i++;
        }

        pid_t pid, pgrp;
        // no pipe
        if (!cmdarr[1]) {
            if ((pid = fork()) < 0) {
                perror("fork");
                exit(1);
            }
            if (pid == 0) {
                do_work(buf);
                exit(0);
            }
            else {
                wait(NULL);
                continue;
            }
        }
        else {
            // printf("WITH PIPE");
            int pipefd[MAXPIPES][2];
            int j;
            for (j = 0; j < i - 1; ++j) {
                if (pipe(pipefd[j]) < 0) {
                    perror("pipe");
                    exit(1);
                }
            }

            for (j = 0; j < i; ++j) {
                if ((pid = fork()) < 0) {
                    perror("fork");
                    exit(1);
                }
                if (pid == 0) {
                    if (j == 0) {
                        dup2(pipefd[j][1], 1);
                    }
                    else if (j == i - 1) {
                        dup2(pipefd[j - 1][0], 0);
                    }
                    else {
                        dup2(pipefd[j - 1][0], 0);
                        dup2(pipefd[j][1], 1);
                    }
                    for (int k = 0; k < i - 1; ++k) {
                        close(pipefd[k][0]);
                        close(pipefd[k][1]);
                    }
                    do_work(cmdarr[j]);
                }
                else {
                    if (j == 0) {
                        pgrp = pid;
                    }
                    setpgid(pid, pgrp);
                    //控制信号，显示前台进程组id
                    tcsetpgrp(0, pgrp);
                }
            }

            for (int k = 0; k < i - 1; ++k) {
                close(pipefd[k][0]);
                close(pipefd[k][1]);
            }

            for (int k = 0; k < i; ++k) {
                wait(NULL);
            }

            tcsetpgrp(0, getpid());
        }
    }

    return 0;
}
