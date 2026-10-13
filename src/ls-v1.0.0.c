/*
* Programming Assignment 02: ls v1.6.0
* Adds: -R recursive listing.
* Existing: -l long listing, -x horizontal, default down-then-across,
*           alphabetical sorting, colorized output.
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

/* ANSI escape codes */
#define COLOR_RESET   "\033[0m"
#define COLOR_BLUE    "\033[0;34m"   /* directory */
#define COLOR_GREEN   "\033[0;32m"   /* executable */
#define COLOR_RED     "\033[0;31m"   /* tarballs / archives */
#define COLOR_PINK    "\033[0;35m"   /* symbolic links */
#define COLOR_REVERSE "\033[7m"      /* special files */

typedef enum
{
    MODE_DEFAULT,      /* down then across */
    MODE_LONG,         /* -l */
    MODE_HORIZONTAL    /* -x */
} DisplayMode;

int use_color = 0;     /* set to 1 in main when stdout is a terminal */

void do_ls(const char *dir, DisplayMode mode, int recursive);
void build_path(char *buf, size_t size, const char *dir, const char *name);
void print_long(const char *dir, const char *name);
void print_columns(const char *dir, char **names, int count, int maxlen);
void print_horizontal(const char *dir, char **names, int count, int maxlen);
void print_colored(const char *dir, const char *name);
int has_suffix(const char *name, const char *suffix);
void mode_to_str(mode_t m, char *s);
int get_terminal_width(void);
int compare_names(const void *a, const void *b);

