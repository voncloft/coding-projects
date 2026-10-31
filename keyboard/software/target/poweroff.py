#!/usr/bin/env python3
from pathlib import Path
import subprocess
from guard import require_target
require_target()
if Path('/run/keyboard/poweroff-request').is_file():
    subprocess.run(['systemctl','poweroff'],check=True)
