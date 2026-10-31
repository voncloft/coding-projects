# Rev H audit inter-board harnesses

Pin numbers below refer to PCB pads. Match housing pin numbers to the board drawing; never assume a premade cable is straight-through. All 15 harnesses are checked against current PCB nets.

Use JST GHR-xxV-S housings with SSHL-002T-P0.2 contacts for GH, and VHR-xN housings with SVH-21T-P1.1 contacts for VH. Main/audio 2.54 mm headers need matching 2.54 mm female housings and crimps. GH power contacts remain limited to their rated current; the key RGB budget is approximately 0.65 A at the firmware brightness cap.

Keep USB D+/D− in short 90-ohm shielded cable pairs with continuous ground. Do not substitute loose hookup-wire pairs for validated USB cabling. Cable lengths and bend clearance are part of the enclosure check; high-speed operation requires prototype testing.

## H01: usb-io J9 to power J1

PD input; VH 2-pin at both ends

| From pin | Net | To pin | Net |
|---:|---|---:|---|
| 1 | PD_IN | 1 | PD_IN |
| 2 | GND | 2 | GND |

## H02: power J2 to usb-io J10

Core and external USB rails; VH 4-pin at both ends

| From pin | Net | To pin | Net |
|---:|---|---:|---|
| 1 | +5V_CORE | 1 | +5V_CORE |
| 2 | GND | 2 | GND |
| 3 | +5V_IO | 3 | +5V_IO |
| 4 | GND | 4 | GND |

## H03: power J3 to usb-io J11

Controls; GH 8-pin at both ends

| From pin | Net | To pin | Net |
|---:|---|---:|---|
| 1 | +3V3_AON | 1 | +3V3_AON |
| 2 | GND | 2 | GND |
| 3 | I2C_SDA | 3 | I2C_SDA |
| 4 | I2C_SCL | 4 | I2C_SCL |
| 5 | POWER_REQUEST | 5 | POWER_REQUEST |
| 6 | EXT_USB_ENABLE | 6 | EXT_USB_ENABLE |
| 7 | WATCH_ENABLE | 7 | WATCH_ENABLE |
| 8 | WATCH_FAULT_N | 8 | WATCH_FAULT_N |

## H04: usb-io J7 to keyboard J2

USB pair; GH 3-pin to existing 2.54 mm header

| From pin | Net | To pin | Net |
|---:|---|---:|---|
| 1 | DN1_DP | 1 | HUB_KB_DP |
| 2 | DN1_DM | 2 | HUB_KB_DM |
| 3 | GND | 3 | GND |

## H05: power J4 to keyboard J3

Keyboard power and control; GH 8-pin to existing 2.54 mm header

| From pin | Net | To pin | Net |
|---:|---|---:|---|
| 1 | +3V3_AON | 1 | +3V3_AON |
| 2 | GND | 2 | GND |
| 3 | +5V_RGB | 3 | +5V_RGB |
| 4 | GND | 4 | GND |
| 5 | POWER_REQUEST | 5 | POWER_REQUEST |
| 6 | MAIN_ENABLE | 6 | MAIN_ENABLE |
| 7 | POWER_IRQ | 7 | POWER_IRQ |
| 8 | GND | 8 | GND |

## H06: power J5 to control J2

I2C and always-on supply, GH 4 at both ends

| From pin | Net | To pin | Net |
|---:|---|---:|---|
| 1 | I2C_SDA | 1 | PWR_SDA |
| 2 | I2C_SCL | 2 | PWR_SCL |
| 3 | +3V3_AON | 3 | +3V3_AON |
| 4 | GND | 4 | GND |

## H07: power J7 to stereo-audio J3

Audio power; VH 2-pin to positions 1–2 of existing 7-pin header

| From pin | Net | To pin | Net |
|---:|---|---:|---|
| 1 | +3V3_AUDIO | 1 | +3V3_AUDIO |
| 2 | GND | 2 | GND |

## H08: control J1 to keyboard J4

Bridge UART, I2C and AON; GH 6 to main 2.54 mm 6-pin housing

| From pin | Net | To pin | Net |
|---:|---|---:|---|
| 1 | BRIDGE_TX | 1 | BRIDGE_TX |
| 2 | BRIDGE_RX | 2 | BRIDGE_RX |
| 3 | PWR_SDA | 3 | I2C_SDA |
| 4 | PWR_SCL | 4 | I2C_SCL |
| 5 | +3V3_AON | 5 | +3V3_AON |
| 6 | GND | 6 | GND |

