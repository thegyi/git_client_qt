# GitClientQt

A Qt-based Git client.

## Description

GitClientQt is a lightweight desktop Git client built with Qt. It provides a
three-pane interface that lets you browse repository branches, tags and stashes,
inspect the working tree, view commit history with unpushed/unpulled markers,
and compare file diffs without leaving the application.

The client is designed to stay out of your way while exposing the most common
Git operations: opening and initializing repositories, staging and committing
changes, pushing and pulling, and stashing work in progress. All interactions
are backed by the regular `git` command-line tool, so your existing Git setup
(SSH keys, remotes, submodules, hooks) works unchanged.

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

1. **Open a repository** with `File → Open Repository`.
2. The main window shows the commit table in the center, the repository
   explorer (branches, tags, stashes, submodules) on the left, and the working
   tree on the right.
3. Click a commit to see the files changed in that commit in the right panel.
   Click a file to view its diff.
4. Stage unstaged files from the right panel with the context menu.
5. Enter a commit message and click `Commit` to create a commit.
6. Use the `Push` and `Pull` toolbar buttons to sync with the remote.
7. Right-click unstaged files and choose `Stash file/folder` to stash changes;
   stashes are listed under `Stashes` in the left panel.
8. Close the repository with `File → Close Repository` to return to the welcome
   state.
