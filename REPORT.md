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