int main(int argc, char *argv[])
{
    int opt;
    DisplayMode mode = MODE_DEFAULT;
    int recursive = 0;

    while ((opt = getopt(argc, argv, "lxR")) != -1)
    {
        switch (opt)
        {
        case 'l':
            mode = MODE_LONG;
            break;
        case 'x':
            mode = MODE_HORIZONTAL;
            break;
        case 'R':
            recursive = 1;
            break;
        default:
            fprintf(stderr, "Usage: %s [-l | -x] [-R] [dir...]\n", argv[0]);
            return 1;
        }
    }

    /* only emit color codes when writing to a terminal */
    use_color = isatty(STDOUT_FILENO);

    if (optind == argc)
    {
        do_ls(".", mode, recursive);
    }
    else
    {
        int multiple = (argc - optind > 1);
        for (int i = optind; i < argc; i++)
        {
            /* in recursive mode do_ls prints its own "dir:" header */
            if (multiple && !recursive)
                printf("%s:\n", argv[i]);
            do_ls(argv[i], mode, recursive);
            if (multiple)
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

/* Comparison function for qsort(): each argument is a char ** */
int compare_names(const void *a, const void *b)
{
    const char *s1 = *(const char **)a;
    const char *s2 = *(const char **)b;
    return strcmp(s1, s2);
}

/* Returns 1 if name ends with suffix */
int has_suffix(const char *name, const char *suffix)
{
    size_t n = strlen(name);
    size_t s = strlen(suffix);
    if (n < s)
        return 0;
    return strcmp(name + n - s, suffix) == 0;
}

/* Builds "dir/name" (avoids a double slash if dir already ends with '/') */
void build_path(char *buf, size_t size, const char *dir, const char *name)
{
    size_t len = strlen(dir);
    if (len > 0 && dir[len - 1] == '/')
        snprintf(buf, size, "%s%s", dir, name);
    else
        snprintf(buf, size, "%s/%s", dir, name);
}

/*
 * Prints a filename in the color that matches its type.
 * Uses lstat() so that a symbolic link is detected as a link.
 * Prints ONLY the name (no padding, no newline).
 */
void print_colored(const char *dir, const char *name)
{
    if (!use_color)
    {
        printf("%s", name);
        return;
    }

    char path[4096];
    build_path(path, sizeof(path), dir, name);

    struct stat st;
    if (lstat(path, &st) == -1)
    {
        printf("%s", name);
        return;
    }

    const char *color = NULL;
    mode_t m = st.st_mode;

    if (S_ISLNK(m))
        color = COLOR_PINK;
    else if (S_ISDIR(m))
        color = COLOR_BLUE;
    else if (S_ISCHR(m) || S_ISBLK(m) || S_ISFIFO(m) || S_ISSOCK(m))
        color = COLOR_REVERSE;
    else if (m & (S_IXUSR | S_IXGRP | S_IXOTH))
        color = COLOR_GREEN;
    else if (has_suffix(name, ".tar") || has_suffix(name, ".gz") ||
             has_suffix(name, ".zip"))
        color = COLOR_RED;

    if (color)
        printf("%s%s%s", color, name, COLOR_RESET);
    else
        printf("%s", name);
}

void do_ls(const char *dir, DisplayMode mode, int recursive)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    /* In recursive mode, print the directory name first, like ls -R */
    if (recursive)
        printf("%s:\n", dir);

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
        /* skips hidden files, and therefore also "." and ".." */
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

    /* Step 2: sort alphabetically */
    qsort(names, count, sizeof(char *), compare_names);

    /* Step 3: choose the display function based on the mode flag */
    if (mode == MODE_LONG)
    {
        long total = 0;
        for (int i = 0; i < count; i++)
        {
            char path[4096];
            struct stat st;
            build_path(path, sizeof(path), dir, names[i]);
            if (lstat(path, &st) == 0)
                total += st.st_blocks;
        }
        printf("total %ld\n", total / 2);

        for (int i = 0; i < count; i++)
            print_long(dir, names[i]);
    }
    else if (mode == MODE_HORIZONTAL)
    {
        print_horizontal(dir, names, count, maxlen);
    }
    else
    {
        print_columns(dir, names, count, maxlen);
    }

    /* Step 4: if -R, descend into every subdirectory */
    if (recursive)
    {
        for (int i = 0; i < count; i++)
        {
            char path[4096];
            struct stat st;

            /* full path = dir + "/" + name; needed so the recursive call
               opens the right directory, not one relative to the cwd */
            build_path(path, sizeof(path), dir, names[i]);

            /* lstat (not stat): symlinks are not followed, so no loops */
            if (lstat(path, &st) == -1)
                continue;

            if (S_ISDIR(st.st_mode))
            {
                puts("");                       /* blank line before header */
                do_ls(path, mode, recursive);   /* recursive call */
            }
        }
    }

    /* Step 5: free memory */
    for (int i = 0; i < count; i++)
        free(names[i]);
    free(names);
}

/* "Down then across" column printing (default) */
void print_columns(const char *dir, char **names, int count, int maxlen)
{
    if (count == 0)
        return;

    int term_width = get_terminal_width();
    int col_width = maxlen + 2;
    int cols = term_width / col_width;
    if (cols < 1)
        cols = 1;
    if (cols > count)
        cols = count;
    int rows = (count + cols - 1) / cols;

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            int idx = c * rows + r;
            if (idx >= count)
                break;

            print_colored(dir, names[idx]);

            /* pad using the PLAIN name length (escape codes take no space) */
            int next = (c + 1) * rows + r;
            if (c + 1 < cols && next < count)
                printf("%*s", col_width - (int)strlen(names[idx]), "");
        }
        printf("\n");
    }
}

/* Horizontal (row-major) printing for -x */
void print_horizontal(const char *dir, char **names, int count, int maxlen)
{
    if (count == 0)
        return;

    int term_width = get_terminal_width();
    int col_width = maxlen + 2;
    int pos = 0;
    int prev_len = 0;

    for (int i = 0; i < count; i++)
    {
        if (i > 0)
        {
            if (pos + col_width + col_width > term_width)
            {
                printf("\n");
                pos = 0;
            }
            else
            {
                printf("%*s", col_width - prev_len, "");
                pos += col_width;
            }
        }
        print_colored(dir, names[i]);
        prev_len = strlen(names[i]);
    }
    printf("\n");
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
    build_path(path, sizeof(path), dir, name);

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

    printf("%s %2lu %-8s %-8s %8ld %s ",
           perms,
           (unsigned long)st.st_nlink,
           pw ? pw->pw_name : "?",
           gr ? gr->gr_name : "?",
           (long)st.st_size,
           date);

    print_colored(dir, name);

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
