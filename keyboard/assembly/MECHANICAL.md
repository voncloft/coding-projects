# Mechanical manufacture and assembly

Use the supplied native FreeCAD assembly and STEP solids. Logical layout X runs right and Y runs toward the front; in FreeCAD, Y is negative toward the front. Z is up. PCB placements are in `../layout-dimensions.json`; board STEP origins are local, while enclosure part STEP files retain assembly coordinates. Do not centre every imported file independently before assembling.

## Material and dimensional baseline

| Part | Quantity | Material / process | Requirement |
|---|---:|---|---|
| Lower shell | 1 | Industrial SLS PA12, dyed black | Full 584.55 mm width in one piece; quote machine capacity. No unreviewed segmentation. Remove powder from passages and tap pilot holes after manufacture. |
| Top frame | 1 | SLS PA12, black | 3 mm nominal cover; 2 mm remaining above rotary cable pocket. |
| Switch plate | 1 | 1.5 mm 5052-H32 or 6061 aluminium, laser/waterjet/CNC | Deburr and flatten. Switch openings 14.00 mm, target +0.10/−0.00; validate the selected Cherry switch latch on a coupon. |
| Plate spacers | 9 | PA12 or machined insulating nylon | 3.4 mm high, OD 5, bore 2.2; `PlateSpacer0` through`PlateSpacer8`. |
| CM4 spacers | 4 | Insulating nylon | 3.0 mm high, OD 4.8, bore 2.7; do not confuse with 1.5 mm CM4 connector versions. |
| Speaker straps | 2 | PA12 | 2 mm thick; frame retention only, no cone load. |
| Rotary knob | 1 | PA12 or machined polymer | D-shaped bore; 20 mm OD, 12 mm high. Friction fit; do not drill away the flat. |
| Battery indicator light pipe | 1 | Clear PMMA or clear optical resin | Polish stem; diffuse visible 2.9 mm tip. Do not glue against LED packages. |
| CM4 heatsink | 1 | 6061-T6 aluminium | `cm4-heatsink.step`: 24×24×6.5, 1.5 mm base, eight 1 mm fins. Deburr; base flatness 0.10. Add a 0.5 mm insulating thermal pad, total 7 mm. |
| Feet | 4 | Silicone rubber, adhesive backed | Diameter 12, height 4. Individual STEP references supplied; purchased equivalent permitted. |

Case outline is 584.55 ×153.70 mm, with the display pod extending to 166.50 mm. Body plus cover reaches Z42; keys and knob project higher. Use ±0.30 mm general polymer prototype tolerance, but control PCB mounting centres to ±0.15 mm and mating heights to ±0.15 mm. The manufacturer must account for SLS shrinkage/warpage; these values are functional acceptance targets, not a claim that every printer meets them without compensation.

Plate thickness, support heights and PCB thickness are a stack: main PCB underside Z32.5, PCB top34.1, spacer3.4, plate underside37.5, top39.0. Changing any of these affects switch latching/travel. Main PCB is 1.6 mm. The 0.4 mm display-ribbon passage is especially sensitive: inspect the actual supplied FFC thickness, fold radius and latch engagement in a fit coupon before the full shell/cover build.

The custom heatsink fits inside the checked 24×24×7 allowance, including its pad. It does not establish cooling performance. Align its base to the CM4 processor lid, avoid adjacent parts, and verify interface contact without flexing the module. Preserve the underside ventilation openings and orient the fins to permit air circulation.

## Hardware baseline

`hardware.csv` lists the fastening groups. Screws are stainless pan-head machine screws with small washers where stated. Thread the polymer pilots with controlled-depth taps after manufacture; do not install heat-set inserts into the existing pilot geometry. If the manufacturer prefers inserts for repeated servicing, return a revised boss design and fresh fit checks first.

