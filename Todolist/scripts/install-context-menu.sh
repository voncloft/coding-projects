#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
xdg_data_home="${XDG_DATA_HOME:-$HOME/.local/share}"
launcher_dir="$HOME/.local/bin"
applications_dir="$xdg_data_home/applications"
servicemenu_dir="$xdg_data_home/kio/servicemenus"
plasma_plasmoids_dir="$xdg_data_home/plasma/plasmoids"
icons_dir="$xdg_data_home/icons/hicolor/scalable/apps"
desktop_containment_source="/usr/share/plasma/plasmoids/org.kde.desktopcontainment"
desktop_containment_dest="$plasma_plasmoids_dir/org.kde.desktopcontainment"
desktop_containment_qml="$desktop_containment_dest/contents/ui/FolderViewLayer.qml"
launcher_path="$launcher_dir/todolist-launcher"
app_desktop_path="$applications_dir/com.von.TodoList.desktop"
servicemenu_path="$servicemenu_dir/create-todo-list.desktop"
icon_path="$icons_dir/com.von.TodoList.svg"

mkdir -p "$launcher_dir" "$applications_dir" "$servicemenu_dir" "$plasma_plasmoids_dir" "$desktop_containment_dest" "$icons_dir"

cat > "$launcher_path" <<EOF
#!/usr/bin/env bash
set -euo pipefail

project_root="$project_root"
binary="\$project_root/build/todolist"

if [[ ! -x "\$binary" ]]; then
  printf 'Built binary not found at %s\n' "\$binary" >&2
  printf 'Run: cmake -S %q -B %q && cmake --build %q\n' "\$project_root" "\$project_root/build" "\$project_root/build" >&2
  exit 1
fi

exec "\$binary" "\$@"
EOF

chmod +x "$launcher_path"

cp "$project_root/assets/todolist-icon.svg" "$icon_path"

sed "s|@LAUNCHER@|$launcher_path|g" \
  "$project_root/packaging/com.von.TodoList.desktop.in" > "$app_desktop_path"
sed "s|@LAUNCHER@|$launcher_path|g" \
  "$project_root/packaging/create-todo-list-servicemenu.desktop.in" > "$servicemenu_path"

chmod +x "$app_desktop_path" "$servicemenu_path"

cp -a "$desktop_containment_source/." "$desktop_containment_dest/"

if [[ ! -f "$desktop_containment_qml" ]]; then
  printf 'Desktop containment QML not found at %s\n' "$desktop_containment_qml" >&2
  exit 1
fi

if ! grep -q 'createTodoListDesktopAction' "$desktop_containment_qml"; then
  perl -0pi -e 's@(\n    PlasmaCore\.Action \{\n        id: actionSeparator\n        isSeparator: true\n    \}\n)@\n    PlasmaCore.Action {\n        id: createTodoListDesktopAction\n        text: i18n("Create Todo List")\n        icon.name: "com.von.TodoList"\n        onTriggered: Qt.openUrlExternally("applications:com.von.TodoList.desktop")\n    }\n$1@' "$desktop_containment_qml"

  perl -0pi -e 's@(\n        Plasmoid\.contextualActions\.push\(actionSeparator\);)@\n        const newMenuAction = folderView.model.action("newMenu");\n        if (newMenuAction && newMenuAction.menu) {\n            newMenuAction.menu.addSeparator();\n            newMenuAction.menu.addAction(createTodoListDesktopAction);\n        }\n$1@' "$desktop_containment_qml"
fi

perl -0pi -e 's/icon\.name: "view-pim-tasks"/icon.name: "com.von.TodoList"/g' "$desktop_containment_qml"

if command -v desktop-file-validate >/dev/null 2>&1; then
  desktop-file-validate "$app_desktop_path"
fi

if command -v kbuildsycoca6 >/dev/null 2>&1; then
  kbuildsycoca6 >/dev/null 2>&1 || true
fi

printf 'Installed launcher: %s\n' "$launcher_path"
printf 'Installed application entry: %s\n' "$app_desktop_path"
printf 'Installed service menu: %s\n' "$servicemenu_path"
printf 'Installed icon: %s\n' "$icon_path"
printf 'Installed Plasma desktop override: %s\n' "$desktop_containment_dest"
