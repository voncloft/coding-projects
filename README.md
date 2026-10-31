# Coding projects

A single-repository backup of the projects in this directory. Each project is stored here as ordinary files, so a clone or GitHub source download includes the project contents without submodules.

This repository starts with one snapshot of the current files. Earlier individual-project Git histories are not imported.

## Restore

```sh
git clone git@github.com:voncloft/coding-projects.git
cd coding-projects
```

Each project has its own build instructions and dependencies. Source code, required assets, CAD designs, firmware sources, and documentation are included. Build directories, compiled programs and libraries, firmware images, packaged browser extensions, runtime logs, local credentials, and development caches are excluded. Reinstall dependencies, rebuild the projects, and restore any credentials separately after reinstalling the OS.

## Update the backup

Run these commands from the top-level `coding-projects` directory:

```sh
git add -A
git commit -m "Update project backup"
git push
```

The projects retain their individual licenses and third-party notices. The keyboard project's existing engineering-audit and manufacturing-hold status is unchanged.
