# Electrical fabrication and power baseline — Revision H audit

**Manufacturing HOLD.** The following requirements do not authorize a build. See `../OPEN-ITEMS.md`.

## Board fabrication

Manufacture one of each of the eight board designs. The seven non-audio boards have four copper layers; stereo-audio has two. All are nominal 1.6 mm FR-4. Copper layer order is F.Cu / In1.Cu / In2.Cu / B.Cu for four-layer boards, F.Cu / B.Cu for audio. Match each board's Gerber job and native stack definition. Use ENIG, matching the current native stackup. See `../manufacturing/STACKUP-REQUIREMENTS.md` for the explicit laminate and controlled-impedance requirements; supplier approval remains open. Quote 1 oz nominal external copper and the native internal copper requirement, solder mask both sides and white legend. No blind/buried vias are specified. Plated and non-plated drill files are separate; preserve routed slots and mounting holes.

Board outlines are the Edge.Cuts **centre lines**. Gerbers and drills use the absolute native board origin. Placement files use millimetres, positive X right, positive Y up as viewed from the front; bottom rotations are native KiCad, not mirrored. The assembly house must verify its library zero-angle conventions in its placement preview, especially hot-swap sockets, reverse-mounted RGB LEDs, USB connectors, QFNs and the 200-pin CM4 connector pair.

Use `all-bom.csv`/`all-cpl.csv` as the complete 829-position inventory across the eight boards. `jlc-smt-*` excludes footprints with plated through-hole pads;`manual-*` contains those mixed or through-hole assemblies for manual/selective solder. A connector with both SMT and through-hole anchors appears in the manual group so it is not silently omitted. The manufacturer may move such parts into its qualified pin-in-paste process, preserving counts. Mounting holes have no purchased component and are absent from BOM/CPL. There are no intentionally omitted electronic parts in this release.

`bom/pcb-parts.csv` is the procurement authority for assigned MPNs/specifications, including updated passive selections. Schematic values describe the circuit; a higher-voltage capacitor in the same package is intentional where the procurement table selects it. Parts whose MPN starts with `PROCUREMENT SPEC` must be sourced by the manufacturer to that complete specification and returned with the purchased MPN/LCSC code. Do not fabricate catalog IDs. The quote-stage assignment does not authorize a different IC pinout, capacitance, resistor value, connector mating height or fuse rating.

The copper uses regular horizontal/front and vertical/back routing for ordinary signals. Compact pad escapes, power distribution, ground planes and high-speed pairs are exceptions. **Native DRC does not establish differential impedance or high-current thermal performance.** Before releasing Gerbers to fabrication, the manufacturer's electronics engineer must approve a real fabrication stackup, compare routing to 90-ohm USB /100-ohm DSI differential targets, review reference-plane continuity/via transitions and QFN thermal/return paths, and return any required corrections with new DRC/ERC and enclosure results. Do not change trace widths or internal layers silently during CAM processing. Controlled-impedance coupons and the prototype link tests are the acceptance evidence.

Bare boards require electrical open/short testing using the supplied IPC-D-356 netlist. Assembled boards require AOI, polarity review, continuity and X-ray of hidden solder joints where optical inspection cannot establish wetting/bridging. Respect package MSL/bake/reflow and LED/switch-socket heat limits. No glue or conformal coating on hot-swap contacts, USB mating contacts, FFC latches or RF sockets.

## Rails and component constraints

| Rail/function | Baseline |
|---|---|
| Input | Dedicated USB-C PD, validated 20 V /3 A offer required for full input-power mode; HUSB238_002DD negotiation and independent voltage telemetry |
| Pack | Protected 2S, 7.4 V nominal /8.4 V charge ceiling, 5 Ah CU-JAS503, 6 A maximum specified discharge |
| Charger | BQ25798, 2S configuration/readback, 1 A charge request; temperature/fault gating and watchdog |
| Always-on | AP63203WU-7, 3.3 V; 2 A regulator class; verify ESP32/radio/RP2040 transient headroom before connecting all loads |
| Core and IO | TPS568230 regulators, 5.1 V nominal feedback; output banks 4×47 µF each |
| Audio | TPS568230, 3.3 V nominal, 4×47 µF bank; two MAX98357A amps into 8-ohm speakers |
| RGB | TPS2553 limited switched rail, brightness cap 32/255 in QMK; 108 SK6812MINI-E LEDs |
| External USB | Three USB-A outputs, budget 1.5 A combined; per-port limiting/fault signals |
| Watch | TPS25810 USB-C source, 5 V /1.5 A class, switched by the supervisor; no watch dongle protocol is implemented in keyboard firmware |