## H09: control J3 to cm4-carrier J7

CM4 UART, recovery and final-halt; GH 6 at both ends

| From pin | Net | To pin | Net |
|---:|---|---:|---|
| 1 | C3_RX | 1 | C3_RX |
| 2 | C3_TX | 2 | C3_TX |
| 3 | C3_EN | 3 | C3_EN |
| 4 | C3_BOOT_N | 4 | C3_BOOT_N |
| 5 | CM4_HALTED_N | 5 | CM4_HALTED_N |
| 6 | GND | 6 | GND |

## H10: control J4 to rotary J1

Rotary inputs and supply; GH 5 at both ends

| From pin | Net | To pin | Net |
|---:|---|---:|---|
| 1 | +3V3_AON | 1 | +3V3_AON |
| 2 | GND | 2 | GND |
| 3 | ENC_A | 3 | ENC_A |
| 4 | ENC_B | 4 | ENC_B |
| 5 | ENC_SW | 5 | ENC_SW |

## H11: control J5 to battery-light J1

Battery indicator red/green/GND; GH 3 at both ends

| From pin | Net | To pin | Net |
|---:|---|---:|---|
| 1 | RED_PWM | 1 | RED_PWM |
| 2 | GREEN_PWM | 2 | GREEN_PWM |
| 3 | GND | 3 | GND |

## H12: control J7 to cm4-carrier J10

Always-on isolated UART supply; GH 2 at both ends

| From pin | Net | To pin | Net |
|---:|---|---:|---|
| 1 | +3V3_AON | 1 | +3V3_AON |
| 2 | GND | 2 | GND |

## H13: power J6 to cm4-carrier J3

CM4 and display core supply; VH 2 at both ends

| From pin | Net | To pin | Net |
|---:|---|---:|---|
| 1 | +5V_CORE | 1 | +5V_CORE |
| 2 | GND | 2 | GND |

## H14: usb-io J8 to cm4-carrier J4

CM4 USB audio/recovery; GH 4, shielded 90-ohm USB pair

| From pin | Net | To pin | Net |
|---:|---|---:|---|
| 1 | CM4_VBUS | 1 | CM4_VBUS |
| 2 | DN2_DM | 2 | USB_DM |
| 3 | DN2_DP | 3 | USB_DP |
| 4 | GND | 4 | GND |

## H15: cm4-carrier J6 to stereo-audio J3

I2S/control; GH 5 to audio 2.54 mm pins 3-7; shares a 7-way audio housing with H07

| From pin | Net | To pin | Net |
|---:|---|---:|---|
| 1 | I2S_BCLK_R | 3 | CLK_IN |
| 2 | I2S_LRCLK_R | 4 | LR_IN |
| 3 | I2S_DATA_R | 5 | DATA_IN |
| 4 | AMP_ENABLE | 6 | AMP_ENABLE |
| 5 | GND | 7 | GND |

## External connections

- Power J8: pin 1 BAT_PACK positive, pin 2 GND. Selected protected pack: BatterySpace CU-JAS503, 7.4 V 5 Ah, 6 A maximum, 129 × 76 × 15 mm. Add the specified external NTC; the pack listing does not claim balancing.
- Power J9: pin 1 BATT_TS, pin 2 GND. Semitec 103AT-2 10 kΩ NTC bonded to the cells with insulating tape.
- CM4 carrier J5: pin 1 +5V_DISPLAY, pin 2 unused, pin 3 GND. Use the Touch Display 2 supplied power lead in this orientation.
- CM4 carrier J8: 22-way DSI socket. Use the supplied 22-to-15-way display ribbon with contacts oriented to the socket drawing.
- Audio J1/J2: speaker +/−. Neither speaker terminal is ground. Selected drivers: PUI AS04008CO-R, 8 Ω, 40 × 20 × 5.8 mm.
- Keyboard J1: SWD/debug only. Keyboard J5: spare RGB expansion; the status indicator uses its separate two-LED board.
- Control J6: 3.3 V UART/recovery. Do not attach a 5 V TTL serial adapter.
- Three external USB-A ports belong to the connected PC. They do not become wireless USB ports or CM4-hosted storage.
- The top OFF/ON switch requests shutdown; the charge controller and always-on circuits remain connected to the battery. Disconnect the pack for service.
