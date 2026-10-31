# Internal computer and touchscreen deployment

The CM4 runs the touchscreen application and Plexamp independently of the attached PC. The touch application includes programmable macro pages and device controls; the Music tab does not replace those functions. Wi-Fi connects this internal Linux computer to the owner's network. The ESP32 handles the keyboard's separate Bluetooth host link.

The application and installer were checked with isolated simulation/fixtures. Audio hardware, device-tree overlays, radio operation and physical shutdown have not been exercised on a built keyboard. The manufacturer must complete `../assembly/ACCEPTANCE.md` on the final board stack.

## Hardware-free preview

From this folder, run in an isolated development directory:

```sh
python3 -B -m keyboard_ui.app --simulate --data ./preview-data
```

Open `http://127.0.0.1:8765` in a browser. Simulation does not open serial/GPIO or send Wi-Fi/IR commands. It saves only the chosen preview data. Stop the process when finished. The Music tab requires the separately installed Plexamp service; simulation does not install it.

To inspect exactly what target deployment files would be prepared:

```sh
python3 -B install_target.py --prepare-directory ./fresh-target-preview --desktop-user keyboard
```

The destination must be empty or new and must not resolve through symlinks. This command creates files inside that directory only; it does not download releases, install packages, enable services, change boot settings or flash a device.

## Provision the dedicated CM4

These steps belong on the **new keyboard CM4**, not on the owner's live computer. Use a current supported **64-bit Raspberry Pi OS Desktop Trixie, with WirePlumber 0.5 or later** image with KMS and the five-inch Touch Display 2 overlay. Record the exact image release/kernel/hash in the manufacturer acceptance record. The installer expects `/boot/firmware` and Python with `tarfile.data_filter` support (Python 3.12 or a maintained 3.11 version with that backport).

1. Power the carrier using the qualified prototype supply arrangement. Fit the carrier J9 recovery jumper only for eMMC programming and reset the CM4 into USB boot. Use the official Raspberry Pi `rpiboot` procedure and verify the target storage identity before writing the 32 GB eMMC. Remove J9 afterward. The carrier's internal USB link reaches the PC through the hub; if initial hub power is not yet qualified, use the carrier's documented USB debug connection under the hardware engineer's control. Do not connect two USB hosts simultaneously.
2. Create a normal desktop user, for example `keyboard`, with a private password and appropriate country/timezone/Wi-Fi settings. No credentials are included in this archive. Configure networking on the CM4 through Raspberry Pi OS. Use the KMS desktop configuration; FKMS is rejected by the installer.
3. Copy the package to the CM4, retaining `software/` beside `firmware/binaries/`, then enter `software/`. After the manufacturer has reviewed the target changes, run there, substituting the actual regular desktop username:

```sh
sudo python3 install_target.py --apply-on-cm4 --desktop-user keyboard
```

The script checks the CM4 model, ARM64 architecture, root permission, regular user home and boot configuration before package installation. It installs Python, PipeWire/WirePlumber, BlueZ, ALSA compatibility, USB file-transfer and Chromium dependencies, downloads pinned official Node 20/Plexamp releases with SHA256 verification, prepares project files under `/opt/keyboard`, backs up the affected initial boot/audio/desktop configuration under `/var/backups/keyboard-revg`, writes project services, and adds the user to audio/video/serial input groups. It enables the dedicated user's PipeWire session and Bluetooth audio, and enables the USB serial login console. This installer belongs only on the dedicated keyboard CM4. Services start after the next target reboot; Plexamp is left disabled until claimed.

4. Enable desktop autologin for this dedicated appliance using Raspberry Pi OS's configuration utility. Complete the Plexamp claim as the desktop user, following the official headless setup flow. From `/opt/keyboard/plexamp`, run `/opt/keyboard/node/bin/node js/index.js` interactively for initial setup; keep claim tokens out of logs and the handoff archive. Select the intended Plex library/server and the ALSA default output routed through PipeWire. Stop that interactive process after setup, then enable its project service:

```sh
sudo systemctl enable keyboard-plexamp.service
```

