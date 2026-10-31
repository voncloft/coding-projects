# Full keyboard — Revision H engineering audit

**Updated 21 September 2026. MANUFACTURING HOLD.** This is the current engineering work set, not an approved fabrication package. Revision G is superseded for design review and must not be built. No physical prototype has been tested. Read `OPEN-ITEMS.md` and `CHECK-RESULTS.md` before using any fabrication exports. A replacement manufacturer ZIP has not been issued.

See [USB-C and headphones](USB-C-AND-HEADPHONES.md) for port selection, charging limits, future programming and the two independent Bluetooth roles.

## Work to quote and deliver

Please quote **one assembled engineering prototype**, with PCB/component minimum-order quantities and non-recurring engineering charges shown separately. PCBWay or another specialist may quote fabrication, assembly and enclosure manufacture. Retain the JLCPCB catalog references for chips and passives; purchase by the exact manufacturer part number and full specification. Quote enclosure manufacture, wire harnesses, final assembly, programming and the acceptance tests in this package. Return any necessary engineering changes in editable native files with regenerated manufacturing exports.

This work set may support an engineering review/quotation only. Before ordering, close the open design items and complete the explicit pre-build items in `assembly/ACCEPTANCE.md`: electrical-rating review, differential-impedance/stackup approval, procurement specifications, battery documentation and a small mechanical fit sample. After assembly, qualify the complete system against that test plan. These are manufacturer tasks in the requested prototype build, not claims that physical testing has already occurred. **Do not start a volume production run from this archive without a passed prototype and approved change record.**

## Files to use

| Folder/file | Purpose |
|---|---|
| `kicad/` | Eight KiCad 9 projects, schematics, routed boards, portable local footprints and 3D models |
| `manufacturing/<board>/` | Gerbers, separate plated/non-plated drill data, IPC netlist, schematic PDF, assembly PDF, all-parts and SMT/manual BOMs and placement files |
| `enclosure/keyboard-enclosure.FCStd` | Native FreeCAD full assembly, including all eight populated boards, screen, CM4, battery and speaker reference models |
| `enclosure/*.step` | Case, plate, spacers, straps, knob, light pipe, feet and custom heatsink geometry |
| `board-assemblies/` | Populated board STEP exports in each board's own coordinates |
| `bom/keyboard-parts-rev-h-audit.ods` | Parts, source addresses, listed prices, estimate ranges and manufacturer quote input cells |
| `bom/pcb-parts.csv`, `bom/assembly-parts.csv` | Procurement tables; exact MPNs or constrained procurement specifications |
| `assembly/` | Mechanical build, cable assembly, power constraints and prototype acceptance instructions |
| `firmware/` | RP2040/QMK/VIA and ESP32-C3 source, compiled images, programming instructions and source/version archives |
| `software/` | CM4 touch macro UI, local music integration, guarded target installer and deployment instructions |
| `checks/`,`CHECK-RESULTS.md` | Design-rule, connectivity, CAD, build and software-fixture evidence; physical tests are separately identified |
| `keyboard-layout.svg`, `key-positions.csv` | Key order, legends, widths and placement |
| `checks/audit-integrity.json` | Current native/source/export reconciliation; no release ZIP is approved |

Open `kicad/keyboard/keyboard.kicad_pro` in KiCad 9; each daughterboard has its own project. Open the FreeCAD assembly for enclosure fit. STEP exports are interchange files, not a requirement to use a particular enclosure factory CAD system. All distances are millimetres. Revision H identifies this engineering audit. Some native board title blocks retain their earlier individual board revision;`manufacturing/<board>/board-info.json` records it. The supplied file hashes identify the exact board versions reviewed, and earlier archive contents must not be mixed in.

## Product requirements included in this revision

- 108 Cherry MX-compatible switch positions on standard **19.05 mm pitch**, with a 6.25u space bar. Cherry MX RGB Red clear-housing switches are the procurement baseline. Shine-through keycaps have the exact custom legends in the layout.
- F1–F12, main typing rows, Print Screen, 3×3 macro cluster, inverted-T arrows, full numpad and three dedicated Bluetooth selectors. Nine physical macro keys have four VIA-programmable banks.
- Per-key RGB through 108 addressable LEDs, with a firmware brightness cap and switched power. VIA configuration uses the wired PC connection; the keyboard also provides Bluetooth HID to three stored hosts.
- Five-inch portrait colour touchscreen, its top aligned with the F-key region, rotary control to its right, and a small battery light above the screen. Green = 80–100%; yellow = 30–79%; red = 0–29%. Invalid/stale battery telemetry turns the indicator off rather than showing a false charge state.
- Editable touchscreen macro pages, button colours, screen theme, brightness, IR learn/test/save, Wi-Fi device actions through Home Assistant, and a Music page for Plexamp.
- Internal Wi-Fi CM4 with 32 GB eMMC, so the screen and Plexamp run independently of the attached PC. The music library/Plex server must remain reachable, for example on a NAS; turning off the only library server also removes access to that library.
- Three USB-A downstream ports, a dedicated watch USB-C power output, a PC USB-C data input, a combined USB-C PD charging/data/programming input, and an OFF/ON switch beside the charging input.
- Two internal speakers, plus CM4 Bluetooth A2DP headphone output, for key sounds, local music and USB audio from the PC. PC audio needs the USB data connection. USB thumb drives enumerate on the attached PC; Bluetooth is not a wireless USB hub. The internal CM4 does not take ownership of those ports when the PC is off.
- Protected 2S rechargeable battery, monitored temperature, sequenced shutdown, and load shedding. Watch/USB/RGB/audio power availability depends on the power budget; see the power guide.

**Remote-control scope:** the installed hardware is infrared receive/transmit plus Wi-Fi integration. Point an existing IR remote at the keyboard to learn its command, or import a known supported code, then aim the keyboard at the appliance and test it. The user must confirm a response before saving. A passive projector cannot reveal its command simply by being pointed at. There is no RFID/NFC reader, universal RF scanner or automatic code discovery circuit in this revision. Wi-Fi bulbs require their supported Home Assistant integration and owner-supplied credentials.

## Quote baseline

The spreadsheet's **approximately $745–$1,685 parts-and-case allowance** is a planning estimate, not a manufacturing quote. It excludes PCB fabrication, assembly labour, engineering/qualification, tax and shipping. Listed-price coverage is shown separately; blank catalog prices are not treated as zero-cost parts. Do not infer available stock from a catalog price. Supplier quotation and final purchased-MPN confirmation are part of procurement.

The manufacturer's handoff back to the owner should contain: the assembled prototype; updated native design and manufacturing files; purchased BOM; programmed firmware hashes; target OS image/version and commissioning instructions; completed measurements/test records; any deviations; and the priced path to a repeat build. Return it without the manufacturer's or another customer's credentials.

This project was developed and checked in an isolated workspace. No installation, flashing, service activation or operating-system changes were made to the owner's live PC.
