# Reproduce the checks

Run in a separate developer workspace. These tests use temporary files, private serial/configuration fixtures and mocked hardware APIs. They do not flash or pair devices, mount real storage, install services or require owner credentials.

```sh
python3 -B -m unittest discover -s tests -p 'test*.py'
python3 -B tests/run_firmware_tests.py ./temporary-firmware-test-results
python3 -B tests/run_logic_tests.py ./temporary-logic-test-results
python3 -B tests/run_power_fault_matrix.py ./temporary-power-fault-results
python3 -B tests/check_designs.py --output ./temporary-design-checks
python3 -B tests/check_audit_integrity.py --output ./temporary-audit-integrity.json
```

The Python suite has 24 tests, including target-side GPIO failure cleanup, a private USB configuration tree and pseudo-terminal framing. Firmware fixtures compile the shipped C/C++ source against register, GPIO and radio stand-ins. Logic/fault runners enable AddressSanitizer and UndefinedBehaviorSanitizer; LeakSanitizer is explicitly disabled because this sandbox cannot use ptrace. This does not prove real radio timing, USB enumeration or power integrity.

Native checks require KiCad 9 CLI and the matching Python `pcbnew` module. Firmware fixtures require suitable C/C++ compilers. Check each runner's arguments before use. Reports are written to the selected temporary directory; native design files are read only.

The prior browser simulation report is historical evidence, explicitly identified in `checks/README.md`. Live display, Bluetooth, audio, charging and mechanical qualification remain in the unperformed manufacturer acceptance record. The target install prefix `/opt/keyboard/rev-g` is retained for compatibility; it is a target path, not the current hardware revision.
