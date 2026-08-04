# GitClientQt

A Qt-based Git client.

## Description

GitClientQt is a lightweight desktop Git client built with Qt. It provides a
three-pane interface that lets you browse repository branches, tags and stashes,
inspect the working tree, view commit history with unpushed/unpulled markers,
compare file diffs, and inspect per-line blame information without leaving the
application.

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
12. Right-click a file and choose `Blame` to open the per-line blame view. The
    source column expands to use the available window width.
13. The diff dock hides automatically when no diff is selected and reappears
    when you click a file that has changes.
14. Use the `Filter` toolbar to search commits by message, author, branch, or
    SHA. Type `file:<pattern>` to filter for commits that touched a matching
    file or path.

## Features

- **Repository management**: open, clone, and initialize repositories; reopen recent repositories from the `File` menu.
- **Commit history and search**: browse the commit graph with author, date, message and SHA information; use the `Filter` toolbar to filter by message, author, branch, SHA or touched files (`file:<pattern>`). Unpushed and unpulled commits are highlighted.
- **Working tree panel**: stage and unstage files, view modified/untracked files, and stash changes from the right dock.
- **Repository explorer**: inspect branches, tags, stashes and submodules in the left dock.
- **Diff view**: compare working-tree or committed file changes. The diff dock is hidden automatically when nothing is selected and reappears when a file with changes is clicked.
- **Blame view**: right-click any file and choose `Blame` to see per-line commit metadata, with a resizable source column that expands with the window.
- **Pull mode support**: choose between a normal pull, rebase, or merge pull strategy and fetch all branches from the pull dropdown.
- **Push targeting**: push the current branch to its configured remote; choose normal, force or `force-with-lease` push modes.
- **Customizable keyboard shortcuts**: edit shortcuts through `Edit → Preferences` in the Shortcuts tab.
- **Auto-reload**: the repository view can be reloaded manually or watched for changes.

## Configuration / Settings

Settings are stored in `QSettings` and can be changed through `Edit → Preferences`:

- **General**: pick the pull mode (`Merge`, `Rebase`, or `Plain Pull`).
- **Shortcuts**: edit the keyboard shortcuts that are used for open, clone, commit, push, pull, fetch and other actions.
- **Window state**: the main window geometry and dock layout are saved and restored.

## Troubleshooting

### Push fails with "Invalid username or token"

GitClientQt calls the `git` command-line tool, so authentication depends on your Git setup. For GitHub:

1. Create a Personal Access Token with `repo` and `read:org` scopes.
2. Update the remote URL to include the token:
   ```bash
   git remote set-url origin https://<user>:<token>@github.com/<user>/<repo>.git
   ```
3. Alternatively, use SSH:
   ```bash
   git remote set-url origin git@github.com:<user>/<repo>.git
   ```

### Blame window does not resize

The blame view uses a `QTableWidget` and the source column is set to stretch. If the source does not expand on resize, make sure the dialog window is actually being resized and not maximized inside a constrained layout.

### Diff dock is not visible

The diff dock is hidden automatically when no file is selected. Click a file in the working tree or commit file list to show it.

