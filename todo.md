# GitClientQt Missing Features

## Done

- Separate "Untracked" section in the working tree.
- Working tree file operations: `git rm`, `git mv`, `git clean`.
- Add/remove worktree and remote management (set URL, set upstream, remove).
- Commit table column selection with save/restore.
- Commit table horizontal scroll bar.
- Custom application icon and `.desktop` file.
- Lane-based commit graph renderer.
- Submodule outdated status display (`--recursive` and `U` handling).
- Explicit "checkout commit" or "checkout file" action.
- Status bar visibility and window resizing.
- Ahead/behind counters on the branch list or status bar.
- Spell-check and commit message templates.
- Built-in terminal / arbitrary git command runner.
- Side-by-side and image diff support.

## Top functional gaps

- `git reset --hard/--mixed/--soft` to an arbitrary commit.
- Branch merge / merge preview UI.
- Multi-select in the commit table for range compare or batch cherry-pick.

## UI / polish gaps

- More commit table columns: stats, GPG signature, author e-mail.

## Done

- Asynchronous git execution with progress/cancel.

## Fixed bugs

- Prevent commit context menu false-positive triggers for disabled actions.
- Clear untracked file list on reload to avoid duplicates and stale entries.
- Move context-menu git feedback to the status bar so the menu does not close.
- Replace all remaining feedback message boxes with status bar messages.
- Remove the modal per-command QProgressDialog that blocks the UI.
- Wrap right dock in a scroll area so the Commit Files widget is not hidden by the system tray.

## Less common but useful

- `git notes`, `git bundle`, `git archive`, `git sparse-checkout`.
- `git maintenance` / `gc`.
- `git rebase --onto`.
- LFS status view.
