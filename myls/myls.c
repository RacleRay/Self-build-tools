/*************************************************************************
    > File Name: myls.c
    > Author: racle
    > Mail: racleray@qq.com
    > Created Time:
 ************************************************************************/

#include <dirent.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>


int ALLTAG         = 0;
int LISTTAG        = 0;
int MAXFILENAMELEN = 0;
int MAXLINKNUM     = 0;
int TERMWIDTH      = 0;
int MAXFILESIZE    = 0;


typedef struct infos {
    char filetype;
    char permission[12];
    char uname[12];
    char gname[12];
    char stime[15];
    char filename[25];
    int  nlink;
    int  filesize;
    int  forecolor;
} infos;


void getfiletype(const struct stat* statbuf, infos* inform);
void getpermission(const struct stat* statbuf, char* permission);
void gettime(const struct stat* statbuf, char* stime);
int  cmp(const void* a, const void* b);
int getdigits(int num);
void lprint(infos* infocache);


void getfiletype(const struct stat* statbuf, infos* inform) {
    switch (statbuf->st_mode & S_IFMT) {
    case S_IFSOCK:
        inform->filetype  = 's';
        inform->forecolor = 35;
        break;
    case S_IFLNK:
        inform->filetype  = 'l';
        inform->forecolor = 36;
        break;
    case S_IFREG:
        inform->filetype  = '-';
        inform->forecolor = 37;
        if (statbuf->st_mode & S_IXUSR || statbuf->st_mode & S_IXGRP
            || statbuf->st_mode & S_IXOTH)
            inform->forecolor = 32;
        break;
    case S_IFBLK:
        inform->filetype  = 'b';
        inform->forecolor = 33;
        break;
    case S_IFDIR:
        inform->filetype  = 'd';
        inform->forecolor = 34;
        break;
    case S_IFCHR:
        inform->filetype  = 'c';
        inform->forecolor = 33;
        break;
    case S_IFIFO:
        inform->filetype  = 'p';
        inform->forecolor = 33;
        break;
    default:
        inform->filetype  = 'u';
        inform->forecolor = 37;
        break;
    }
}


void getpermission(const struct stat* statbuf, char* permission) {
    permission[0] = statbuf->st_mode & S_IRUSR ? 'r' : '-';
    permission[1] = statbuf->st_mode & S_IWUSR ? 'w' : '-';
    permission[2] = statbuf->st_mode & S_IXUSR ? 'x' : '-';
    permission[3] = statbuf->st_mode & S_IRGRP ? 'r' : '-';
    permission[4] = statbuf->st_mode & S_IWGRP ? 'w' : '-';
    permission[5] = statbuf->st_mode & S_IXGRP ? 'x' : '-';
    permission[6] = statbuf->st_mode & S_IROTH ? 'r' : '-';
    permission[7] = statbuf->st_mode & S_IWOTH ? 'w' : '-';
    permission[8] = statbuf->st_mode & S_IXOTH ? 'x' : '-';

    permission[2] = statbuf->st_mode & S_ISUID
                        ? (permission[2] == 'x' ? 's' : 'S')
                        : permission[2];
    permission[5] = statbuf->st_mode & S_ISGID
                        ? (permission[5] == 'x' ? 's' : 'S')
                        : permission[5];
    permission[8] = statbuf->st_mode & S_ISVTX
                        ? (permission[8] == 'x' ? 't' : 'T')
                        : permission[8];
}


void gettime(const struct stat* statbuf, char* stime) {
    struct tm* time = localtime((time_t *)&statbuf->st_mtim);

    char* month = "";
    switch (time->tm_mon) {
    case 0:
        month = "Jan";
        break;
    case 1:
        month = "Feb";
        break;
    case 2:
        month = "Mar";
        break;
    case 3:
        month = "Apr";
        break;
    case 4:
        month = "May";
        break;
    case 5:
        month = "Jun";
        break;
    case 6:
        month = "Jul";
        break;
    case 7:
        month = "Aug";
        break;
    case 8:
        month = "Sep";
        break;
    case 9:
        month = "Oct";
        break;
    case 10:
        month = "Nov";
        break;
    case 11:
        month = "Dec";
        break;
    }

    sprintf(stime, "%s %2d %02d:%02d", month, time->tm_mday, time->tm_hour,
            time->tm_min);
}


int cmp(const void* a, const void* b) {
    return strncmp((*(infos*)a).filename, (*(infos*)b).filename, 25);
}


int getdigits(int num) {
    int digits = 0;
    while (num) {
        num /= 10;
        digits++;
    }
    return digits;
}


void getwidth() {
    struct winsize size;
    TERMWIDTH = 80;

    if (!isatty(STDOUT_FILENO)) return;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) < 0) {
        perror("ioctl");
        exit(1);
    }

    TERMWIDTH = size.ws_col;

    return;
}


