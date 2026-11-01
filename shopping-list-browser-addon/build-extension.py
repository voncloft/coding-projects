#!/usr/bin/env python3
"""Build a Firefox XPI containing only extension runtime files."""
import json
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo

root = Path(__file__).resolve().parent
manifest = json.loads((root / 'manifest.json').read_text())
files = (
    'manifest.json', 'background.js', 'content-script.js',
    'popup.html', 'popup.js', 'popup.css',
    'options.html', 'options.js', 'options.css',
)
output = root / f"walmart_price_updater-{manifest['version']}.xpi"
with ZipFile(output, 'w', compression=ZIP_DEFLATED) as archive:
    for name in files:
        entry = ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))
        entry.compress_type = ZIP_DEFLATED
        entry.external_attr = 0o100644 << 16
        archive.writestr(entry, (root / name).read_bytes())
with ZipFile(output) as archive:
    assert archive.testzip() is None
    assert set(archive.namelist()) == set(files)
print(output)
