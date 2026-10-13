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


## Feature 4: Horizontal Column Display (v1.3.0)

### Q1: Compare the implementation complexity of the "down then across" (vertical) printing logic versus the "across" (horizontal) printing logic. Which one requires more pre-calculation and why?

The vertical ("down then across") logic is more complex and needs more pre-calculation. Before printing anything I must compute the column width (longest name + 2), the number of columns (`terminal_width / col_width`), and the number of rows (`ceil(count / cols)`). Printing is then done row by row, and each item's array index has to be calculated as `c * rows + r`. I also have to handle the last column being shorter than the others, so I check that the index is less than the file count.

The horizontal ("across") logic is simpler. It only needs the terminal width and the column width. It loops through the array in its natural order, keeps a running horizontal position, and starts a new line when the next column would not fit. It doesn't need the number of rows or any index arithmetic, because the order of printing is the same as the order of the array.

### Q2: Describe the strategy you used in your code to manage the different display modes (-l, -x, and default). How did your program decide which function to call for printing?

I used an `enum DisplayMode` with three values: `MODE_DEFAULT`, `MODE_LONG` and `MODE_HORIZONTAL`. In `main`, a `getopt()` loop sets this variable: `-l` sets `MODE_LONG` and `-x` sets `MODE_HORIZONTAL`. If both are given, the last option wins. If no option is given, the mode stays `MODE_DEFAULT`.

The mode is passed to `do_ls()`. After `do_ls()` has read all the filenames into a dynamic array, it checks the mode with an if / else-if chain. `MODE_LONG` calls the long-listing function `print_long()` for each file, `MODE_HORIZONTAL` calls `print_horizontal()`, and anything else calls the original down-then-across function `print_columns()`. The directory reading logic is shared, and only the display function differs.


## Feature 5: Alphabetical Sort (v1.4.0)

### Q1: Why is it necessary to read all directory entries into memory before you can sort them? What are the potential drawbacks of this approach for directories containing millions of files?

`readdir()` returns entries one at a time, in whatever order the file system stores them, which is not alphabetical. To know which name comes first, the program has to compare every name with the others, so all the names must be available at once. Sorting cannot be done on a stream that is printed as it arrives. That is why I read every name into a dynamic array first and call `qsort()` on it.

For a directory with millions of files this has drawbacks. It uses a lot of memory, because every name is stored (a string plus a pointer for each entry). Nothing is printed until reading and sorting are completely finished, so the user waits and sees no output. Sorting takes O(n log n) time, which gets slow for very large n. In an extreme case the allocation could fail. Programs that need to handle such directories may skip sorting or process the entries in chunks.

### Q2: Explain the purpose and signature of the comparison function required by qsort(). How does it work, and why must it take const void * arguments?

`qsort()` is a generic function. It can sort an array of any type, so it doesn't know how to compare two elements. The comparison function tells it how. The signature is:

```c
int compare(const void *a, const void *b);
```

It must return a negative number if the first element should come before the second, zero if they are equal, and a positive number if the first should come after. `qsort()` calls it many times while sorting.

In my program the array holds `char *` strings, so `qsort()` passes pointers to those elements, which are `char **`. The function casts them back and compares the strings with `strcmp()`:

```c
int compare_names(const void *a, const void *b)
{
    const char *s1 = *(const char **)a;
    const char *s2 = *(const char **)b;
    return strcmp(s1, s2);
}
```

The parameters are `const void *` because `void *` can point to any type, so the same `qsort()` works for ints, structs or strings. `const` promises that the comparison will not modify the data being sorted.


## Feature 6: Colorized Output (v1.5.0)

### Q1: How do ANSI escape codes work to produce color in a standard Linux terminal? Show the specific code sequence for printing text in green.

ANSI escape codes are special character sequences that the terminal interprets as formatting commands instead of printing them. A sequence starts with the escape character (ASCII 27, written `\033` in C), followed by `[`, a number code, and the letter `m`. The code selects an attribute such as a color. Everything printed afterwards keeps that color until another code changes it. A reset code, `\033[0m`, returns the terminal to the default style.

To print text in green:

```c
printf("\033[0;32m%s\033[0m", "hello");
```

`\033[0;32m` switches the text color to green, `hello` is printed in green, and `\033[0m` resets the color so the following text is not affected. The escape codes take no space on the screen, so when aligning columns I calculate padding from the plain filename length, not the length of the colored string.

### Q2: To color an executable file, you need to check its permission bits. Explain which bits in the st_mode field you need to check to determine if a file is executable by the owner, group, or others.

The execute permission has one bit for each class of user in `st_mode`:

- `S_IXUSR` (0100): executable by the owner
- `S_IXGRP` (0010): executable by the group
- `S_IXOTH` (0001): executable by others

I test them with bitwise AND. A non-zero result means the bit is set:

```c
if (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
    /* file is executable by at least one class of user */
```

Before applying this check, I first make sure the entry is not a directory, because directories also carry the execute bit (it means "can enter"). In my code the directory check comes earlier in the chain, so directories are colored blue and only other files can be colored green.


## Feature 7: Recursive Listing (v1.6.0)

### Q1: In a recursive function, what is a "base case"? In the context of your recursive ls, what is the base case that stops the recursion from continuing forever?

A base case is the condition under which a recursive function stops calling itself and returns directly. Without one, the function would call itself forever and the program would crash with a stack overflow.

In my recursive `ls`, the recursion only happens for entries that are directories. The base case is a directory that has no subdirectories (or one that cannot be opened). `do_ls()` lists its entries, finds no entry for which `S_ISDIR()` is true, makes no further recursive call, and returns. `opendir()` failing also ends that branch immediately with an error message. Because a directory tree is finite, every path of calls eventually reaches such a directory.

Two details keep the recursion safe. Hidden entries are skipped when reading, so `.` and `..` are never visited, which would otherwise bring the program back to the same or the parent directory forever. I also use `lstat()`, so a symbolic link to a directory is not treated as a directory and is not followed, which prevents loops caused by links that point back to a parent.

### Q2: Explain why it is essential to construct a full path (e.g., "parent_dir/subdir") before making a recursive call. What would happen if you simply called do_ls("subdir") from within the do_ls("parent_dir") function call?

`readdir()` returns only the entry name, such as `subdir`, with no information about where it is. A relative name like `subdir` is resolved by the system against the program's current working directory, which does not change when the program descends into `parent_dir`. So `do_ls("subdir")` would look for `subdir` in the directory where the program was started, not inside `parent_dir`.

The result would be wrong. Usually `opendir("subdir")` would fail with "Cannot open directory", because no such directory exists in the current directory, so the nested contents would never be listed. In the worst case, a directory with the same name exists in the starting directory, and the program would list that one by mistake, giving wrong output and possibly recursing in a loop.

Building the full path with `parent_dir/subdir` gives the exact location of the entry. The same full path is also needed for `lstat()`, which must find the right file to know whether it is a directory.