void lprint(infos* infocache) {
    printf("%c%s ", infocache->filetype, infocache->permission);
    printf("%*d ", MAXLINKNUM, infocache->nlink);
    printf("%s %s ", infocache->uname, infocache->gname);
    printf("%*d %s ", MAXFILESIZE, infocache->filesize,
           infocache->stime);
    if (infocache->filetype == 'l') {
        char ltarget[128] = {0};
        if (~readlink(infocache->filename, ltarget, 128)) {
            struct stat buf;
            infos       tmp;
            lstat(ltarget, &buf);
            getfiletype(&buf, &tmp);

            printf("\033[%dm%s\033[0m -> ", infocache->forecolor,
                   infocache->filename);
            printf("\033[%dm%s\033[0m\n", tmp.forecolor, ltarget);
        }
        else {
            perror("link file");
            exit(1);
        }
    }
    else {
        printf("\033[%dm%s\033[0m\n", infocache->forecolor, infocache->filename);
    }
}


int myls(char* workpath) {
    if (chdir(workpath) < 0) return -1;

    DIR* dir = opendir(".");
    if (dir == NULL) {
        closedir(dir);
        return -1;
    }

    struct dirent* dirent;
    struct stat    statbuf;

    infos cache[1024];
    int   count = 0;

    while ((dirent = readdir(dir)) != NULL) {
        lstat(dirent->d_name, &statbuf);

        infos inform;

        // 1 file type
        (void)getfiletype(&statbuf, &inform);

        // 2 permission
        strcpy(inform.permission, "---------");
        (void)getpermission(&statbuf, inform.permission);

        // 3 user name
        struct passwd* pwd = getpwuid(statbuf.st_uid);
        struct group*  grp = getgrgid(statbuf.st_gid);
        strcpy(inform.uname, (const char*)pwd->pw_name);
        strcpy(inform.gname, (const char*)grp->gr_name);

        // 4 time
        (void)gettime(&statbuf, inform.stime);

        // 5 link number
        inform.nlink = statbuf.st_nlink;
        int ndigits  = getdigits(inform.nlink);
        MAXLINKNUM   = ndigits > MAXLINKNUM ? ndigits : MAXLINKNUM;

        // 6 file size
        inform.filesize = statbuf.st_size;
        ndigits         = getdigits(inform.filesize);
        MAXFILESIZE     = ndigits > MAXFILESIZE ? ndigits : MAXFILESIZE;

        // 7 file name
        strcpy(inform.filename, (const char*)dirent->d_name);
        MAXFILENAMELEN = strlen(inform.filename) > MAXFILENAMELEN
                             ? strlen(inform.filename)
                             : MAXFILENAMELEN;

        cache[count++] = inform;
    }

    // sort
    qsort(cache, count, sizeof(infos), cmp);

    // print
    (void)getwidth();
    int cols   = TERMWIDTH / MAXFILENAMELEN - 1;
    int outcnt = 0;

    for (int i = 0; i < count; i++) {
        char filename[25] = {0};
        strncpy(filename, cache[i].filename, 25);

        if (ALLTAG && !LISTTAG) {
            printf("\033[%dm%-*s\033[0m", cache[i].forecolor,
                   MAXFILENAMELEN + 1, filename);
            outcnt++;
            if (outcnt % cols == 0 || i == count - 1) printf("\n");
        }
        else if (!ALLTAG && !LISTTAG) {
            if (filename[0] == '.')
                continue;
            else {
                printf("\033[%dm%-*s\033[0m", cache[i].forecolor,
                       MAXFILENAMELEN + 1, filename);
                outcnt++;
                if (outcnt % cols == 0 || i == count - 1) printf("\n");
            }
        }
        else if (!ALLTAG && LISTTAG) {
            if (filename[0] == '.')
                continue;
            else {
                lprint(&cache[i]);
            }
        }
        else {
            lprint(&cache[i]);
        }
    }

    closedir(dir);

    return 0;
}


int main(int argc, char** argv) {
    char* workpath = NULL;
    if (argc == 1) {
        workpath = ".";
    }
    else if (argc == 2) {
        if (!strncmp(argv[1], "-a", 3)) {
            ALLTAG   = 1;
            workpath = ".";
        }
        else if (!strncmp(argv[1], "-l", 3)) {
            LISTTAG  = 1;
            workpath = ".";
        }
        else if (!strncmp(argv[1], "-al", 3) || !strncmp(argv[1], "-la", 3)) {
            ALLTAG   = 1;
            LISTTAG  = 1;
            workpath = ".";
        }
        else {
            workpath = argv[1];
        }
    }
    else if (argc == 3) {
        if (!strncmp(argv[1], "-a", 3)) {
            ALLTAG = 1;
        }
        else if (!strncmp(argv[1], "-l", 3)) {
            LISTTAG = 1;
        }
        else if (!strncmp(argv[1], "-al", 3) || !strncmp(argv[1], "-la", 3)) {
            ALLTAG  = 1;
            LISTTAG = 1;
        }
        workpath = argv[2];
    }
    else {
        perror("Usage: ls [directory] or ls -al [directory]\n");
        exit(1);
    }

    if (myls(workpath)) {
        perror("myls");
        exit(1);
    };

    return 0;
}