| Joint | Quantity | Baseline screw | Stack / engagement target |
|---|---:|---|---|
| Cover to shell | See hardware.csv | M3×8 | 3 mm cover +0.5 mm washer; about 4.5 mm thread engagement |
| Main PCB + plate spacers + plate to posts | 9 | M2×10 | 6.5 mm hard stack; about 3.5 mm engagement; retain insulating PCB washers if screw head otherwise contacts copper |
| USB I/O PCB | 5 | M2.5×6 | 1.6 mm PCB; about 4.4 mm engagement |
| Power PCB | 4 | M2.5×5 | 1.6 mm PCB; about 3.4 mm engagement; no longer screw into bottom floor |
| CM4 carrier PCB | 4 | M2×4 | 1.6 mm PCB; about 2.4 mm engagement |
| Audio PCB | 4 | M2×4 | 1.6 mm PCB; about 2.4 mm engagement |
| Control PCB | 4 | M2×5 | 1.6 mm PCB; about 3.4 mm engagement |
| Rotary PCB | 4 | M2×5 | 1.6 mm PCB; about 3.4 mm engagement |
| Battery-light PCB | 2 | M2×5 | 1.6 mm PCB; about 3.4 mm engagement |
| Speaker straps | 4 | M2×5 | 2 mm strap, about 3 mm engagement; add compliant frame gasket without crushing speaker |
| CM4 module to carrier | 4 | M2.5×10, washer/nut | 3 mm spacers, PCB/module stack; verify thread protrusion clear of enclosure |
| Display bosses from underside | 4 | M2.5×30 with washer adjustment | Through full-height hollow support; target 2–3 mm boss engagement, never bottom screw in display insert. Verify actual boss face/depth on the purchased screen. |

Screw lengths above are nominal assembly selections. At the fit sample, check actual head diameter, washers, threaded insert depth and thread engagement with a depth gauge. Stop if a screw bottoms or a PCB bows. Set assembly torque by the selected polymer's thread pull-out test; there is no validated torque value yet. Do not use an arbitrary steel-joint torque on printed posts.

## Assembly sequence

1. Inspect the shell for flatness, clear all passages, tap pilots to the measured safe depths, and trial-fit a blank PCB/plate coupon with the selected switches/stabilizers. Check the real USB plugs, ON/OFF actuator and display cable latch.
2. Fit keyboard stabilizers to the main PCB before installing its plate. Use seven 2u and one 6.25u Cherry-pattern PCB-mount stabilizer assemblies. Numpad Plus and Enter are vertical. Verify clearance through 4 mm travel; do not substitute different stabilizer hole patterns without revising the CAD.
3. Install power, USB I/O, audio, control, CM4 carrier, rotary and battery-light PCBs onto their labelled supports. The board-side copper/mounting-hole clearances are in KiCad; no screw may touch an exposed signal pad or trace.
4. Seat the CM4 into both 3 mm Hirose sockets with even pressure and no twist, fit its four spacers/fasteners, then fit the thermal pad/heatsink. The two connector-body overlaps in the CAD report are intentionally mated sockets, not an enclosure interference.
5. Mount speakers on thin soft frame gaskets and retain with the two straps. Leave the cones and vent openings free. Insulate both speaker leads: each amplifier output is bridge-tied; neither speaker terminal is ground.
6. Lay in the checked harnesses before the main keyboard PCB and display obstruct access. Follow `WIRING.md`. Fit the pack with soft foam/strap retention inside the 129×76×15 allowance; protect it from screw ends and sharp edges. Do not compress or puncture the pouch. Affix and insulate the NTC on the pack surface. Leave the battery disconnected until electrical bring-up authorizes it.
7. Fit display fasteners from the case underside, using washers to limit engagement. Use its supplied power lead by net assignment and its 22-to-15-way FFC. Route FFC beside the display backplate, then horizontally into the connector, following the CAD access allocations. Do not make the former direct vertical crossing through the backplate.
8. Fit the main PCB, nine 3.4 mm spacers and plate. Insert 108 switches squarely while supporting each hot-swap socket from behind; then fit the custom keycaps. Ordinary 1u caps are about 18.05 mm wide on 19.05 mm centres. Follow the actual key-width list, including both Shift keys and the 6.25u space bar.
9. Fit the light pipe above the indicator LEDs with an air gap; wires from this board exit toward the front. CM4 J10 wires exit toward the rear. Route rotary wires under the dedicated cover pocket. Fit cover and knob; require 0.8 mm knob press travel without cover contact and full key travel without rubbing.
10. Complete the acceptance checks with the enclosure closed. Fit four feet, record hardware/assembly deviations and return the final CAD if any dimensions changed.

The full FreeCAD assembly contains reference electronics and hidden cable allocation solids. Manufacture only the case/mechanical parts listed here. `keyboard-enclosure.step` is a combined mechanical reference, not an additional part to order on top of the individual STEP files.
