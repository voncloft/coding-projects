# Bundled 3D models

These model files travel with all four projects: `keyboard`, `stereo-audio`, `usb-io` and `power`. Keep their sibling `models` directory in place. Model references use `${KIPRJMOD}/../models/`; the matching project-local footprints carry the same settings as their PCB instances.

KiCad package/connector and foostan switch/socket/LED source geometry was copied unchanged; rotations and offsets are in the footprints. Missing package shapes, VH headers, the inductor family and the slide switch use simplified dimensioned models created for this project, identified in `generated-models.json`. Those are mechanical viewing aids, not exact manufacturer-certified models. The USB receptacles have the actual bundled KiCad connector STEP models.

Across the complete eight-board system, 829 populated positions carry 937 resolving model references. Switch positions include multiple assembled parts; mounting-hole footprints intentionally have no component model. This shared directory serves the four projects listed above; the other projects also bundle their required models. Native exports verified that referenced STEP files resolve; the enclosure checks cover nominal fit. Exact purchased-part, contact, cable and tolerance qualification remains necessary. See `sources.json`, `generated-models.json`, `LICENSE-KiCad.md` and `LICENSE-foostan.txt`.
