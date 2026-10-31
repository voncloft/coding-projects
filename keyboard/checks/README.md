# Verification evidence - Revision H

`current-h-native/` is the authority for current eight-board DRC, schematic parity and ERC results. The current strict, model, harness and BOM reports sit beside it. Native and populated STEP hashes are reconciled in `audit-integrity.json` and `current-assembly-sources.json`.

The current mechanical reports are `full-assembly-collisions.json`, `support-and-key-travel-check.json`, `detailed-fit-check.json` and `aon-mechanical.json`. Their native assembly hashes identify the tested solids. The complete pair sweep allows only the two documented CM4 connector mating regions.

`signal-paths.json`, `critical-pin-review.json`, `stackup-definition.json`, `impedance-estimates.json` and `reference-plane-sampling.json` document scoped electrical checks and estimates. These are not a whole-product electrical signoff. Open items are in `../OPEN-ITEMS.md`.

`logic-current/`, `firmware-current/`, `power-matrix/`, `software-current.log`, build logs and the firmware binary manifest cover current source and desktop fixtures. Address/undefined behavior sanitizers were used where stated; leak checking was disabled. No hardware flashing or real target execution is claimed.

`baseline-rev-g/` and `baseline-rev-f/` are historical evidence. They must not be used as current board results. Only the prior browser UI simulation is explicitly retained by `retained-browser-evidence.json`, after comparing its complete tested UI source. Older enclosure cable/rotary reports remain historical supplemental information, not a substitute for the current fit reports or physical acceptance.

No physical acceptance measurements have been performed. The 26-row acceptance record remains NOT TESTED. The audit integrity report validates consistency only; fabrication release remains on hold.
