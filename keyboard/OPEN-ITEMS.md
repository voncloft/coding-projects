# Revision H open items - manufacturing HOLD

Updated 21 September 2026. An empty DRC report is not an electrical or product qualification. No replacement manufacturer ZIP is approved. Revision G must not be built. This list separates unfinished design review from measurements that need an actual prototype.

## Design review before fabrication

| Item | Current evidence | Required closure |
|---|---|---|
| Actual PCB stackup and signal integrity | Explicit native stackups; USB/DSI width corrections; connected-pair lengths checked; ideal cross-section estimates and filled-plane sampling recorded | Supplier must approve actual materials/pressed thickness/copper/etch and 90/100-ohm construction. Review the recorded connector/via reference interruptions, coupling changes, transitions and cables. Return any required native routing changes before fabrication. A local ideal estimate is not a qualified full-channel simulation. |
| Capacitors, ripple and regulator margins | AON regulator replaced by AP63203 2 A; current pin mappings checked; nominal capacitor banks identified | Obtain exact-part DC-bias/temperature/tolerance evidence and verify effective capacitance, ripple-current and load-step requirements. In particular, prove the three 4 x 47 uF TPS568230 output banks remain at least 66 uF effective. Input ripple, stability and transient review remains open. Nominal capacitance alone does not close it. |
| USB input startup and inrush | Diode-isolated hub supply, firmware charge qualification, PD sensing and current-limit fault fixtures | Review the complete input network, including USB-I/O C14/C30/C50, regulator startup and power-board C1/C2/C15 behind the PD FET. Establish cold-attach/default-current/suspend compliance and whether input limiting or a capacitance change is needed. Firmware HIZ policy does not limit every attach transient. |
| Battery and fault protection | Selected protected 2S CU-JAS503 pack, NTC interface, charger/gauge fixtures and load budget | Obtain the pack manufacturer's charge/protection/balancing/temperature/transport documentation and approve charger settings, fuses, connector ratings and fault interruption. The selected pack's dimensions and protection description are insufficient evidence. |
| Complete electrical design signoff | Eight clean native rule/parity/ERC checks; 100 selected IC pin checks; 12 hub strap/reset/bias resistor checks | Complete all remaining datasheet pin/rating, worst-case current, return-loop, tolerance, thermal and power-sequence review. Selected pin checks are not an audit of every component or an independent electrical signoff. |
| Purchased parts and enclosure process | 829 positions reconciled; current 937 model references; nominal CAD fit checks | Confirm purchased MPNs and physical dimensions, tolerances, connector plug bodies, switch/stabilizer/keycap samples, FFC thickness/folds, materials, shrinkage and screw engagement. Simplified model envelopes are not supplier-certified metrology. |

## Prototype evidence still absent

All 26 rows in `assembly/acceptance-record.csv` remain NOT TESTED. There is no manufactured keyboard here. Required measurements include power/charge/thermal/fault performance; USB data and programming through both inputs; actual display and macro operation; Bluetooth keyboard plus headphones; speakers/audio; IR/Wi-Fi integration; runtime; and assembled mechanical fit.

These tests cannot truthfully be passed by adding more desktop fixtures. The firmware and software are built/tested against the documented fixtures only. The owner's live system has not been used as a test target.

## Release gate

After the design issues above are closed, regenerate affected native checks, exports, BOM, models and test evidence. A specialist may then fabricate a controlled engineering prototype under the accepted bring-up plan. Production or an assertion that the entire product is fully tested requires the measured acceptance record and disposition of failures. A final release ZIP must contain only the mutually consistent approved revision, an inventory and verified hashes. No current archive is claimed to satisfy that gate.
