from pathlib import Path

def require_target():
    model=Path('/proc/device-tree/model')
    if not model.exists() or b'Compute Module 4' not in model.read_bytes():
        raise SystemExit('This service is restricted to the keyboard Compute Module 4.')
    if not Path('/etc/keyboard/rev-g-target').is_file():
        raise SystemExit('Keyboard target installation marker is missing.')
