/*
* Programming Assignment 02: ls v1.2.0
* Adds: -l long listing (v1.1.0) and default "down then across" columns.
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

extern int errno;

void do_ls(const char *dir, int long_flag);
void print_long(const char *dir, const char *name);
void print_columns(char **names, int count, int maxlen);
void mode_to_str(mode_t m, char *s);
int get_terminal_width(void);

int main(int argc, char *argv[])
{
    int opt;
    int long_flag = 0;

    while ((opt = getopt(argc, argv, "l")) != -1)
    {
        switch (opt)
        {
        case 'l':
            long_flag = 1;
            break;
        default:
            fprintf(stderr, "Usage: %s [-l] [dir...]\n", argv[0]);
            return 1;
        }
    }

    if (optind == argc)
    {
        do_ls(".", long_flag);
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            if (argc - optind > 1)
                printf("%s:\n", argv[i]);
            do_ls(argv[i], long_flag);
            if (argc - optind > 1)
                puts("");
        }
    }
    return 0;
}

/* Returns terminal width, or 80 if it cannot be detected */
int get_terminal_width(void)
{
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1 || w.ws_col == 0)
        return 80;
    return w.ws_col;
}

void do_ls(const char *dir, int long_flag)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    /* Step 1: read all filenames into a dynamic array */
    int capacity = 64;
    int count = 0;
    int maxlen = 0;
    char **names = malloc(capacity * sizeof(char *));
    if (names == NULL)
    {
        perror("malloc failed");
        closedir(dp);
        return;
    }

    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        if (count == capacity)
        {
            capacity *= 2;
            char **tmp = realloc(names, capacity * sizeof(char *));
            if (tmp == NULL)
            {
                perror("realloc failed");
                break;
            }
            names = tmp;
        }

        names[count] = strdup(entry->d_name);
        if (names[count] == NULL)
        {
            perror("strdup failed");
            break;
        }

        int len = strlen(names[count]);
        if (len > maxlen)
            maxlen = len;
        count++;
    }

    if (errno != 0)
        perror("readdir failed");

    closedir(dp);

    /* Step 2: display */
    if (long_flag)
    {
        long total = 0;
        for (int i = 0; i < count; i++)
        {
            char path[4096];
            struct stat st;
            snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
            if (lstat(path, &st) == 0)
                total += st.st_blocks;
        }
        printf("total %ld\n", total / 2);

        for (int i = 0; i < count; i++)
            print_long(dir, names[i]);
    }
    else
    {
        print_columns(names, count, maxlen);
    }

    /* Step 3: free memory */
    for (int i = 0; i < count; i++)
        free(names[i]);
    free(names);
}

/* "Down then across" column printing */
void print_columns(char **names, int count, int maxlen)
{
    if (count == 0)
        return;

    int term_width = get_terminal_width();
    int col_width = maxlen + 2;               /* longest name + 2 spaces */
    int cols = term_width / col_width;
    if (cols < 1)
        cols = 1;
    if (cols > count)
        cols = count;
    int rows = (count + cols - 1) / cols;     /* ceiling of count / cols */

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            int idx = c * rows + r;
            if (idx >= count)
                break;

            /* pad every item except the last one on the row */
            int next = (c + 1) * rows + r;
            if (c + 1 < cols && next < count)
                printf("%-*s", col_width, names[idx]);
            else
                printf("%s", names[idx]);
        }
        printf("\n");
    }
}

/* Convert st_mode into a string like "-rwxr-xr-x" */
void mode_to_str(mode_t m, char *s)
{
    s[0] = S_ISDIR(m) ? 'd' : S_ISLNK(m) ? 'l' : S_ISCHR(m) ? 'c' :
           S_ISBLK(m) ? 'b' : S_ISFIFO(m) ? 'p' : S_ISSOCK(m) ? 's' : '-';

    s[1] = (m & S_IRUSR) ? 'r' : '-';
    s[2] = (m & S_IWUSR) ? 'w' : '-';
    s[3] = (m & S_ISUID) ? ((m & S_IXUSR) ? 's' : 'S')
                         : ((m & S_IXUSR) ? 'x' : '-');

    s[4] = (m & S_IRGRP) ? 'r' : '-';
    s[5] = (m & S_IWGRP) ? 'w' : '-';
    s[6] = (m & S_ISGID) ? ((m & S_IXGRP) ? 's' : 'S')
                         : ((m & S_IXGRP) ? 'x' : '-');

    s[7] = (m & S_IROTH) ? 'r' : '-';
    s[8] = (m & S_IWOTH) ? 'w' : '-';
    s[9] = (m & S_ISVTX) ? ((m & S_IXOTH) ? 't' : 'T')
                         : ((m & S_IXOTH) ? 'x' : '-');
    s[10] = '\0';
}

void print_long(const char *dir, const char *name)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", dir, name);

    struct stat st;
    if (lstat(path, &st) == -1)
    {
        perror(path);
        return;
    }

    char perms[11];
    mode_to_str(st.st_mode, perms);

    struct passwd *pw = getpwuid(st.st_uid);
    struct group *gr = getgrgid(st.st_gid);

    /* ctime() gives "Mon Oct 11 10:30:00 2026\n"; keep "Oct 11 10:30" */
    char *t = ctime(&st.st_mtime);
    char date[13];
    strncpy(date, t + 4, 12);
    date[12] = '\0';

    printf("%s %2lu %-8s %-8s %8ld %s %s",
           perms,
           (unsigned long)st.st_nlink,
           pw ? pw->pw_name : "?",
           gr ? gr->gr_name : "?",
           (long)st.st_size,
           date,
           name);

    if (S_ISLNK(st.st_mode))
    {
        char target[4096];
        ssize_t n = readlink(path, target, sizeof(target) - 1);
        if (n != -1)
        {
            target[n] = '\0';
            printf(" -> %s", target);
        }
    }
    putchar('\n');
}
