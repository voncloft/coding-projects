# VArpeggio project rules

- Bump the application patch version for every delivered change, including fixes, features, and documentation changes. Do not leave a changed build using the previous version.
- Keep the CMake project version and the fallback `VARPEGGIO_VERSION` values in `src/mainwindow.cpp` and `src/helpmanual.cpp` consistent.
- Add a dated entry at the top of the README changelog for each new version. Preserve historical entries.
- Rebuild the changed application before handing it back and identify the updated build. Do not claim an already-running or separately installed copy has been updated unless it actually has.
