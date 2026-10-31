# Verification results - Revision H audit

Updated 21 September 2026. **Manufacturing HOLD.** Current native files, exports, nominal CAD and fixture evidence are reconciled below. No physical keyboard has been built or tested, and no replacement supplier ZIP is approved. Open electrical design work is listed in `OPEN-ITEMS.md`.

| Check | Result and scope | Evidence |
|---|---|---|
| Native KiCad 9 checks | All eight boards: zero DRC violations, unrouted connections, schematic-parity mismatches and ERC violations | `checks/current-h-native/summary.json` and per-board reports |
| Strict footprint/courtyard checks | Zero errors/warnings with the normally ignored footprint/courtyard checks enabled in isolated copies; no DRC/ERC exclusions | `checks/strict-rule-audit.json`, `checks/model-and-rule-audit.json` |
| 3D model references | 937 references resolve; no populated component lacks a model | `checks/model-and-rule-audit.json` |
| Harnesses | 15 harnesses / 64 conductors match the current native connector pins | `checks/harness-check.json` |
| BOM and placement | 829 fitted positions; complete/SMT/manual placement and BOM groups agree | `checks/bom-cpl-audit.json` |
| Stackup and pair routing | Explicit layer constructions; USB/DSI widths corrected; connected paths and CM4 package-adjusted pair lengths pass the stated limits | `checks/signal-paths.json`, `manufacturing/STACKUP-REQUIREMENTS.md` |
| Impedance/reference screening | Ideal cross-section estimate has numerical convergence checks; filled-plane sampling identifies remaining launch/transition review areas | `checks/impedance-estimates.json`, `checks/reference-plane-sampling.json`. Not a physical SI pass |
| Selected IC pin review | 100 pad-to-net checks plus 12 USB hub strap/reset/bias resistors; supervisor expander bit map and stereo channel-selection resistors agree | `checks/critical-pin-review.json`. Not a complete electrical signoff |
| Current assembly sources | All eight fresh populated STEP files loaded, valid and linked by hash in the reopened native FreeCAD assembly | `checks/current-assembly-sources.json` |
| Full nominal assembly | 19 assemblies / 171 pair comparisons; no unexpected intersections | `checks/full-assembly-collisions.json` |
| Supports and key travel | 36 posts at zero nominal centre/height offset; 108 key envelopes clear the frame through 4 mm travel | `checks/support-and-key-travel-check.json` |
| Accessories and interchange files | 23 accessory checks clear the compared solids; 26 mechanical STEP files reopen as valid shapes | `checks/detailed-fit-check.json` |
| Revised AON power parts | Current power assembly and supplier maximum L2 envelope clear the compared case/module/cable allocations | `checks/aon-mechanical.json` |
| Firmware builds | QMK/RP2040 and ESP32 compiled; shipped sources/binaries and refreshed upstream archives match recorded hashes | Build logs, `firmware/binaries/manifest.json`, `checks/source-archive-check.json` |
| Actual-source logic fixtures | 8 QMK and 18 ESP32 scenarios pass AddressSanitizer/UndefinedBehaviorSanitizer; hardware APIs mocked | `checks/logic-current/logic-tests.json` |
| Power/protocol fixtures | 29 supervisor scenarios, 45 individually addressed I2C fault cases, 771 C/Python packet vectors and 400 corruption trials pass | `checks/firmware-current/firmware-tests.json`, `checks/power-matrix/power-fault-matrix.json` |
| Software boundary fixtures | 24 tests pass, including isolated pseudo-terminal framing, GPIO reset/cleanup, private USB gadget configuration and mocked Bluetooth failures | `checks/software-current.log` |
| Browser UI | Prior successful browser simulation retained only after byte-for-byte confirmation that its UI source is unchanged | `checks/retained-browser-evidence.json`. Historical simulation, not a new target run |
| ODS workbook | Three sheets, 409 formulas, 829 positions; totals and visible missing-quote cells reconcile | `checks/ods-check.json`, `checks/workbook-check.json` |
| Assembly drawings | All 16 front/back views fit their pages without clipping; refreshed from the current boards | `checks/assembly-pdf-check.json` |
| Audit integrity | Current board/export/model/source/report hashes reconciled; Gerber ZIP CRC/content checked | `checks/audit-integrity.json`. Does not authorize fabrication |

## Mechanical scope

The only waived full-assembly intersections are two known CM4 connector mating regions, totaling approximately 1.36227 cubic millimetres with the current laminate thickness. Their bounds are recorded individually. No unrelated overlap is suppressed.

Some models are simplified maximum-dimension packages or purchased-module envelopes. The CM4 supplier model includes the previously documented conservative repair of one invalid solid. Valid nominal geometry is not physical metrology. Own-board mating plug envelopes are excluded where they intentionally contain their connector body. Formed cables, latches, purchased part tolerances, screw engagement and manufacturing shrinkage need real samples.

The current native assembly uses all eight refreshed populated board exports. Mechanical part STEP files remain the same case geometry and reopen successfully. Custom heatsink performance, battery retention, display-ribbon folds and connector insertion forces have not been measured.

## Software and hardware scope

The tests use real project source against controlled fixtures. They do not exercise actual regulators, radios, USB links, audio devices or the target Linux system. LeakSanitizer was disabled because this sandbox does not support its ptrace requirement; no leak-test result is claimed. Build success and source hashes do not prove hardware operation.

All 26 physical/procurement acceptance rows remain NOT TESTED. Stackup/SI signoff, capacitor/ripple margins, USB input startup/inrush, battery documentation and complete electrical review remain open before an approved build. No owner PC installation, device flashing, radio pairing, service activation or host audio changes were performed.
