# REPORT

## Feature 2: Long Listing (v1.1.0)

### Q1: What is the crucial difference between stat() and lstat()? When is lstat() more appropriate in ls?

Both system calls fill a `struct stat` with a file's metadata (size, permissions, owner, timestamps). The difference is how they treat symbolic links.

- `stat()` follows the link and returns information about the file the link points to.
- `lstat()` does not follow the link. It returns information about the link itself: its type (`S_IFLNK`), its own size, and its own permissions.

For regular files and directories the two behave the same.

In `ls`, `lstat()` is more appropriate. `ls -l` must show a symbolic link as a link, with type `l` and the form `name -> target`. If `stat()` were used, the link would look like the file it points to, and a broken link would cause an error. `lstat()` also avoids following links into directories, which prevents infinite loops during recursive listing.

### Q2: How can bitwise operators and macros like S_IFDIR or S_IRUSR extract the file type and permissions from st_mode?

`st_mode` is an integer in which different bits store different information. The upper bits store the file type, and the lower bits store the permissions (read, write and execute for owner, group and others, plus the setuid, setgid and sticky bits). Because each piece sits in its own bits, the bitwise AND operator (`&`) with a mask can extract it.

**File type:** AND with `S_IFMT`, which selects the type bits, then compare the result with a type constant.

```c
if ((st.st_mode & S_IFMT) == S_IFDIR)   // it is a directory
if ((st.st_mode & S_IFMT) == S_IFREG)   // it is a regular file
```

The macros `S_ISDIR()`, `S_ISREG()` and `S_ISLNK()` do the same thing in a shorter form. A plain `st_mode == S_IFDIR` would be wrong, because the permission bits are also stored in the integer.

**Permissions:** AND with a single permission bit. The result is non-zero only if that bit is set.

```c
if (st.st_mode & S_IRUSR)   // owner can read
if (st.st_mode & S_IWGRP)   // group can write
if (st.st_mode & S_IXOTH)   // others can execute
```

In my code, `mode_to_str()` tests each of the nine permission bits this way and prints `r`, `w`, `x` or `-`. The first character comes from the type macros. The special bits (`S_ISUID`, `S_ISGID`, `S_ISVTX`) change the execute character to `s`, `S`, `t` or `T`.


## Feature 3: Column Display (v1.2.0)

### Q1: Explain the general logic for printing items in a "down then across" columnar format. Why is a simple single loop through the list of filenames insufficient?

In a "down then across" layout, the names fill the first column from top to bottom, then the second column, and so on. To print it, I first compute the number of columns and rows:

- `col_width = longest_name_length + 2`
- `cols = terminal_width / col_width`
- `rows = ceil(total_files / cols)`

Then I print row by row. For row `r` and column `c`, the item to print is `names[c * rows + r]`. So the first row prints `names[0]`, `names[rows]`, `names[2 * rows]`, and so on.

A single loop over the array prints items in order, one after the other, which gives an "across" layout where neighbours in a row are consecutive names. A terminal prints line by line, so in the down-then-across layout the items that sit on one line are `rows` positions apart in the array. This means the program needs the total number of files and the number of rows before it prints anything, and it has to jump through the array with that stride.

### Q2: What is the purpose of the ioctl system call in this context? What would be the limitations of a fixed-width fallback (80 columns)?

`ioctl()` with the `TIOCGWINSZ` request asks the terminal driver for the current window size, filling a `struct winsize` that has the number of columns (`ws_col`) and rows. I use `ws_col` as the terminal width when calculating how many columns fit.

If the program only used a fixed width of 80 columns, it would be wrong whenever the terminal is not 80 wide. On a wide terminal, it would waste space and print fewer columns than could fit. On a narrow terminal, lines would be longer than the window and would wrap in the middle of the layout, which breaks the column alignment. It also couldn't adapt when the user resizes the window.