Regulator feedback uses the selected 0.1% resistors. For 5.1 V, 75k/10k with 0.6 V reference gives 5.1 V nominal; for 3.3 V, 45k/10k gives 3.3 V nominal. These are nominal calculations, not measured tolerances. The 4×47 µF bank is 188 µF nominal; the selected capacitor's DC-bias, temperature and tolerance must leave **at least 66 µF effective per buck bank**, following the TPS568230 recommended output-capacitance range. Do not replace with physically smaller high-bias-loss parts based on “47 µF” alone. Check the 1.0/1.5/2.2/4.7 µH Bourns inductors against actual ripple, peak/saturation, RMS and temperature rise; their schematic current labels are requirements, not proof of assembled-circuit current capability.

The input fuse is 3 A; battery fuse 10 A is for fault interruption, **not permission to draw 10 A continuously from a 6 A pack**. The firmware trips on sustained overcurrent and budgets below the pack's stated limit. Validate fuse interruption behaviour, MOSFET SOA, connector/trace/via heating and charger thermal dissipation with the real stackup and enclosure.

## Supervisor behaviour implemented in the supplied firmware

- Validate charger identity/cell configuration, battery voltage, NTC and status before enabling charge or loads. Defaults are disabled; failed readback or lost control bus disables discretionary outputs.
- Initialize the charger input limit at 100 mA. Unqualified/default-current input stays in HIZ with charging disabled. A confirmed USB-C source of at least 1.5 A, plus measured VBUS matching its contract, permits a 500 mA charger input limit. Only an accepted 20 V / 3 A result and measured 18–21 V input permits 2.3 A. The service hub draws independently; measure combined input current, inrush and HIZ reset-on-attach behavior before accepting any host/source class.
- Charge voltage is 8.4 V and request 1 A. A valid temperature/fault state is required; do not bypass NTC checks to make a prototype appear to charge.
- Core start from battery requires at least 6.4 V and more than 8% SOC; shutdown is requested at low pack voltage/SOC or faults. The CM4 acknowledges the current boot generation, then performs an orderly shutdown; the supervisor waits for the final halt handshake before removing the main rails.
- The high-PD budget is 36 W. Reserve 17 W for core/display/hub, then grant audio 3 W, RGB 3.5 W, external USB 7.65 W and watch 7.65 W in priority order. All loads together exceed this reservation, so **the watch can be shed when all other loads are requested**, even on PD. The screen distinguishes requested and granted power.
- Battery budget uses pack voltage ×5 A ×0.85 minus 0.8 W before reservations. This is a conservative allocation model, not a runtime guarantee. Do not raise it without measurements and engineering review. At lower charge levels, disconnect external charging loads before sacrificing orderly shutdown.

## Manufacturer engineering evidence required

Record rail min/max/ripple and brownout margin, input/pack current, inductor and connector temperatures, effective-capacitance evidence, charge behaviour with the selected pack, fault response, and shutdown timing. Validate the battery supplier's protection and charging documentation, including whether balancing is present/required. The selected pack's bounding dimensions and protection claim alone do not close that electrical review. Test the USB-C watch port with the owner's Samsung charging dongle; the exact watch/dongle SKU was not supplied.

Primary references: [BQ25798](https://www.ti.com/lit/ds/symlink/bq25798.pdf), [TPS568230](https://www.ti.com/lit/ds/symlink/tps568230.pdf), [AP63203](https://www.diodes.com/assets/Datasheets/AP63200-AP63201-AP63203-AP63205.pdf), [TPS2553](https://www.ti.com/lit/ds/symlink/tps2553.pdf), [TPS25810](https://www.ti.com/lit/ds/symlink/tps25810.pdf), [Bourns SRP7050TA](https://bourns.com/docs/product-datasheets/srp7050ta.pdf), [CM4 hardware](https://www.raspberrypi.com/documentation/computers/compute-module.html). The native schematics and firmware define this design's actual pin assignments and limits.

## Combined USB input

See [USB-C and headphones](../USB-C-AND-HEADPHONES.md) for input selection and programming. Verify AP63203 3.3 V hub regulation at minimum input through SS14, diode heating, SRP5030T-4R7M ripple current, and the effective capacitance/transient response of its 2 x 22 uF output bank.
