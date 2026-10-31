# Programming and firmware sources — Rev H audit

**Manufacturing release on hold.**

The supplied RP2040/QMK and ESP32-C3 firmware **compiled successfully**. Their source, binary hashes and upstream versions are included. They have not been flashed or validated on a physical prototype; perform the bring-up/acceptance plan before connecting the real battery or peripherals.

`binaries/manifest.json` is the image authority. RP2040 uses the 16 MiB W25Q128JVSIQ (128 Mbit, not 128 MB). ESP32-C3-WROOM-02U-N4 has 4 MiB flash. The internal CM4 uses its 32 GB eMMC; no microSD slot is fitted.

## RP2040 keyboard

Image: `binaries/keyboard-rev-h-audit.uf2`. QMK target`personal/full_keyboard `, keymap`via`. The image includes matrix/RGB, four macro banks, VIA dynamic configuration, Bluetooth bridge, guarded macro taps, power supervision and shutdown sequencing.

Use a suitable 3.3 V SWD programmer on keyboard J1 for first controlled bring-up, or use BOOTSEL/RESET to enter the RP2040 ROM UF2 USB mode after the board has a qualified supply and the internal hub/USB connection is working. J1 pin 1=3.3 V reference, 2=GND, 3=SWDIO, 4=SWCLK, 5=RUN, 6=GND. Do not inject a second supply into an already powered board. For UF2, hold BOOTSEL while resetting, release when the RP2040 ROM drive appears, and copy the image to that **verified target drive only**. Do not write a raw firmware image to an arbitrary PC disk.

To rebuild in a separate development environment, install the QMK build prerequisites for your OS, extract `upstream/qmk-build-sources.tar.gz`, and build with `qmk compile -kb personal/full_keyboard -km via` from the extracted QMK tree. The pinned commit/submodule revisions are in the binary manifest. The `qmk/` folder is the standalone project source for edits. Copy its contents into`keyboards/personal/full_keyboard` when starting from a clean upstream checkout. Preserve the pinned revision and required ChibiOS/PicoSDK submodules.

VIA definition: `qmk/via-definition.json`. Connect through J1 PC DATA, or J6 CHARGE / DATA / PROGRAM with J1 unplugged, use VIA's design/import facility for this custom definition, then configure keys/banks/RGB. VIA over Bluetooth is not implemented. PrototypeVID/PID values must be replaced with a legitimate allocation before commercial distribution; update the VIA definition consistently.

BT1–BT3 short press selects a stored Bluetooth host; hold about 1.8 s to pair that slot. Fn+BT1 selects wired USB. Rotate the encoder in macro mode to select one of four macro banks; holding its button about 1 s changes between macro and touchscreen-selection modes. The 9 physical macro keys are read from the chosen VIA bank; default banks send F13–F21 with different modifiers.

## ESP32-C3 control/radio board

The build is pinned by `esp32/platformio.ini`: Espressif32 platform 6.12.0, Arduino framework 2.0.17 package revision, NimBLE-Arduino 2.5.0, ESP32-C3 target, DIO flash mode, 80 MHz flash, 4 MB flash. Rebuild with `pio run -d esp32` in a separate build environment. The module uses an external antenna; do not substitute a non-U module footprint.

Factory programming uses control-board J6 with a 3.3 V UART adapter: pin 2 GND; adapter RX receives pin 3 C3_TX; adapter TX drives pin 4 C3_RX. Pin 5 is EN and pin 6 BOOT_N. Pin 1 is 3.3 V reference/supply; leave the adapter's supply disconnected when the board is powered. Disconnect the CM4 serial harness or keep its TX inactive during this procedure. Hold BOOT low, pulse EN low then high, release BOOT after entering ROM download mode.

Use the selected adapter port explicitly. Example command, run **only with the prototype connected and the port verified** from `binaries/`:

```sh
python -m esptool --chip esp32c3 --port TARGET_UART_PORT --baud 115200 write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB 0x0000 esp32/bootloader.bin 0x8000 esp32/partitions.bin 0xe000 esp32/boot_app0.bin 0x10000 esp32/firmware.bin
```

The offsets come from this exact PlatformIO build's flash images/default partition map, recorded in the manifest. Reset EN after successful write/verify. Do not use the original ESP32's 0x1000 bootloader offset on an ESP32-C3. Reflashing the complete partition set may reset pairing/configuration; preserve owner data for upgrades and test the update process before deployment.

The ESP32 implements authenticated three-slot BLE HID, packet forwarding, rotary reading, battery-colour PWM and 38 kHz IR envelope learning/transmit. Raw imported IR carriers are bounded; the 38 kHz receiver does not measure arbitrary carrier frequency or capture RFID.

## CM4 and software

For updates through the existing charging port, see [USB-C and headphones](../USB-C-AND-HEADPHONES.md). The software bootloader path now waits for CM4 shutdown before resetting.

Provision the CM4 using `../software/README.md`. The keypad and radio microcontrollers remain independently programmable, while the CM4 screen is a normal Linux software environment for future applications. The existing touch UI is source code, not a fixed Plexamp-only appliance.

## Build/relink sources and licensing

Custom QMK/ESP32 files carrying SPDX GPL-2.0-or-later retain that licence. The bundled QMK build source archive includes the target, core/build files and used ChibiOS/PicoSDK/printf source trees with their existing licences. The ESP32 source archive includes NimBLE, Arduino core/libraries/variants and relinkable project objects. PlatformIO downloads the pinned compiler/framework SDK; its upstream package source/version is in the manifest. Keep all third-party licence notices when redistributing binaries or derivative sources. Obtain/update the matching upstream SDK source as its licence requires when shipping a modified binary; do not treat the keyboard's product files as a transfer of third-party rights.

Plexamp and Node archives are **not redistributed** here; the target installer fetches their pinned official releases and verifies SHA256 from `../software/release-manifest.json`. Account credentials, Wi-Fi passwords and Home Assistant tokens are never supplied in this archive.
