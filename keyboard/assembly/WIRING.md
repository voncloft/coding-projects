# Wire harness and connector assembly

The authoritative conductor map is `../harness.csv`, with a readable version in `../harness.md`. It contains 15 inter-board harnesses and64 conductors. `connector-pin-map.json` gives every native connector pad/net, including debug and external-module connectors. Numbers are the **KiCad pad numbers**, not an assumed left-to-right view of an unplugged housing. Verify both ends against the connector manufacturer's mating-face drawing and continuity-test each completed harness before applying power.

`cable-cut-list.csv` gives endpoint positions and prototype cut allowances. These lengths include service slack but are not a validated factory wire drawing: route one sample in the supplied case, trim/retain it without tension, and record the final finished lengths and dressing in the returned build record. Keep wires below the main PCB, away from switch tails, fanless cooling vents and display flex folds. Do not store excess wire over hot power components.

## Housing/contact procurement

| Board connector | Cable-side family | Conductors |
|---|---|---|
| JST BMxxB-GHS-TBT | GHR-xxV-S housing, SSHL-002T-P0.2 contact |26–30 AWG, insulation OD 0.76–1.0 mm; use 26 AWG for GH power and28 AWG signal. Maximum 1 A per contact with 26 AWG. |
| JST B2P-VH / B4P-VH | VHR-2N / VHR-4N, SVH-21T-P1.1 contacts |18 AWG copper for battery, core and IO power; current rating must cover the assigned circuit |
| 2.54 mm single-row headers | Keyed/labeled crimp receptacle housing matching pin count and 0.64 mm square posts |28 AWG signal; 26 AWG when the other end is GH; appropriately rated24 AWG contact for audio supply. Add positive retention against accidental reversal. |
| Molex 53261-0271 | Molex 51021-0200 housing and matching 50079-series PicoBlade crimp contacts |26–28 AWG paired speaker wire; verify exact contact plating/reel suffix during sourcing |

H07 and H15 terminate into **one 7-position audio housing**: H07 fills pins1–2, H15 fills pins3–7. Do not buy two housings and try to overlap them. H04 uses a 3-way main-board housing; H05 an 8-way; H08 a 6-way. Every pad listed in a harness must be populated in its matching housing. Unused debug/RGB-chain headers remain accessible but have no routine harness fitted.

H04 and H14 carry USB differential pairs: use shielded 90-ohm differential USB cable with the D+/D− pair kept twisted, matched lengths and minimal untwisted connector fan-out. H14 also carries USB VBUS sensing. Connect shield/drain to the specified ground structure at the board entry; do not substitute shield for the ground conductor. GH/header transitions are not impedance-controlled connectors, so USB qualification is a required prototype test. Do not stretch the prescribed wire lengths to accommodate an arbitrary new case.

H15 I2S is a short digital harness. Keep it separate from inductor switch nodes, IR drive and class-D speaker pairs. Use its ground conductor adjacent to the clock/data group. Route both speaker leads together and neither to chassis ground.

## External module leads, additional to H01–H15

- Battery: power-board J8, pad1 positive and pad2 GND **only after checking native pin map**. Use the selected pack's existing protection circuit; no bare unprotected cell assembly. Connect a matching VH housing and strain relief without solder heat reaching the pouch.
- NTC: separate 2-wire lead to power-board J9, for the cell-mounted103AT-2. It is not the battery's middle-cell tap. Continuity/temperature-test independently; an open/invalid NTC must inhibit charging.
- Display: CM4-carrier J5 pin 1 is +5V_DISPLAY, pin 2 unconnected, pin 3 GND. Place the red lead on pin 1 and black on pin 3 by their nets; the display's supplied housing does not imply correct orientation on this custom3-pin header. Fit the supplied 22-to-15-way DSI cable; the 22-pin end goes to carrier J8 and 15-pin end to the display. Carrier J9 is the eMMC recovery jumper, not the display connector.
- Speakers: two PicoBlade2-wire leads from audio J1/J2 to the selected PUI drivers. Keep left/right and phase consistent; both terminals are active amplifier outputs.
- CM4 antenna and ESP32 antenna: separate RF pigtails/antennas. Use the actual module-approved antenna types, place them against polymer and clear the battery/plate. Include their installation in the radio acceptance measurement.

All interconnect grounds in the pin map are required. Never join rails because two connectors use the same housing. Never connect a 5 V USB-UART adapter's supply pin to 3.3 V debug power. Programming adapters may share ground and 3.3 V logic; leave their supply pin disconnected when the board is already powered.

Label external rear ports permanently: **USB-A 1 / USB-A 2 / USB-A 3 / WATCH5V / PC DATA / CHARGE PD / OFF–ON**. The watch port is an output; the charging port is the PD input. These labels must remain visible after assembly.
