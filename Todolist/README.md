# Todo List

`Todo List` is a small Qt6 Widgets app that opens a post-it-note style window for building checklists on demand.

## Features

- Add checkboxes dynamically.
- Show task text as labels instead of visible text boxes.
- Edit the title and each task by double-clicking the label text.
- Strike through the task text when a checkbox is checked.
- Keep a remove button to the left of every checkbox row.
- Auto-scale the note layout, fonts, and controls as the window is resized.
- Use the `Format` button to change:
  `Button Background`, `Button Foreground`, `Form Background`, `Form Foreground`,
  `Checkbox Background`, `Checkbox Foreground`, `Strike-Through Foreground`,
  `Button Font Size`, and `Form Font Size`.
- Save the current color scheme as the app default from the format dialog so future launches reuse it automatically.
- Close the window without being prompted to save. If the current list was opened from an existing file, closing autosaves back to that file. New/unsaved lists just close.
- Remove a single checkbox row or remove all checked rows.
- Save and reopen lists as `.json` files.
- Launch from KDE's desktop `Create New` submenu or from Dolphin using `Create Todo List`.

## Project Layout

- `src/` contains the Qt6 application.
- `assets/` contains the app SVG icon and Qt resource file.
- `scripts/run-todolist.sh` runs the built binary from the project root.
- `scripts/install-context-menu.sh` installs the local launcher, KDE service-menu entry, and a Plasma desktop override that adds the action to the desktop `Create New` submenu.
- `packaging/` contains the `.desktop` templates used by the install script.

## Build

```bash
cmake -S /home/von/projects/Todolist -B /home/von/projects/Todolist/build
cmake --build /home/von/projects/Todolist/build
```

After the build finishes, you can run the app directly:

```bash
/home/von/projects/Todolist/build/todolist
```

You can also use the wrapper:

```bash
/home/von/projects/Todolist/scripts/run-todolist.sh
```

## KDE Context Menu Install

Run:

```bash
/home/von/projects/Todolist/scripts/install-context-menu.sh
```

That script installs:

- `~/.local/bin/todolist-launcher`
- `~/.local/share/applications/com.von.TodoList.desktop`
- `~/.local/share/kio/servicemenus/create-todo-list.desktop`
- `~/.local/share/icons/hicolor/scalable/apps/com.von.TodoList.svg`
- `~/.local/share/plasma/plasmoids/org.kde.desktopcontainment`

It also:

- marks the `.desktop` files executable
- copies Plasma's `org.kde.desktopcontainment` package into your local user data directory
- patches `FolderViewLayer.qml` to add a `Create Todo List` action to `Plasmoid.contextualActions`
- refreshes KDE's service cache with `kbuildsycoca6` when available

## How the Context Menu Works

There are two integrations:

- A KIO service menu for Dolphin and other file-manager surfaces.
- A local Plasma desktop-containment override for the actual desktop `Create New` submenu.

The desktop entry exists because Plasma's `org.kde.contextmenu` containment plugin builds the desktop menu from `org.kde.desktopcontainment/contents/ui/FolderViewLayer.qml`. The installer creates a local copy of that package and injects `Create Todo List` into the `newMenu` action, which is the `Create New` submenu you see on the desktop.

On the desktop, the action should appear under `Create New`. The desktop entry launches the app from its desktop entry. The app defaults to the desktop folder when no path argument is provided. The Dolphin/file-manager action still passes the clicked directory through `%u`.

## Future Installs

To repeat this setup on another machine:

1. Clone or copy the project into a local path.
2. Build it with CMake.
3. Run `scripts/install-context-menu.sh`.
4. Restart `plasmashell`, or log out and back in, so Plasma reloads the local desktop-containment package.

## Notes

- The local `org.kde.desktopcontainment` override is user-scoped. It does not modify `/usr/share`.
- If Plasma is updated and KDE changes `FolderViewLayer.qml`, rerun `scripts/install-context-menu.sh` so your local override is refreshed from the current system package before the TodoList action is patched back in.

## File Format

Saved lists are JSON files with the extension `.json`. The app still opens older `.todo.json` files. Each file stores:

- the list title
- each item text
- whether the item is checked
- the active color theme

## Default Theme

The format dialog has two app-level theme actions:

- `Set As Default` saves the current color scheme in the app settings.
- `Clear Saved Default` removes that saved scheme and falls back to the built-in colors on future launches.

When the app starts, it loads the saved default theme automatically. If you open a saved `.json` file that contains its own theme, that file theme takes precedence for that document.
