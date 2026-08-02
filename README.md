# GitClientQt

A Qt-based Git client.

## Description

GitClientQt is a lightweight desktop Git client built with Qt. It provides a
three-pane interface that lets you browse repository branches, tags and stashes,
inspect the working tree, view commit history with unpushed/unpulled markers,
and compare file diffs without leaving the application.

The client is designed to stay out of your way while exposing the most common
Git operations: opening, cloning and initializing repositories, staging and
committing changes, pushing, pulling and fetching, stashing work in progress,
and managing pull-mode preferences. All interactions are backed by the regular
`git` command-line tool, so your existing Git setup (SSH keys, remotes,
submodules, hooks) works unchanged.

## Build

```bash
cmake -B build -S .
cmake --build build
```

## Install

After building, the executable is `build/GitClientQt`. You can run it directly:

```bash
./build/GitClientQt
```

To install it system-wide, copy the executable to a location on your `PATH`:

```bash
sudo cp build/GitClientQt /usr/local/bin/GitClientQt
```

No additional runtime files are required besides the Qt libraries and a working
`git` installation.

## Requirements

- Qt 5 (QtCore, QtGui, QtWidgets)
- CMake 3.x
- A `git` executable available on the system `PATH`

## Usage

1. **Open a repository** with `File → Open Repository`, or pick one from the
   `File → Recent Repositories` submenu.
2. **Clone** an existing remote repository with `File → Clone Repository`.
3. **Initialize** a new repository with `File → Initialize Repository`.
4. The main window shows the commit table in the center, the repository
   explorer (branches, tags, stashes, submodules) on the left, and the working
   tree on the right.
5. Click a commit to see the files changed in that commit in the right panel.
   Click a file to view its diff.
6. Stage or unstage files from the right panel with the context menu.
7. Enter a commit message and click `Commit` (or press `Ctrl+Return`) to create
   a commit.
8. Use the `Push` and `Pull` toolbar buttons to sync with the remote. The
   `Pull` button has a dropdown that also lets you run `Fetch all`.
9. Right-click unstaged files and choose `Stash file/folder` to stash changes;
   stashes are listed under `Stashes` in the left panel.
10. Reload the repository with `F5`.
11. Close the repository with `File → Close Repository` to return to the welcome
    state.

## Keyboard Shortcuts

| Shortcut | Action |
| --- | --- |
| `Ctrl+O` | Open repository |
| `Ctrl+Shift+N` | Clone repository |
| `Ctrl+N` | Initialize repository |
| `Ctrl+W` | Close repository |
| `Ctrl+Q` | Quit |
| `Ctrl+,` | Preferences |
| `Ctrl+Shift+P` | Push |
| `Ctrl+Shift+L` | Pull |
| `Ctrl+Shift+F` | Fetch all |
| `Ctrl+Return` | Commit |
| `F5` | Reload repository |
