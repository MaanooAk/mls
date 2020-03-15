# mls

maanoo's ls

![Screenshot from 2020-03-15 16-55-50](https://user-images.githubusercontent.com/6997990/76703909-0754eb00-66de-11ea-8318-4f026cad4b01.png)

```
NAME
       mls - maanoo's ls (list directory contents)

SYNOPSIS
       mls [OPTION]... [FILE]...

DESCRIPTION
       mls List information about the FILEs or the current working directory if none is provided.

OPTIONS
   General options
       -a     Do not ignore hidden files

       -m     Mix the directories with the other files while sorting

       -d     Ignore all files except directories

   List options
       -s     Reduce listed info, can be repeated 2 times

   Tree options
       -t     Tree recursive directory listing

       -c     Collapse tree single leafs, can be repeated 2 times

   Sort options
       -U     Unsorted

       -T     Sort by modification time (default)

       -N     Sort by the file name

       -X     Sort by extension

       -S     Sort by file size

EXIT STATUS
       0      Successful program execution.

       1      Usage, syntax or configuration file error.

       2      Operational error.

SEE ALSO
       Full documentation <https://github.com/MaanooAk/mls>

```

