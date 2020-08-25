# mls

maanoo's ls

Faster than ls in long listing format and faster than tree in tree mode.

![Screenshot](https://user-images.githubusercontent.com/6997990/91236052-9fe8a000-e73f-11ea-9e13-d771962914d0.png)

## Usage

### Binding

Optional bind it to a keyboard shortcut, eg. `Alt-l`, by adding the following lines to your shell rc, eg. `~/.bashrc`:

```
# Alt-l -> mls -a
bind '"\el":"mls -a\n"'
```

### Options

```
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
       -r     Reverse the sort order, can be repeated

EXIT STATUS
       0      Successful program execution.
       1      Usage, syntax or configuration file error.
       2      Operational error.
```


## Install

```
git clone https://github.com/MaanooAk/mls
sudo make install
```

Arch Linux: [AUR package](https://aur.archlinux.org/packages/mls/).
