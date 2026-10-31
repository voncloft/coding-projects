# Enclosure files

Open `keyboard-enclosure.FCStd` in FreeCAD. This is the assembled native review, including all eight populated PCBs and the corrected CM4 model.

Mechanical STEP exports: `lower-shell.step`, `top-frame.step`, `switch-plate.step`, `speaker-straps.step`, `plate-spacers.step`, `rotary-knob.step`, and `indicator-light-pipe.step`. The spacers file includes both plate and CM4 spacers. `keyboard-enclosure.step` combines the case pieces, spacers, knob, straps and feet; the clear light pipe is provided separately.

The electronics and cable allocations are reference geometry, not case parts to manufacture. Cable allocations are normally hidden; select the named Allocation features in FreeCAD to inspect them. Read `../CHECK-RESULTS.md` for what passed and what still needs a physical prototype.

`cm4-heatsink.FCStd` and `cm4-heatsink.step` define the custom 24×24×6.5 mm aluminum sink. Add the specified 0.5 mm insulating thermal pad to remain inside the 7 mm assembly allowance. `PlateSpacer0..8.step`, `CM4Spacer0..3.step` and `RubberFoot0..3.step` are individual parts extracted from the checked assembly. They are alternatives to extracting those solids from the combined reference, not additional quantities.

Material, tolerances, screw sizes/counts and assembly sequence are in `../assembly/MECHANICAL.md`. Screw engagement, thermal performance and exact FFC bending require the manufacturer's physical prototype tests.