5. Reboot the CM4. Verify the local touch page at `http://127.0.0.1:8765` and the Plexamp player at`http://127.0.0.1:32500`. Confirm that only one Plexamp process runs. The owner needs a Plex account and a reachable library/server; any account/subscription requirements are governed by the current Plex product, not granted by this hardware package.

## Display, serial and audio configuration

`target/boot-config.txt` defines the five-inch DSI display, MAX98357A I2S, USB device controller, UART and final-halt GPIO. CM4 Bluetooth remains enabled for headphones. The ESP32 protocol uses the mini UART through `/dev/serial0`, with fixed core clock settings; no Linux login console may use that protocol UART.

The display connects to DSI1 at carrier J8. Carrier J5 supplies display power on pin 1 and ground on pin 3. The installer adds a separate boot include and rejects conflicting FKMS or explicit Bluetooth-disable settings.

The USB gadget exposes stereo UAC2 audio (48 kHz, 16-bit) and an authenticated serial console. It binds only while upstream VBUS is sensed. `keyboard-audio-loop.service` finds the UAC2 capture node using PipeWire device metadata and routes it to the selected default output. Plexamp and key sounds use that same PipeWire session. The target can select speakers or Bluetooth A2DP headphones in Settings. Confirm actual source/sink enumeration, clock adaptation, no-underrun mixing and radio coexistence on the assembled hardware.

[USB-C and headphones](../USB-C-AND-HEADPHONES.md) contains the port-selection rules, power requirements, update procedure and headphone controls. Both radios can operate independently: ESP32 BLE HID to the PC and CM4 stereo audio to headphones. USB PC audio is separate from BLE HID.

## Owner configuration and future coding

- **Macros:** use the touch editor to create pages/buttons, colour them and add shortcut, text, delay, bank, saved IR or Home Assistant steps. Export the macro configuration before updates. Text macros currently use printable ASCII with the US keyboard layout; OS layout changes require matching mapping work.
- **Device controls:** configure the owner's Home Assistant base URL and long-lived access token locally. Discover supported entity names through that authorized integration, then assign actions. Bulb brand/app credentials are supplied by the owner inside Home Assistant; arbitrary Wi-Fi devices are not automatically controllable just because they appear on a network.
- **IR:** learn from an existing 38 kHz remote or import a validated Pronto/NEC/raw code. Test while aiming the keyboard at the device; the save action requires the owner's explicit successful-response confirmation. No automatic visual response detector is installed.
- **Power:** the screen can request audio/RGB/USB/watch loads and shows the supervisor's granted state. Disable higher-priority discretionary loads if the watch output is shed. The OFF/ON switch requests a safe shutdown; charging may remain possible while the main keyboard is off.
- **Future applications:** edit `keyboard_ui/` for screen behaviour,`firmware/qmk/` for matrix/VIA/power policy and`firmware/esp32/` for BLE/IR/encoder logic. Back up persistent configuration and use the hardware-free tests before a controlled target update. Do not raise power/charge limits casually to support a new application.

Persistent target macro/IR/integration data is in `/var/lib/keyboard`; it contains private tokens when integrations are configured. The UI binds to localhost and checks mutation tokens; it is not an internet-facing control service. Backups of owner data must remain private and should not be included in manufacturer source archives. Linux OS/Plexamp credentials and pairing state are provisioned per unit.

## Failure diagnosis / return to baseline

Inspect the target journals for `keyboard-ui`, `keyboard-usb-audio`, `keyboard-audio-loop` and `keyboard-plexamp`. Check UART, power grants, display overlay and audio enumeration first. Keep credentials out of reports. Restore failed deployment from the target image or `/var/backups/keyboard-revg`; never run the installer on another PC. Return the commissioned image and private credentials securely.

Official references: [CM4/eMMC USB boot](https://www.raspberrypi.com/documentation/computers/compute-module.html), [Touch Display 2 setup](https://www.raspberrypi.com/documentation/accessories/touch-display-2.html), [Plexamp](https://www.plex.tv/plexamp/). Exact downloaded Node/Plexamp versions and SHA256 values are in `release-manifest.json`.
