# Revision H audit: charging, programming and headphones

**Manufacturing HOLD; physical operation remains unverified.**

## External USB connections

J6, beside the OFF/ON switch, is **CHARGE / DATA / PROGRAM**. It carries USB 2 data to the internal hub as well as USB-C PD charging. J1 remains an alternative **PC DATA** input. J5 is the **WATCH POWER** output for the owner's Samsung USB-C charging dongle. The three USB-A ports belong to the selected external computer.

J1 has automatic data priority whenever its VBUS is present. To program through J6, unplug J1. To use a dedicated 20 V / 3 A PD charger and a separate computer simultaneously, connect the charger to J6 and the computer to J1. The upstream switch selects one data host; it does not combine hosts or connect their VBUS rails directly. Changing the selected input disconnects/reconnects the hub, so safely eject storage first.

A cable must carry USB data for programming. A charger with no data host provides charging only. USB-C charging from a computer requires a source advertising at least 1.5 A or an adequate explicit PD contract. The firmware disables battery charging on an unqualified/default-current source. Ordinary low-power USB does not guarantee net charging while the CM4, display, music or peripheral charging are running. Use a 20 V / 3 A fixed-PDO source for the full power budget. Default-current/legacy USB use requires a charged, installed battery; startup and reconnect current still need prototype measurement before that source class can be accepted.

## Programming through J6

1. Back up VIA settings and the CM4's private configuration. Eject USB storage. Unplug J1 and connect a verified data cable from the computer to J6. Use a qualified source and sufficient battery reserve.
2. **RP2040:** request QMK bootloader mode using a VIA-assigned Bootloader key. The supplied firmware requests an orderly CM4 shutdown and waits for its halt acknowledgment, with a 120-second fallback, before resetting. The independently powered hub stays supplied during RP2040 reset. Once the verified RP2040 ROM drive appears, copy `firmware/binaries/keyboard-rev-h-audit.uf2` to that drive. The native BOOTSEL/RESET buttons remain the recovery route; shut down the CM4 first because physical reset cannot wait for Linux. The SWD header remains available for initial bring-up or fault recovery.
3. **CM4 screen applications:** with the keyboard running, the same USB connection exposes a Linux serial console alongside stereo USB audio. Open that new serial device on the computer with a terminal supporting file transfer, and log in using the CM4 account/password. The target has `lrzsz` for ZMODEM upload/download (`rz` /`sz`); transfer application files, then restart the affected project service on the CM4. This is a normal authenticated Linux login. It does not expose a passwordless update shell or a network service to the computer.
4. **ESP32-C3:** upload replacement images and a reviewed matching manifest to the CM4. From its authenticated console, run `sudo python3 /opt/keyboard/rev-g/target/flash_esp32.py --bundle /path/to/verified/binaries`. For the shipped baseline, omit`--bundle`. The utility checks file hashes, exact offsets, controller telemetry and battery/PD reserve, stops the touchscreen service to release the serial link, uses the existing BOOT/RESET wiring, flashes the C3, resets it and restarts the service. Keep the OFF/ON switch on and power stable. The physical UART recovery header is still available if the C3 cannot communicate well enough for the guarded updater.
5. **CM4 eMMC recovery:** fitting carrier J9 and using the official `rpiboot` procedure exposes the CM4 storage through the hub. This is a service operation requiring the case to be opened; normal screen-code updates do not require it. Verify the target disk before imaging, then remove J9.

The packaged binaries were compiled and checked against fixtures. USB enumeration, recovery and programming on the assembled hardware are required acceptance tests, not completed physical measurements.

## Two simultaneous Bluetooth roles

The ESP32-C3 provides keyboard BLE HID to the selected computer. The CM4's separate Bluetooth radio provides stereo A2DP audio **from the keyboard to headphones**. The headphones do not occupy one of the three keyboard host slots.

On the touchscreen, open Settings, put the headphones in pairing mode and choose **Find headphones**, **Pair and connect**, then select their audio output. Already paired headphones have a **Connect** button; **Disconnect** retains their bond, while **Forget** removes it. Switching the output to **Keyboard speakers** returns playback to the internal speakers. Ordinary A2DP headphones are the target; headset microphone/call audio is not implemented.

Plexamp, key sounds and incoming PC USB audio share PipeWire. The Music tab remains available while macro pages and device controls are used. Plexamp can play with the PC off as long as the keyboard has power and its library server remains reachable. PC audio requires the USB data connection; the BLE keyboard link does not carry PC sound.

The target uses Raspberry Pi OS 64-bit Desktop **Trixie with WirePlumber 0.5 or later**, preserves CM4 Bluetooth, and assigns the ESP32 protocol to `/dev/serial0` with fixed core clock settings. Do not put a serial login console on that protocol UART. The login console above is USB`ttyGS0`.

## Hardware change and source basis

The USB board now uses TS3USB221A for data selection, AP63203 for a hub supply that survives keyboard reset, diode-isolated supply inputs and translated upstream-presence sensing. HUSB238 retains CC negotiation; its former DP/DM charging-detection pins are disconnected. The new parts and their models are included in the native KiCad project, BOM and populated board STEP. The enclosure openings and board mounting positions remain compatible.

The charger policy checks the [HUSB238 status registers](https://en.hynetek.com/uploadfiles/site/219/news/eb6cc420-847e-40ec-a352-a86fbeedd331.pdf) and measured VBUS before enabling charging. [BQ25798](https://www.ti.com/lit/ds/symlink/bq25798.pdf) HIZ is reapplied for unqualified sources and ADC sampling restored after HIZ entry. Adapter attachment can reset HIZ, so firmware checks alone do not establish compliant startup current. Measure complete hub-plus-charger current, attach/inrush, suspend and weak-source behavior on the prototype. See the acceptance record for the required combinations.
