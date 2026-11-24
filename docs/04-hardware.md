# Hardware

## 1 Block diagram

```
                 ┌──────────────────────────────────────────┐
                 │        ESP32-S3-DevKitC-1 N16R8          │
                 │                                          │
   Hall marks  ──┤ GPIO4  (ISR)        I²C ──┬── LCD 4×20   │
   Hall index  ──┤ GPIO5  (ISR)              │   (PCF8574)  │
   Hall wheel  ──┤ GPIO6  (ISR)              │   SDA8/SCL9  │
   Motor Hall  ──┤ GPIO7  (ISR)              └── (free)     │
   HX711 DOUT  ──┤ GPIO15                                   │
   HX711 SCK   ──┤ GPIO16          UART1 ───── controller   │
                 │                 (TX18/RX21)              │
   E-stop       ─┤ GPIO2  (in)                              │
   feedback      │                                          │
                 │                 BLE ──── pressure sensor │
                 └──────────────────────────────────────────┘

   Power path (galvanically isolated from the logic side):

   Power supply 36 V ──[E-stop]──[fuse]── motor controller ── hub motor
```

## 2 Mechanical layout

The drum sits on its **own, co-rotating shaft**, not on the motor rotor
(decision A10). The motor is rigidly connected to the shaft end via a flange;
only its **stator axle** is held, via the lever arm against the load cell.
Dimension chain and bearing calculation in [08-stand](08-stand.md) §1 and §4.

```
          Test wheel (liftable)
                 ↓ F_N = 417 N (42.5 kg)
        ╔════════╧════════╗     Drum C = 2.842 m (measured), clamped to the shaft
        ║      Drum       ║     at both end discs
  ──┬───╫──┬───────────┬──╫───┬──────  Shaft co-rotating, stepped Ø15/Ø30
    │   ╚══╪═══════════╪══╝   │
  6902     Clamp hubs        6902      Flange on the shaft end
                                        │
                                   Hub motor, rotor rigidly co-rotating
                                        │
                              Stator axle, held only at the lever arm
                                        │
                              L = 0.084 m → shift cable → load cell 5 kg, calibration mark 0.330 m
                                        + end stops at r = 200 mm
```

The rotor-stator friction cancels exactly as long as the stator axle is held **only**
via the lever arm — derivation in [08-stand](08-stand.md) §7.2. That is why the tension
member is a shift cable and not a rod: it transmits no bending moment. The two
pillow-block bearings of the drum shaft, by contrast, lie **outside** the cradle: their
friction is part of the idle-loss curve and is subtracted with it ([09-measurement-theory](09-measurement-theory.md)
§1.6).

**No gearing.** The motor turns at about 480 rpm; the drum needs 341 rpm for
60 km/h; a belt would lie outside the torque path and enter with η ≈ 0.97 as a
2–3 % scale error. `BELT_RATIO` stays in `config.h` in case one becomes
necessary after all.

Using the motor as a friction roller against the drum surface is not an option: it would
need 1393 rpm, three times its rated speed.

## 3 Parts list

### On hand

| Part | Data |
|---|---|
| Load cell | **5 kg**, bending beam |
| Amplifier | HX711 breakout |
| Display | HD44780 4×20 characters |
| Motor driver | SimpleFOCShield v2 — only for commissioning, too weak for the full build |
| Vials | block levels 15.1 × 15.1 × 40 mm, for aligning the profiles; clip §7.8 |

### To be procured

| Part | Requirement | Status |
|---|---|---|
| Hub motor | **Hoverboard drive wheel 6.5″, 36 V / 350 W** | **procured** |
| Motor controller | **FSESC 4.20 / 50 A** (VESC class), key data and connections in §10 | **procured** |
| 3 × automotive changeover relays 12 V/40 A | short-circuit brake, §9 | in cart |
| 2N5551, 1N4007, resistors | low-side switch of the relay chain, §9 | assortment procured |
| NTC for the stator axle | only if the 6th wire at the Hall connector is not a sensor (A5) | open |
| Mating connector Julet 9-pin | or cut off the connector and fit own connectors | open |

| Power supply | 36–42 V, ≥ 10 A | open |
| XT60E-M panel-mount connector (Amass) | supply lead from the power supply at the frame; holder §7.6 | holder designed 2026-11-22 |
| ESP32 board | **ESP32-S3-DevKitC-1 N16R8** | procured |
| USB power supply | 5 V / ≥ 1 A, for measurement operation (not the notebook) | open |
| Five-way tactile switch | navigation button, 5 contacts + common ground (e.g. ALPS SKQU); **no** analog joystick breakouts | open |
| HX711 module | **separate VCC/VDD** (five-pin, SparkFun form factor) — four-pin modules are ruled out, see §5.2 | open |
| Hall sensors | 3 × TLE4905L (marks, index, wheel), unipolar | on hand |
| Magnets | 8 + 1, Ø10 × 3 mm with Ø3 countersunk hole | on hand |
| Power supply | 36 V, ≥ 10 A (better 15 A) — reserve for the current limit when starting | open |
| Brake resistor + chopper | **dropped** — braking is done by short circuit, §9 | – |
| Pillow-block bearings for the stator axle | 2 pieces, on the cradle frame | open |
| Timing belt | **dropped with direct drive** — only needed if A6 turns out negative | – |
| Emergency stop | positively opening, disconnects the power path | open |
| Second HX711 | same form factor, for the later wheel-load measurement | later |
| BLE pressure sensor | **SKS AirSpy** at the valve, protocol from `~/git/airspy` | on hand |

## 4 Pin assignment

Board: **ESP32-S3-DevKitC-1**, module WROOM-1 **N16R8** (16 MB flash, 8 MB octal PSRAM).
Reconciled with `firmware/src/config.h` and `firmware/platformio.ini`.

The authoritative source is `hardware/schaltplan.py` — it also wires the peripherals
that the firmware does not yet touch.

| GPIO | Constant | Function | Direction | Note |
|---|---|---|---|---|
| 2 | `PIN_ESTOP_FB` | emergency-stop feedback | in, pull-up | **display** only; the disconnection happens in hardware. **Not yet wired** (`ESTOP_FB_WIRED` = false); until then the firmware detects the emergency stop from the missing motor current ([07-safety](07-safety.md) §4) |
| 4 | `PIN_MARK` | speed marks drum | in, interrupt | **8** marks/revolution |
| 5 | `PIN_INDEX` | index mark drum | in, interrupt | 1/revolution, triggers the averaging |
| 6 | `PIN_WHEEL` | speed of the test wheel | in, interrupt | **1** magnet/revolution (`WHEEL_MARKS_PER_REV`); provides the effective wheel circumference (§14.1), later the wheel spin-down test |
| 7 | `PIN_MOTOR_HALL` | motor Hall (one phase) | in, interrupt | also the high-resolution speed source |
| 8 | `PIN_I2C_SDA` | I²C SDA | bidir | S3 default assignment; free for the IR sensor |
| 9 | `PIN_I2C_SCL` | I²C SCL | out | ditto |
| 10 | `PIN_SD_CS` | microSD, chip select | out | remaining SPI pins still **TBD** |
| ~~11~~ | ~~`PIN_BTN_UP`~~ | ~~D-pad up~~ | — | **free since 2026-11-22** |
| ~~12~~ | ~~`PIN_BTN_DOWN`~~ | ~~D-pad down~~ | — | free |
| ~~13~~ | ~~`PIN_BTN_IN`~~ | ~~D-pad in~~ | — | free |
| ~~14~~ | ~~`PIN_BTN_OUT`~~ | ~~D-pad out~~ | — | free |
| 15 | `PIN_HX711_DOUT` | HX711 #1 DOUT (torque) | in | |
| 16 | `PIN_HX711_SCK` | HX711 SCK | out | **shared clock of both HX711** |
| 17 | `PIN_HX7112_DOUT` | HX711 #2 DOUT (wheel load) | in | second cell not yet fitted |
| 18 | `PIN_DRIVE_TX` | UART1 TX → controller | out | |
| 21 | `PIN_DRIVE_RX` | UART1 RX ← controller | in | |
| ~~39~~ | ~~`PIN_BTN_OK`~~ | ~~D-pad center~~ | — | **free since 2026-11-22** |
| 40 | `PIN_BRAKE_RELEASE` | MOSFET in the coil ground of the brake relays | out | **high = relays on = brake released.** Pull-down at the gate: braked on reset |
| 48 | `PIN_STATUS_LED` | status LED | out | on DevKitC-1 **v1.0** this is the RGB LED, on v1.1 it is on GPIO 38 |

### Which GPIOs are blocked on this board

The earlier assignment (25/26/27/32/33/34/35/22) came from the classic ESP32 and on
the S3 fell almost entirely on pins that either do not exist or are occupied:

| GPIO | why blocked |
|---|---|
| 22…25 | do not exist on the ESP32-S3 |
| 26…32 | SPI flash |
| 33…37 | octal PSRAM — the **R8** in the module name |
| 19, 20 | USB D−/D+ |
| 0, 3, 45, 46 | strapping pins |
| 43, 44 | UART0, serial console |
| 38 | RGB LED of the DevKit |

That leaves **GPIO 1, 41, 42 and 47** free. The microSD still needs three of them
(SCK/MOSI/MISO, CS is on GPIO 10) — after that GPIO 1 is the only free pin. If
anything else is added later, it has to go over the I²C bus, which is already there anyway.

GPIO 1 became free because the motor sensor is connected to the controller and not to the ESP32
(§11.4). If something does need to be measured in analog after all: only **ADC1** (GPIO 1…10)
is usable, ADC2 is occupied by the radio while WiFi is on. GPIO 1 is ADC1_CH0
and therefore the right pin for it.

That GPIO 39…42 are the JTAG pins (MTCK/MTDO/MTDI/MTMS) only costs an **external**
JTAG adapter. The on-chip USB-Serial-JTAG of the S3 runs over the native USB socket and
remains usable.

Display: `LCD_I2C_ADDR` = 0x27 (**TBD**, usual alternative 0x3F), `LCD_COLS` = 20,
`LCD_ROWS` = 4.

## 5 Power supply and logic levels

### 5.1 Supply via USB

The logic side is powered **via USB-C**, not via the 5V pin. Espressif calls the
three paths *mutually exclusive*
([User Guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html)).

> **Correction of 2026-11-22.** This section used to say that an externally fed 5 V and a
> simultaneously connected notebook would be "a short circuit of two sources". **That is
> wrong**, and demonstrably so: in the
> [board schematic](https://dl.espressif.com/dl/schematics/SCH_ESP32-S3-DevKitC-1_V1.1_20220413.pdf)
> (V1.1, sheet 2) there is a Schottky diode to `VCC_5V` behind **each** USB socket —
> **D1** at `J2` (COM) and **D7** at `J4` (native), both 1N5819HW-7-F. The 5V pin is
> `J1` pin 21 and hangs **directly on `VCC_5V`**, i.e. behind the diodes.
>
> Three things follow:
>
> 1. **Back-feeding into the notebook is impossible.** The diodes conduct only from USB to the
>    board. The computer's USB port is protected by design.
> 2. **Two sources do not short each other, they are OR-ed.** Whichever is higher
>    feeds. A USB port at 5.0 V arrives behind the Schottky at ≈ 4.7 V — a
>    clean 5.0 V supply at the pin therefore wins and blocks the USB diodes.
> 3. **What you lose is the protection.** There is no diode in front of the 5V pin any more:
>    reverse polarity or overvoltage go unchecked into the LDO and onto everything hanging on
>    `VCC_5V`.
>
> That is why the supply stays on USB — not because the alternative blows up, but
> because the USB path rules out the reverse-polarity fault by design.

The board has two USB-C sockets, and **both may feed at the same time**
("either one or both"). So both cables may stay plugged in permanently; nothing has to
be unplugged for flashing.

**Which cable goes where — and not arbitrarily:**

| Socket | Cable | why |
|---|---|---|
| `COM` (UART bridge) | **notebook**, only when needed | esptool flashes there without a button press, and **only there does the serial console come out** |
| `USB` (native) | **power supply**, permanent | feeds along, otherwise not needed |

The other way round does not work: at the native socket the download mode would have to be
set by hand, and the console would stay silent.

The two sockets are however **not equivalent**, even though they are for the
supply:

| Socket | Path into the chip | for what |
|---|---|---|
| `USB` | **native USB** of the S3 via GPIO 19/20, directly at the chip | USB-Serial-JTAG, USB-CDC, debugging without an external adapter |
| `COM` | **USB-UART bridge** to UART0 (GPIO 43/44) | flashing and serial console, reset via DTR/RTS |

Practical difference when flashing: the bridge at `COM` pulls the EN and GPIO 0
lines via DTR and RTS, so it can reset the chip into download mode **by itself**.
The native socket can only do this as long as the running firmware brings the
necessary reset handling — if it does not, esptool stalls with *"No serial
data received"*, and the download mode must be set by hand:
**hold BOOT, tap RESET, release BOOT.**

Second reason to plug the notebook into `COM`: **the serial console is there.**
The PlatformIO board definition sets `ARDUINO_USB_MODE=1`, but **not**
`ARDUINO_USB_CDC_ON_BOOT`. So Arduino's `Serial` is UART0 on GPIO 43/44, i.e.
the `COM` socket — nothing would come out at the native socket after flashing. Anyone who
wants the console there anyway sets `-DARDUINO_USB_CDC_ON_BOOT=1` in
`platformio.ini`; then it moves along, and the reset handling comes with it.

> Checked on 2026-11-15: at the native socket the board reports as
> `0x303A:0x4001` ("Espressif Device", serial number `123456`, composite device). The
> board definition expects `0x303A:0x1001` there, i.e. the chip's USB-Serial-JTAG.
> So there is a firmware on the board that occupies the native socket itself as USB-CDC
> — and esptool cannot get into download mode over it.
>
> At `COM` sits a **WCH CH343** (`0x1A86:0x55D3`, "USB Single Serial"), under macOS
> without an additional driver as `/dev/cu.usbmodem*`. Flashed over this socket on
> 2026-11-15: ran through without any button press, "Hard resetting via RTS pin",
> console reports at 115200 Bd.

With USB supply the **5V pin is an output** (`VCC_5V`, i.e. VBUS minus
diode voltage). Both rails thus come from the same source:

| Rail | Origin | supplies |
|---|---|---|
| 5 V | 5V pin = VBUS | Hall sensors including their pull-ups, HX711 **VCC**, LCD |
| 3.3 V | 3V3 pin, onboard LDO from VBUS | HX711 **VDD** |

Because the 3.3 V regulator runs from the same 5 V, there is **no sequencing problem**:
the rails come and go together. Only with separate sources would VDD have to be present before
VCC, otherwise the HX711's input protection diodes drive current into the
digital section.

| Consumer | Current |
|---|---|
| ESP32-S3 with WiFi | ≈ 100 mA, peak ≈ 350 mA |
| LCD 2004 with backlight | 25…40 mA |
| 3 × TLE4905L | ≈ 21 mA |
| HX711 | 1.4 mA |

Around 150 mA on average. A 1 A supply is sufficient three times over. For measurement
operation use a dedicated power supply, not the notebook — a sleep state in the middle of a run
costs the measurement.

### 5.2 Logic levels: the ESP32-S3 is not 5 V tolerant

| Component | Supply | Signal to the ESP32 |
|---|---|---|
| TLE4905L | V_S = 5 V (minimum 3.8 V) | open collector, **pull-up 470 Ω to 5 V and Zener diode 3.3 V to ground at the ESP32** → ≈ 3.2 V, cathode to the signal. Soldering and connection template: `hardware/hallplan.pdf` |
| HX711 | **VCC = 5 V** (analog), **VDD = 3.3 V** (digital) | DOUT at 3.3 V |
| ~~LCD / PCF8574~~ | — | **dropped since 2026-11-22**, operation is via WiFi |

**Spec: HX711 module with separate VCC and VDD pins** (SparkFun form factor,
five pins: VDD, VCC, DAT, CLK, GND). VCC feeds the bridge, VDD sets the logic level.
At the chip these are AVDD (pin 3) and DVDD (pin 16).

The cheap four-pin modules (VCC, GND, DT, SCK) are thus ruled out — there both
are connected. The alternative would be to run the whole chip at 3.3 V. That
costs **no** resolution in counts, because the bridge is fed from AVDD and AVDD
is at the same time the ADC reference, so the measurement is ratiometric. But it does cost
**SNR**: one third less excitation at the same input noise is about 3.6 dB.
With 12 N of signal from the best racing tire on a 98 N cell, that is not to be
given away.

### 5.3 Where the HX711 sits

**At the cell cable, not at the ESP32.** The cell signal is the sensitive part of the
chain, the digital signal the robust one — so the long run is placed on the
digital side.

| | |
|---|---|
| Full scale of the 5 kg cell at 1 mV/V and 5 V excitation | 5 mV |
| Design case including ramp (31.4 N) | 3.2 mV |
| **ΔCrr = 0.0005 (1.22 N)** | **124 µV** |

On a line that is supposed to resolve 124 µV, every coupled-in disturbance is directly a
measurement error — and the PGA amplifies it by 128.

The real damage is not noise but **EMC rectification**: the
controller's PWM couples at a few tens of kHz into the high-impedance PGA inputs and
rectifies there into a **DC component** that moves with the motor current.
That looks exactly like signal and does not show up in the idle-loss curve, because
it occurs there just the same.

The often-cited reason — lead resistance with the four-wire cell — is by contrast
**no** argument: 2 m of stranded wire are about 0.5 Ω in the excitation loop; with 1 kΩ
bridge resistance and 10 K temperature swing that gives 0.002 % span error.

In practice:

- HX711 **within about 10 cm** of the cell, in a small shielded
  housing on the frame — not on the cradle part, otherwise its cable pulls on the lever arm.
- **Not** near the motor or in its exhaust air, despite the short run to the cell:
  offset drift.
- Cell cable as **two twisted pairs** (E+/E−, S+/S−), overall shield, shield
  connected at one end to the cell body.
- At the HX711 input 100 Ω in series each and 100 nF differentially across INA+/INA−.
- The digital run twisted or shielded, **routed separately from the cell cable**,
  and 100 Ω in series in SCK directly at the ESP32 — edge-slowing, so that the clock does not
  couple back into the analog side.

### 5.4 Connectors

**The cell lead gets no connector.** It is only 10 cm long and goes
directly into the **screw terminal on the HX711 board**. A screw terminal is a
gas-tight clamped connection without additional metal junctions — electrically the best
there is here, and a screwdriver is enough to swap the cell.

The **digital side** is plugged. Recommendation: **JST-XH, 6-pin** — keyed,
crimpable, cheap. Six poles instead of five, so that every signal has a ground next to
it:

| Pin | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|---|
| | 5 V | GND | SCK | DOUT | GND | 3.3 V |

Two things decide here, not signal quality:

- **Reverse-polarity protection.** 5 V and 3.3 V are on the same connector. A twisted or
  one-pin-offset connector destroys either the HX711 or the ESP32. The
  form factor must be positively keyed — hence JST and not a pin header. The
  order above additionally puts 5 V and 3.3 V at the two ends.
- **Vibration.** The connector sits on the test stand. Cable ties as strain relief, or
  a latching form factor right away (Molex Micro-Fit 3.0).

Cable for it: **LiYCY 6×0.14 mm²** — shielded, six-core, and 0.14 mm² ≈ 26 AWG
is right in the middle of the crimp range of the XH contacts.

With six cores on six positions the shield has **no pin of its own**. It is
brought out as a short pigtail and connected to ground **only at the ESP32 end**, left
open at the HX711: grounded at one end, so no ground loop parallel to the
GND core. If that is not enough against the controller interference, the second end is added.

For lengths over 1 m or if the cable runs past the controller: **shielded
RJ45 patch cable** (S/FTP). Eight cores as four twisted pairs, each signal with
its own return, shield to the housing, latching, available as a ready-made cable in any
length. Electrically far better than any ribbon cable.

**Not** necessary is effort on account of the signals themselves: 3.3 V CMOS at a maximum of 1 MHz
over one meter is uncritical. The HX711 however has **no checksum** — a
flipped bit delivers a wildly wrong value. The firmware should discard outliers
instead of relying on the line.

Reason: every connector introduces two metal junctions per core into the signal path,
and at µV levels the **thermoelectric voltage** matters. It cancels only as long as both
contacts of a pair are built the same and are equally warm.

| Junction | Thermoelectric voltage |
|---|---|
| copper–gold | ≈ 0.3 µV/K |
| copper–solder | 1…3 µV/K |
| copper–nickel (worn-through gold plating) | ≈ 20 µV/K |

For comparison: 1 % of the signal of a good racing tire is **16 µV**. A
worn-through contact with a 2 K gradient across the pair is at 40 µV — i.e.
2.5 % error, and a systematic one at that.

If one does have to go into the analog run after all:

- **S+ and S− on adjacent pins**, E+ and E− likewise — then both contacts are
  equally warm and the thermoelectric voltages cancel.
- Contacts **gold-plated**, not tinned.
- Shield to the **metal housing** of the connector, use a metal hood.
- Do not place in a temperature gradient, i.e. not near the motor.
- **Never pin headers or Dupont cables** in the analog path.
- Analog and digital cores **not through the same connector**; if it cannot be
  avoided, place ground cores between them.

### 5.5 Measurement aspects

- **Power the HX711 separately.** Not from the motor branch. Ferrite and 100 µF directly at the
  AVDD pin, star ground.
- **Check the RATE pin.** `HX711_RATE_SPS` = 80 presupposes RATE on DVDD. Many
  modules tie pin 15 permanently to GND, which is 10 SPS — at 60 km/h only **1.8 samples
  per revolution** instead of 14, with which the revolution-synchronous averaging
  ([09-measurement-theory](09-measurement-theory.md) §3.8) collapses. If necessary cut the trace
  and bridge to DVDD.
- **Motor leads over the pivot bearing as a soft loop.** On the hub motor they
  exit axially through the stator axle, i.e. on the pivot axis — the advantage is
  lost if the cable leaves taut.
- **Connect logic and power ground at exactly one point.**

### 5.6 Bringing the LCD to 3.3 V levels

> **Moot since 2026-11-22**: D-pad and LCD are dropped, operation is
> via WiFi on the phone ([10-firmware](10-firmware.md) §3.1). The section stays
> in place because the trade-off will be needed again as soon as an I²C device with 5 V
> is added — the IR temperature sensor is a candidate.

Backpack (PCF8574) and HD44780 module run on **5 V**. SDA and SCL are open drain:
the high level is made not by the chip but by the **pull-up** — and on the
backpack sit 4.7 kΩ to **5 V**. Exactly those must not go to the S3.

| Path | Intervention | Assessment |
|---|---|---|
| **a) Level shifter** | two MOSFETs (BSS138) or a ready-made module between bus and ESP32 | within specification, nothing to change on the module; costs one component and two plug points |
| **b) Move the pull-ups** | unsolder the 4.7 kΩ on the backpack, instead 2.2…4.7 kΩ to **3.3 V** at the ESP32 | least effort, but **outside the specification** |
| **c) LCD at 3.3 V** | on the backpack a voltage regulator is **not fitted**; some capacitors and resistors would be added (visual inspection of the existing board) | cleanest level, but assembly work — and the contrast of the HD44780 at 3.3 V has to be checked |

Why (b) is "not quite clean": the PCF8574 requires as high level
**0.7 · V_DD**, at 5 V supply therefore **3.5 V**. A bus that is pulled up only to 3.3 V
lies below that.

- **Nothing breaks in the process.** The dangerous direction would be the reverse — 5 V at the
  S3 —, and that is exactly what is gone after unsoldering.
- Whether the chip reads the 3.3 V **reliably** as high, the datasheet does not
  guarantee. That is decided by the specimen and the temperature. In practice
  it often works; it is not an assurance.
- **A marginal high level cannot be improved afterwards.** Smaller pull-ups
  only make the edges steeper, not the level higher; 100 kHz instead of 400 kHz
  likewise. If the bus acts up, only (a) or (c) remain.

**Order for (b): unsolder first, then connect.** As long as the 4.7 kΩ to
5 V hang on the bus, 5 V is present at GPIO 8 and 9.

Decision open, see [06-open-questions](06-open-questions.md) B3.

## 6 Power requirement and power supply

At 60 km/h (ω = 35.73 rad/s), wheel load 42.5 kg. Torques including idle losses and
wheel losses (about 0.51 Nm, [08-stand](08-stand.md) §5.1):

| Case | Torque | P mech | Current at kt = 0.568 Nm/A |
|---|---|---|---|
| best racing tire (Crr 0.0025) | 1.00 Nm | 36 W | 1.8 A |
| good racing tire (0.0037) | 1.24 Nm | 44 W | 2.2 A |
| Contact Urban (0.0055) | 1.59 Nm | 57 W | 2.8 A |
| **Design case (Crr 0.007)** | **1.88 Nm** | **67 W** | **3.3 A** |
| + starting at the end of the ramp | 2.51 Nm | 90 W | 4.4 A |
| *slowest MTB (0.0127) — not intended* | *2.98 Nm* | *106 W* | *5.2 A* |

Electrically about **90 W, peak 120 W**. The motor's rated current is about
9.7 A — in the design case therefore 34 % of it is used. With the change from the
MTB to the racing-tire design the drive is clearly oversized.

Power supply **36 V / 10 A** suffices; 15 A give headroom so that the current limit when starting
comes from the controller and not from the power supply.

**Mind regeneration:** During regenerative braking energy flows back. A
lab power supply cannot absorb it, the DC-link voltage rises and the controller
shuts down with overvoltage. Solutions: brake resistor with chopper (1.4 kJ over 10 s
is 140 W, a 100 W resistor suffices) or a battery as buffer. There is no mechanical
brake ([07-safety](07-safety.md) §3).

## 7 Printed parts

Two housings following the same pattern: §7.1–§7.3 the sensor housing for the
Hall sensors, §7.4 the housing for the load-cell board. Common to both are the
cavity open at the bottom, the board seat made of ledge plus locating keys and the
cable groove, which clamps the jacket against the mounting surface.

In addition §7.5, the mounting clamp for the ESP housing — not a housing but
a holder.

**Sensor housing for the Hall sensors** — stationary side of the
speed sensing, on the frame. Its counterpart is §8.

3D print, outer dimensions **37.2 × 20.0 × 12.0 mm**, 5.6 cm³.

`hall-gehaeuse.FCStd` is a **PartDesign body with feature tree** and a
spreadsheet `Parameter`; all sketches are fully constrained and bound to the
table via expressions. Dimensions are changed **in the table**, not in the sketches.

| | |
|---|---|
| Feature tree | Grundkoerper · Kavitaet · Bauteilraum · **Auflagefasen · Kabelrampe** · Rastnasen · Federfeld · Schraubloch · Kabelnut (+rund) · Kabelmulde (+rund) · LED_Loch |
| `hardware/hall_gehaeuse.py` | builds the tree from scratch, exports `.step` and `.stl` |

Whoever works in the GUI maintains the FCStd — the script overwrites it. It is the
generator, not the truth.

### 7.1 Dimensions

| Feature | Dimension |
|---|---|
| Board | perfboard 18 × 13 × 1.6 mm, 2.54 pitch, 7 × 5 holes |
| Cavity | 20 × 14 × 10 mm, open at the bottom |
| Solder space under the board | 2.0 mm |
| Component space above the board | 6.4 mm |
| End wall in front of the sensor | 1.2 mm |
| Side walls / lid | 3.0 / 2.0 mm |
| M6 through-hole | Ø 6.6, offset 4.5 mm from center |
| Cable channel | D-slot 4.4 wide, 3.4 deep (cable Ø 4, three-core) |
| Cable recess in front of the board | same groove, 5.2 deep, x = 20…26 mm |
| LED hole | Ø 3.2 in the lid, 15 mm from the end wall |

End wall 1.2 mm because it enters the magnet air gap (§8.3). Groove 3.4 mm for a
4 mm cable so that the sheet clamps the jacket — that is the strain relief.

The jacket does **not** fit under the board; there are only 2.0 mm there. It ends in the
recess, which at 5.2 mm is deeper than the cable is thick — nothing is clamped there and
nothing is pressed against the board edge. Only the three cores run on. Clamping
happens only in the solid block behind it.

### 7.2 Board seat

| Feature | Dimension |
|---|---|
| Ledge | 1.8 mm protrusion per side, **continuous 20 mm**; bearing surface 1.3 mm |
| Lower edge of the ledge | z = 3.6 mm, inside with **0.4 mm chamfer** |
| Cable ramp at the cable end | 4 mm long, opens to **1.5 mm** above the board's top edge |
| Locating keys | 0.65 mm protrusion, 6 mm long, **center only**; overlap 0.15 mm |
| Shape of the key | symmetrical wedge, apex z = 2.0 mm |
| Spring field, relieved from outside | 12 mm long, 0.8 mm remaining wall |

Two stops with separate tasks:

- The **ledge** sits deep in the cavity and prevents the board from sliding
  further in.
- The **locating keys** sit below it and prevent it from falling out toward the open
  side.

The keys have **no undercut** — the slope is the same above and below;
holding is by preload. The board comes out again without tools.

Keys only in the center, because the 3 mm thick wall only springs at all in the relieved
field. The ledge by contrast continuous; it does not have to spring but to carry.

> The outermost perfboard strip rests on the ledge and must remain free of solder joints:
> fit components within ±5.2 mm around the board center.

#### So that the board does not pinch the cores

The cable groove begins **3 mm inside** the cavity and thus runs under the
ledge. There the board's top edge and the ledge meet, both at
z = 3.6 mm, and as a 90-degree edge. A core lying there is sheared off when the
board is pushed in. Two measures, both on 2026-11-21:

1. **Chamfer 0.4 mm** on the inner lower edge of the ledge, all round. The
   sharp edge becomes a wedge: a stray core is pushed inward
   instead of being crushed.
2. **Cable ramp**: over the last 4 mm before the cable end the underside
   of the ledge rises by 1.5 mm. Exactly there the core leaves the board and dives
   into the groove; the gap opens wedge-shaped instead of closing.

The same applies to the HX711 housing, §7.4 — there it is the same geometry for
the same reason.

### 7.3 Mounting

Open side onto the aluminium sheet, one M6 screw. From this follows:

1. The sheet closes the housing and clamps the cable in the groove.
2. The board has 2.0 mm clearance to the sheet. Adhesive tape as insulation is thus
   a fallback, not a prerequisite.
3. One screw means rotatable — the alignment to the drum mark is set
   while tightening.

### 7.4 Housing for the load-cell board (HX711)

Takes the SparkFun board *HX711 Load Cell Amp*, which sits on the cradle frame next to
the load cell. Outer dimensions **49.2 × 28.2 × 12.3 mm**, 9.3 cm³.

| | |
|---|---|
| Feature tree | Grundkoerper · Kavitaet · Bauteilraum · **Eckauflagen · Auflagefasen · Kabelrampe** · Rastnasen · Federfeld · Schraubloch · Kabelhals · Kabelnut · Kabelmulde (+rund) · Maulfasen |
| `hardware/hx711_gehaeuse.py` | builds the tree from scratch, exports `.step` and `.stl` |

Structure, spreadsheet and caveat as above: the script is the generator, the FCStd
the truth.

#### Dimensions

| Feature | Dimension |
|---|---|
| Board | **30.6 × 23.0 mm** (re-measured; nominal 30.48 × 22.86 = 1.2 × 0.9″), 1.6 mm thick, **4.3 mm with components** |
| Cavity | 31.2 × 23.4 × 10.3 mm, open at the bottom |
| Board clearance | **0.6 mm lengthwise, 0.4 mm crosswise, 0.3 mm in thickness** — below, *Fit* |
| Solder space under the board | 2.0 mm |
| Component space above the board | 6.7 mm — 2.7 mm components, **4.0 mm for the cores** |
| End wall at the cell end | 2.0 mm |
| Side walls / lid | **2.4** / 2.0 mm — reasoning below |
| M6 through-hole | Ø 6.6, **centered**, 3.00 / 3.50 mm web to the cable recesses |
| Cable channel +y, lead to the computer | **omega slot** Ø 3.9, neck 3.2, 3.4 deep |
| Cable channel −y, strands of the load cell | **omega slot** Ø 2.9, neck 2.4, 2.6 deep |
| Channel centers | y = ±8.6 mm |
| Remaining wall next to a channel | 3.20 mm (+y) / 3.70 mm (−y) |
| Cable recess at the transition | Ø **4.6** / **3.6** (wider than the cable), 5.2 / 4.2 deep, 5 mm long |
| Clamping length behind it | 11 mm |

#### Fit

The clearance stands as a **separate quantity** in the table (`fit_w`, `fit_l`), not as
an allowance in the formula. Previously it read `cav_w = pcb_w + 1` — taken over from the
sensor housing, where the board is 13 mm wide. At 23 mm, 1.1 mm of clearance is no longer
clearance but wobble.

| | before | now |
|---|---|---|
| Clearance crosswise | 1.1 mm | **0.4 mm** (0.2 per side) |
| Clearance lengthwise | 2.0 mm | **0.6 mm** (0.3 per end) |
| Bearing on the ledge | 1.3 mm | **1.60 mm** per long side — asymmetrical since 2026-11-21, see below |
| Component zone around the center | ±10.25 mm | −8.90 / **+10.90 mm**, in the corners ±8.90 |

0.2 mm per side is a sliding fit that an FDM print also hits; less
would be pressing. The board goes in from below, not lengthwise — so the lengthwise clearance
only absorbs tolerance, not the insertion.

> The price: the component zone has become narrower. The solder pads lie on the
> short sides and are not affected — the **capacitors are**, see
> the next section.

#### The ledge broke off two capacitors

**What happened.** During the first assembly the two large capacitors
on one long side of the board caught on the ledge and broke off.
Also, the board could not be snapped in: it was too thick for the locating keys.

**Why.** Both were built into the model, not the print:

1. The ledge ran **all round** at 1.8 mm and thus reached 1.8 mm over the
   board edge. According to the SparkFun Eagle data
   ([repo](https://github.com/sparkfun/HX711-Load-Cell-Amplifier),
   `hardware/SparkFun_HX711_Load_Cell.brd`, outline 30.48 × 22.86 mm), on
   **one** long edge there are two 10 µF capacitors in 1210 package (C6, C7), center
   2.54 mm from the edge, **package up to 1.24 mm from the edge**; between them C3 and
   C5 (0603) up to 2.04 mm. On the other long edge a strip of
   **3.95 mm** is free — the nearest component is R4 or C1.
2. Between the apex of the locating keys and the lower edge of the ledge there were
   **exactly 1.6 mm**, i.e. the nominal thickness of the board without any air. A real
   board with its thickness tolerance does not fit in there.

**What was changed.**

| | before | now |
|---|---|---|
| Ledge free long side (−y) | 1.8 mm | **2.8 mm** |
| Ledge capacitor side (+y) | 1.8 mm | **0.8 mm** between the corners |
| Ledge in the corners (+y) | 1.8 mm | **2.8 mm** over 5 mm length each |
| Apex of the locating keys | z = 2.00 mm | **z = 1.70 mm** (`fit_t` = 0.3) |

The corners work because along the capacitor edge the first copper only begins at
x = 6.22 mm (pad of C3) and the last ends at 24.68 mm (pad of C6).
Four wide corners hold the board better than a continuous 0.8 mm strip.

> **The board now only fits in one orientation**, and that is intentional:
> the wrong way round the capacitors hit the wide corner seats before anything
> breaks. The capacitor side faces **+y** in the housing.

Recalculated on the model, worst-case position of the board within its clearance: 0.44 mm of air
between ledge and 1210 package, 0.92 mm between corner seat and the pad of
C6, 1.22 mm between corner seat and the pad of C3.

> **Not measured is the height of the capacitors.** Eagle carries no heights.
> The component space above the board is, at 6.7 mm, so ample that this does not
> bind here — for a 1210 package 1.7 to 2.0 mm is usual.

#### Why two channels on one side

The solder pads of the board lie on the two **short sides** — on the left the five
connections of the load cell, on the right VDD/VCC/DAT/CLK/GND to the computer. Both
leads should exit on the same side, so the cores run from the pads
upward, across the board and together to the screw block. From this follow
two dimensions that are not arbitrary:

1. The 4.0 mm above the components are the path of the cores, not reserve.
2. The load-bearing ledge runs along the **long sides**. On the short sides it would
   sit on the solder joints.

The screw stands centered between the channels: one screw clamps both
jackets, and in the area where the surface lies closest.

#### The two channels are different sizes

Not the same thing runs through them. On the **+y** channel lies the ready-made
lead to the computer, 4.0 mm jacket, re-measured. On the **−y** channel only the
**four thin strands of the load cell** arrive, bundled with heat-shrink tubing.
One channel for both would be too wide for the bundle, and then nothing clamps — the
strain relief depends on the slot being 0.1 mm narrower than what goes into it.

**3.0 mm for the bundle is estimated, not measured.** The calculation behind it:

| | |
|---|---|
| 4 cores AWG 28…30, insulated | 0.7 … 1.0 mm per core |
| square bundle, circumscribing circle | (1 + √2) · d = **1.7 … 2.4 mm** |
| thin-walled 2:1 heat-shrink tubing, shrunk | + 0.4 … 0.5 mm per side |
| **Total** | **2.5 … 3.4 mm** |

Before printing, measure with the caliper and adjust `cab_cable_m`; the
three dependent dimensions (`cab_d_m`, `cab_mouth_m`, `cab_depth_m`) follow the
table. A bundle is compliant: 0.2 mm too tight only compresses it, 0.5 mm
too wide means it is no longer held.

**The clamping is gentler at the −y channel**: the slot lies 0.4 mm below the bundle,
not 0.6 as with the jacket. Thin strands in heat-shrink tubing are soft, and
0.6 mm would flatten them instead of holding them.

#### So that the board does not pinch the cores

Both channels run toward the ledge, and the **corner seat at the channel end** lies
exactly in the path of the cores. There the board's top edge and the ledge meet,
both at z = 3.6 mm, as a 90-degree edge — a core in between is sheared off when the
board is pushed in. Two measures:

1. **Chamfer 0.4 mm** on the inner lower edge of the ledge, all round. The
   sharp edge becomes a wedge: the core is pushed inward instead of
   crushed. 0.4 mm also fits on the narrow 0.8 mm strip of the
   capacitor side and leaves 0.4 mm of bearing surface there.
2. **Cable ramp**: over the last 4 mm before the channel end the underside
   of the ledge rises by 1.5 mm — measured on the model, the gap opens to
   1.43 mm. Exactly there the cores dive into the channels.

The same applies to the Hall housing, §7.2.

#### Why 2.4 mm side wall and 2.0 mm lid

**2.4 mm with a 0.4 nozzle is exactly six passes.** A wall made of a whole
number of passes needs no fill seam in the middle — that is the place where
printed walls tear open. 3.0 mm would be 7.5 passes.

**The lid stays at 2.0 mm.** It spans the cavity unsupported, and an
unsupported ceiling sags visibly when thinner. Savings are made where material only
carries, not where it bridges.

**The remaining wall at the spring field stays 0.8 mm**, because `relief_t` is given as an absolute
and not as a fraction of the wall. The locating keys therefore spring unchanged; only the relief
is shallower at 1.6 instead of 2.2 mm.

#### Board seat

| Feature | Dimension |
|---|---|
| Ledge, free long side (−y) | 2.8 mm protrusion, continuous |
| Ledge, capacitor side (+y) | **0.8 mm** between the corners, **2.8 mm in the corners** over 5 mm length each |
| Lower edge of the ledge | z = 3.6 mm, inside with **0.4 mm chamfer** |
| Cable ramp at the channel end | 4 mm long, opens to **1.43 mm** above the board's top edge |
| Locating keys | 0.65 mm protrusion, 6 mm long, center only; apex at **z = 1.70 mm** |
| Space between key apex and ledge | **1.90 mm** = 1.6 board + 0.3 `fit_t` |
| Spring field, relieved from outside | 14 mm long, 0.8 mm remaining wall |

The same division as in §7.2: the ledge carries, the keys hold against
falling out, no undercut, held by preload.

The four mounting holes of the board remain unused — they lie in the
corners and thus under the ledge.

#### Omega slot: neck narrower than the circle

The cable is **Ø 4.0 mm** (measured). The slot is therefore no longer a U: above
a **neck of 3.2 mm** sits a **circle of Ø 3.9**, which protrudes 0.35 mm per side
beyond the neck. This undercut is the claw.

| Location | Width |
|---|---|
| Neck, at the open side | **3.20 mm** |
| at the height of the circle center (z = 1.45) | 3.88 mm |
| Undercut per side | **0.35 mm** |
| Total slot depth | 3.40 mm |
| Chamfer at the mouth | 0.3 mm — below, *The chamfer at the mouth* |

**Two things hold the cable, and in sequence:**

1. **During assembly** the claw. The cable is pushed through the 0.8 mm narrower
   neck and then does not fall out again while the board is
   inserted and the housing is turned over. Previously exactly that happened.
2. **Afterwards the mounting surface.** The slot is 3.4 deep, the cable 4.0 thick —
   0.6 mm clamping, as in §7.1. That is the strain relief.

> **Why not simply cut into the circle**, i.e. place its center so high
> that the opening itself becomes narrower? Because then mouth width and slot depth
> would be coupled: a mouth of 3.2 would need a slot only 2.9 mm deep, and so
> the cable would be squeezed by the surface by 1.1 mm instead of 0.6. Guiding neck and circle
> separately makes both dimensions independent.

In the **recess** at the transition the slot at Ø 4.6 is deliberately 0.6 mm **wider**
than the cable — there it bends away and must not be squeezed.

#### The chamfer at the mouth

The cable has to be **pushed** through the narrower neck, and without a chamfer the
sharp edge bites into the jacket. Four small chamfers of **0.3 mm**, two per channel,
make the entry wider without touching the round shape.

| z | Width |
|---|---|
| 0.01 — open side, with chamfer | **3.76 mm** |
| 0.35 — end of chamfer | 3.22 mm |
| 1.45 — circle center | 3.89 mm |

**The claw remains unchanged at 0.35 mm per side.** The chamfer sits *below* the
undercut and takes nothing away from it — it only widens the opening through
which the cable has to pass the same claw.

> There lies a hard limit. The circle only emerges from the neck at
> z = `cab_depth` − R − √(R² − (neck/2)²) = **0.335 mm**; the chamfer may reach
> up to there, not further. At 0.3, 0.035 mm remain. The script
> calculates the limit itself and **aborts** if a changed slot
> violates it — a chamfer into the undercut would quietly
> remove the claw.

If it is still too tight, `cab_mouth` is the place: a wider neck
lets the cable in more easily **and** creates more room for the chamfer — but
costs claw. At neck 3.4 it would be 0.25 mm undercut and 0.495 mm to the
onset.

#### Mounting

Open side onto the mounting surface, one M6 screw. The surface closes the
housing and clamps both cables in their slots. The jacket does not fit under the
board; it ends in the recess and only the cores run on.

An M6 washer (DIN 125, Ø 12) reaches to 6.0 mm from the center and thus stays
0.7 mm short of the cable recess — tight, but clear. Larger washers press on it.

### 7.5 Mounting clamp for the ESP housing

The **ESP housing** takes the ESP32, possibly the display and the
wiring; primarily the ESP. It is **not part of this repo** — an
existing housing. Documented here is only what the clamp needs to know
about it:

| Feature | Dimension |
|---|---|
| Holes per end face | **two, Ø 4.5** at the edge |
| Center-to-center distance | **50.0 mm** (measured on the housing, 2026-11-18) |
| Hole center to outer edge | **6.2 mm** |
| End faces | two |

Two clamps needed, one per end face. **Only one is designed**: it is
symmetrical about the M6 axis and fits both ends.

Outer dimensions **61.3 × 26.4 × 4.0 mm**, 4.8 cm³ (version of 2026-11-23).

| | |
|---|---|
| Feature tree | Platte · M4_Gewinde · M6_Durchgang · M6_Senkung · Verrundung |
| `hardware/esp_spange.py` | builds the tree from scratch, exports `.step` and `.stl` |

#### Three holes in a triangle

```
     o-----------------o     two M4 threads directly in the plate,
      \               /      50.0 mm apart
       \             /
        \           /        contour converges toward the center
         \  +---+  /
          \ |(O)| /          one M6, centered, offset 12.7 mm
           -+---+-            outward, head in a counterbore
```

| Feature | Dimension |
|---|---|
| Plate | **4.0 mm** thick like the emergency-stop clamp, top side **smooth** |
| M4 threads | **M4×0.7 directly in the plate**, modeled (FreeCAD hole as in `not-aus-spange.FCStd`), on the holes of the housing; just under six turns |
| M6 through-hole | **Ø 6.5**, axis 12.7 mm from the M4 line |
| M6 counterbore | **Ø 11, 2.5 mm deep** from above — **1.5 mm** of floor remains under the head. For button head ISO 7380 (Ø 10.5, 3.3 high), the head protrudes 0.8 mm |
| Taper | 61.3 → 15.0 mm, kink at y = 6.2 (at the housing edge) |
| Fillet | r = 3.0 on the **six vertical** outline edges |

**Two screws with separate tasks**, as with the board seat in §7.2:

- The **two M4** come from above through the housing holes directly into the
  threads of the plate. They hold the housing.
- The **M6** goes through the tip onto the rail. It holds the clamp.

#### How far out the M6 sits

Offset and tip follow the **recessed head**: the counterbore should not
cut into the housing edge, and next to it there must be wall at the tip.

| | |
|---|---|
| M6 axis behind the housing edge | **6.5 mm** |
| Counterbore edge in front of the housing edge | 1.0 mm |
| Wall next to the counterbore | 2.0 mm per side, tip 15.0 mm wide |

Until 2026-11-23 the tip was dimensioned for a **body washer Ø 18**
(M6 axis 10 mm behind the edge, tip 21 mm). The screws are short, ISO 7380
with button head, and must go deeper — hence the counterbore, and with it the
washer is dropped: a counterbore of washer size would leave no wall. The tip has accordingly
become slimmer.

#### Top side smooth, corners rounded

No raised pads — the plate lies flat against the end face over its full area. The
original 6.0 mm thickness was owed solely to the embedded nuts; with
the threads in the print, 4.0 mm suffice as with the emergency-stop clamp.

**Only the vertical** outline edges are rounded, not those at top and bottom:
there the print gains nothing; the nozzle only has to slow down at the vertical
corners.

> The edges are found in the script by their **position**, not by their index.
> An index shifts with every change to the tree, a coordinate does not — and
> the many edges of the modeled thread thus stay out of it. If the
> script does not find exactly six edges, it aborts instead of rounding arbitrary ones.

> **Print orientation:** with the **smooth top side down** on the bed. The floor
> of the M6 counterbore then lies on top and is bridged over 11 mm — at 1.5 mm thickness
> and the Ø 6.5 hole in the middle that is a narrow ring that holds without support.
> The threads are through-going and the same in both orientations.

### 7.6 Holder for the XT60 panel-mount connector

The supply lead from the power supply gets an **Amass XT60E-M** on the frame, the
panel-mount connector with two ears. The holder is an L-bracket: base plate with **one
central M6** onto the profile, on it the end wall with the cutout for the sleeve.
**The connector lies flat** — its long axis parallel to the bearing surface, the plug-in direction
parallel to the profile. The cable thus runs along the frame.

Outer dimensions **40.0 × 34.0 × 19.1 mm**, 9.5 cm³.

| | |
|---|---|
| Feature tree | Grundplatte · Stirnwand · Rippe_p/_m · Ausschnitt · Ohrloecher · M6_Durchgang · M6_Senkung · Fasen |
| `hardware/xt60_halter.py` | builds the tree from scratch, exports `.step` and `.stl` |

Connector dimensions from the dimensional drawing in the Amass datasheet, read off 2026-11-22:

| Feature | Dimension |
|---|---|
| Sleeve (outer contour) | 15.5 × 8.1 mm |
| Length over the ears | 27.0 mm |
| **Hole spacing of the ears** | **20.0 mm**, Ø 3.0 |
| Flange | 4.0 mm thick, total depth 16.7 mm |

> **Before printing, re-measure the hole spacing on the connector.** The drawing in the
> datasheet is a small raster graphic, and dealers quote 16, 19 or 20 mm for "XT60 panel mount"
> depending on the variant. It is the only dimension where a reading error
> makes the holder unusable; `con_pitch` is in the table for that.

How the sleeve comes through the wall: cutout 16.0 × 8.6 mm (0.25 mm of air per
side), the ears rest against the **back** of the 3 mm thick wall, two **M3
from the front** through the wall into the ears, nuts in the hexagonal pockets that the
connector has in the ears on the solder side. The sleeve protrudes, in every reading of the
drawing, at least 7 mm in front of the flange, i.e. about 4 mm in front of the wall.

**Fully open at the top.** The cutout runs at full sleeve width up to the
top edge, so the wall is **two posts** that end at the height of the sleeve's top edge.
The connector is inserted from above with the leads soldered on, nothing is
threaded, and there is no bridge in the print.

> **The web next to the ear hole is 0.4 mm** — that is the geometry of the connector,
> not of the holder: hole spacing 20.0 with a 15.5 sleeve and Ø 3 means 2.25 mm from the
> hole edge to the sleeve contour; with clearance and M3 through-hole 0.4 mm remain. The slicer
> prints the sliver or leaves it out, either is fine: the screw is guided by the ear,
> the head rests three quarters on the post, and the sleeve sits
> laterally in the cutout.

#### First the M6, then the connector

The connector lies above the screw; with the connector inserted no wrench
can be applied any more. That is why the cylinder head is recessed 3 mm (counterbore Ø 11), and the
cutout sits so high that the sleeve stays 1.5 mm clear above the head: center of the
cutout 14.8 mm above the bearing surface. Two triangular ribs outside, 3 mm thick, support
the wall against the insertion force; they lie with 0.5 mm of air next to the ears.

No key for the profile slot — as with the other holders, the clamping of the
M6 holds. Whoever wants one measures the slot width on the profile and enters it as `key_w`/`key_h`;
then a bar is added at the bottom.

> **Print orientation:** with the **base plate on the bed**. Wall and ribs then stand
> vertical; there are no overhangs.

### 7.7 Insulating plates under load-cell and sensor housings

Both housings (§7 and §7.4) are open at the bottom; the board lies with the solder side
down 2 mm above the mounting surface — and that is aluminium. Under each goes
a **0.5 mm thin plate** in the housing contour, with the M6 hole in the same
place, and with a **1 mm wide, 1 mm high rim** around the outside, in which the
housing sits with 0.1 mm of air per side and cannot wander when tightening.

| | Load-cell housing | Sensor housing |
|---|---|---|
| Outer dimensions | 51.4 × 30.4 × 1.5 mm | 38.3 × 22.2 × 1.5 mm |
| M6 | x = 42.2, centered | x = 30.2, offset 4.5 mm |
| Rim | all round | **three sides**, open at the front |

| | |
|---|---|
| Feature tree | Platte · Sitz · M6_Loch · Kanal_1 (· Kanal_2) |
| `hardware/isolierplatte.py` | builds **both** plates, exports `.step` and `.stl` |

**No dimensions of its own.** The script reads contour, hole position and channel position when building
from the parameter lists of `hx711_gehaeuse.py` and `hall_gehaeuse.py`. If a housing changes,
the script runs again, and the plate follows. The same number in
two places was the source of almost all errors in this project.

**Under the cable channels the plate is cut out**, from the end of the cavity to
the rim and through it. The housings clamp the cable between cable groove and
mounting surface, deliberately a few tenths narrower than the cable; 0.5 mm underneath
would make that double and squeeze the thin cell strands. The cable lies
there directly on the aluminium — it is insulated, the board is not. The cavity remains
completely covered.

**On the sensor housing the rim is missing on the sensor side.** The end wall there is 1.2 mm
thick and stands with the air gap in front of the magnets; a millimeter of rim in front of it would go
into the air gap. The plate ends flush at the front; the housing is pushed into the
tray from behind and sits on three sides.

> **Print:** flat, tray facing up. 0.5 mm of floor is two
> to three layers at 0.2 mm layer height; the rim comes on top as a wall.

### 7.8 Clip for the vials

For aligning the frame, **block levels 15.1 × 15.1 × 40 mm** (measured on the part,
2026-11-23) go onto the profiles. One clip per vial: a **1.5 mm thin
base plate** with **two M6 button head** in the profile slot, one per end, the vial
in between. 1.5 mm under the screw head is a given; the whole plate is that
thick so that the vial lies in the same plane as the screw seat and the
underside is a single surface. No counterbore, the head stands on top.

Outer dimensions **70.3 × 17.8 × 17.9 mm**, 3.0 cm³, hole spacing **55.8 mm**.

| | |
|---|---|
| Feature tree | Grundplatte · Leisten · Stirnwaende · Nasen · M6_Loecher |
| `hardware/libellen_clip.py` | builds the tree from scratch, exports `.step` and `.stl` |

**The vial remains visible from three sides** — from above and from both long sides.
It is therefore held at the end faces:

| | |
|---|---|
| End walls | 1.5 mm thick, 16.4 mm high above the plate, springy |
| Snap hooks | 0.6 mm over the top edge, 10 mm wide, centered; slope about 27° to the vertical |
| Longitudinal bars | 1.2 mm thick, **2.0 mm high**, full length — align crosswise and stiffen the plate |
| Clearance | 0.3 mm in length and width, 0.1 mm under the snap hooks |

Insertion from above: the vial pushes the end walls apart via the slopes,
the snap hooks snap over the top edge. Release: push one end wall outward with the thumb,
lift the vial. The wall thereby flexes 0.6 mm at the top edge,
at 1.5 mm thickness and about 14 mm of free height above the bar under 1 % strain
at the foot. The screw head sits with 1 mm of air next to the wall and below where
it moves.

> **Do not overtighten.** Under the head there is 1.5 mm of plastic on 52 mm² of ring area
> — hand-tight is enough, the clamping holds, and too much torque presses the head into the
> material and warps the plate on which the vial lies.

> The head diameter 10.5 mm is the standard value ISO 7380 for M6 and determines only the
> length of the pads; other screws are entered as `head_d` in the table.

> **Print:** flat, base plate on the bed. The snap hooks protrude 0.6 mm freely, one
> nozzle width, without support.

### 7.9 Spacer disc with locating key

Disc **Ø 40 × 5 mm**, hole **Ø 8.5** (M8 through-hole), on one end face a
**bar 8 mm wide, 3 mm high** across the face. The disc lies flat on the
aluminium profile, the bar engages the slot and secures it against rotation, the M8 goes through the
disc into the slot. Around the hole the bar is **cut away over 9 mm**, so that two
bar pieces with straight inner edges remain; all eight vertical bar edges, inside
at the cutout and outside at the circumference, are rounded with **r = 1 mm**. Outer dimensions
40.0 × 40.0 × 8.0 mm, 6.7 cm³.

| | |
|---|---|
| Feature tree | Scheibe · Nase · Rundung · Loch |
| `hardware/distanzscheibe.py` | builds the tree from scratch, exports `.step` and `.stl` |
| Bar as printed | 7.8 mm wide: nominal 8.0 minus 0.2 print allowance (`nose_fit`), parallel flanks |
| Bar pieces | 2 × 15.1 mm long; outside they end exactly on the circumference, inside 4.5 mm from the axis |

> **Print:** bar-free side on the bed, the bar stands on top. No overhangs. If
> the bar sits too tight in the slot, increase `nose_fit`, do not file the bar down.

## 8 Mark mounting on the drum

Co-rotating side, counterpart to §7. Not a printed part.

**8 magnets** on the pitch circle of the 8 spacer blocks of the drum end face. There
the 15 mm plywood disc is supported from behind, the radius is maximal, and
the pitch is fixed by the existing structure.

### 8.1 Parts

| | |
|---|---|
| Magnet | disc Ø10 × 3 mm, center hole Ø3.0 countersunk, N42 |
| Screw | **DIN 7982 A2, 2.9 × 13 mm**, countersunk head, Phillips |
| Pilot hole | Ø2.0 mm, approx. 11 mm deep |
| Screw-in depth | 10 mm into 15 mm plywood |
| Centrifugal force per magnet at 60 km/h | 1.2 N |

Ø2.9 instead of 3.0, because a 3.0 screw does not go through a true Ø3.0 hole. Smaller
than 2.9 hardly exists in A2 with countersunk head and Phillips; if the 2.9 binds,
brass 2.5 × 12 — equally non-magnetic.

### 8.2 The screw must be non-magnetic

Not a corrosion but a measurement question: the screw sits in the flux path of the axially
magnetized magnet. Steel shorts north and south through the magnet, the
field becomes ring-shaped instead of central, the switching point wanders, in the limit there are two
edges per magnet. **A2 or A4, never galvanized steel.**

The head of the 2.9 measures 5.5 mm and sits somewhat deeper in a countersink for Ø3
than a 3.0 would — desirable, because the steel is thus further from the sensor.

### 8.3 Field margin

`B(z) = Br/2 · [(z+t)/√(R²+(z+t)²) − z/√(R²+z²)]`, N42 with Br ≈ 1.3 T, hole subtracted by
superposition as a counter-disc R = 1.5 mm. Distance to the chip:
`z = air gap + 1.2 mm housing end wall + ≈ 0.5 mm chip position`.

| z | B |
|---|---|
| 3 mm | 116 mT |
| 4 mm | 96 mT |
| 6 mm | 58 mT |
| 8 mm | 35 mT |

B_OP of the TLE4905L in the worst case 18 mT. The hole costs 22 %, at 8 mm a
factor of 2 remains. Therefore choose the air gap rather generously: the disturbance from hole and
screw head has a length scale of only a few millimeters.

### 8.4 Polarity

TLE4905L is unipolar. All eight magnets with the same side facing out; a
cylinder magnet fits into the hole in both directions, the mechanics do not prevent the
flip. **Check each magnet individually against the sensor before tightening.**

### 8.5 Angular position: measure instead of drilling

Drilling accuracy is not needed. At high speed ω changes within one
revolution by at most 0.15 % ([09-measurement-theory](09-measurement-theory.md) §1.7); the
deviations of the eight intervals from 1/8 each are thus directly the angular errors.
Record once during spin-down, store as a correction table.

Needed only for speed **below** one revolution. What goes into the result runs
index-triggered over whole revolutions — there the angular error cancels exactly.

### 8.6 Revolution without a second Hall: counting or intermediate magnet

The 8 magnets are mounted; initially **one** Hall is wired at the marks
(`PIN_MARK`). Where the firmware then gets the whole revolution from (`speed.cpp`,
`service()`) is shown by the sensor side under the counter table:

| | Revolution | Zero point | Self-correction |
|---|---|---|---|
| **only 8 marks** | every 8th edge | first mark seen, arbitrary | none: a swallowed edge shifts all subsequent boundaries by 45° until the drum stops |
| **Intermediate magnet** (optional, prepared 2026-11-24) | pulse after two short intervals | fixed: the gap with the intermediate magnet | every revolution re-locks the count; swallowed and extra pulses are counted |
| Index Hall at `PIN_INDEX` | index edge | fixed | like intermediate magnet, costs a second sensor |

**Intermediate magnet:** a ninth magnet, same type, same polarity, **exactly in the
middle of one** 45° gap (22.5° from two neighbors). The Hall then sees seven
intervals of 45° and two of 22.5°. The firmware detects "short, short" (interval
below `MARK_SHORT_RATIO` = 0.7 of the running mean), confirms the pattern when it
comes `MARK_PATTERN_CONFIRM` = 2 times at a spacing of exactly 9 pulses, and thereafter counts
9 pulses per revolution with 8 regular marks; the intermediate pulse is not a mark and
does not change the angle bin. Shortest pulse spacing at 60 km/h 10.6 ms, at 100 km/h
6.4 ms; `MIN_PULSE_US` = 800 µs stays well below.

Mechanics: the magnet with its A2 screw (about 10 g) sits asymmetrically; at 60 km/h that is
5 N of centrifugal force once per revolution — exactly the order that shows up in the
ripple. **Opposite (180°) fit an equally heavy, non-magnetic screw** (A2 or
brass), then the imbalance is gone and the Hall sees nothing there. As with the
eight: check polarity against the sensor before tightening (§8.4).

What the detection reports: *re-locked* counts how often the pattern came at an unexpected
position (before that an edge was lost or one was added), *swallowed* intervals that were too long,
*spurious pulses* a "short, short" in the middle of the revolution. All three should stay at zero in
measurement operation; `Zähler nullen` resets them. Without an intermediate magnet
the firmware stays with counting every 8th edge; nothing has to be switched.

## 9 Brake: short circuit via three relays

The motor has no mechanical brake. Braking is done by shorting the three phases
against each other; the energy stays in the copper.

**Measured on 2026-11-01:** 25 s from over 400 rpm. From that `J·ω/t` = 1.94 · 41.9 / 25 =
**3.25 Nm** mean braking torque and 68 W mean power loss — thermally
irrelevant. Compared to 3 to 6 minutes of free spin-down, a factor of 8 to 14.

### 9.1 Circuit

```
   +36…42 V (bus)
        │
   ┌────┴────┐   three 12 V coils in SERIES
   │ K1 K2 K3│   together 140 mA, 1N4007 antiparallel across the whole chain
   └────┬────┘
        │
        C
      2N5551        B ──1 kΩ── GPIO40
        E                └─10 kΩ─┐
        │                        │
       GND ──────────────────────┘
```

| Part | Value | Why |
|---|---|---|
| Relays | 3 × automotive changeover relays 12 V, contact at **87a** (NC) | closed when de-energized = braked |
| Coils | **in series** on the bus | 3 × 12 V = 36 V, no DC-DC converter needed |
| Transistor | **2N5551**, 160 V / 600 mA | bus plus diode drop; 45 V types are too tight at 42 V |
| Base resistor | 1 kΩ | 2.6 mA at hFE ≈ 60 under load |
| Base–emitter | 10 kΩ | GPIO is high-impedance on reset — without it the base floats |
| Freewheel | 1N4007 across the chain | one is enough |
| Phase leads | ≥ 4 mm² | 60 A peak; the supplied socket cables are too thin |

### 9.2 Why in series and not via a converter

The coils hang directly on the bus. If it collapses, the relays drop out **immediately**;
a DC-DC converter would hold them for a few more milliseconds via its output electrolytic. The
current in series is only **once** 140 mA, not three times.

### 9.3 Limits

- The NC contact 87a on automotive changeover relays is usually specified only for **20–30 A**, not
  the advertised 40 A. Against 60 A peak as carrying current for seconds acceptable,
  not permanently.
- When closing into the spinning machine, current flows immediately (20 V back-EMF). With
  contact bounce this gives a make arc — the contacts are a wear part.
- The three relays are a **set**: in series they divide the voltage according to their
  resistance. Replace only with the same model.
- A **parking brake this does not replace.** For that the blocking wedge remains
  ([07-safety](07-safety.md) §3).

## 10 Motor controller FSESC 4.20

**Flipsky Mini FSESC 4.20 with heat sink.** VESC hardware V4.20, configured
via USB with the VESC/BLDC tool. Source of this section is the manufacturer's manual
supplied with the controller (specs and wiring diagram).

### 10.1 Key data

| Quantity | Value | Note |
|---|---|---|
| Hardware | V4.20 | VESC project, open source |
| Current | **50 A continuous, 150 A peak** | design case of the test stand: 3.3 A (§6) |
| Cells | 3–13S LiPo | |
| Voltage | **8–60 V** per manufacturer, **6–57 V** per firmware | *see note below* |
| BEC | 5 V / 1.5 A, internal driver | **not used**, see §10.3 |
| Timing | by software | |
| Cutoff voltage | programmable | relevant only in battery operation |
| Setpoint input | PWM (PPM/servo) | we drive via UART, §10.3 |
| Reverse | yes | direction of rotation by software, not by phase swap |
| Programming card | no | setting exclusively via USB |
| Mass | 80 g | |
| Outer dimensions | 67 × 39 × 18.3 mm | with heat sink |
| Scope of delivery | controller + heat sink, micro-USB cable, VESC sensor cable, manual | |

> Two lines of the original — "Governor" and the bare board dimension — are not legible
> in print and therefore are not in the table.

> **The manufacturer's 60 V is not the effective limit.** The VESC firmware clamps
> for hardware 410 to **57 V** and does not allow anything higher for `l_max_vin` at all — in the
> configuration found there it was exactly 57.0, i.e. the stop. The
> firmware is authoritative, not the datasheet.
>
> Without consequence for the stand: a full 13S pack has 54.6 V and stays below. In
> our case `l_max_vin` is at **48 V** anyway, deliberately above the regeneration throttling
> and far below the hardware limit ([07-safety](07-safety.md) §3.2c).

Two notes from the manufacturer on the same page, both retrieved 2026-11-17
([product page](https://flipsky.net/products/mini-fsesc4-20-50a-base-on-vesc%C2%AE-4-12-with-aluminum-anodized-heat-sink)):

1. *"The current factory-installed firmware is version 7.0 (the latest version)."* —
   see §10.5a.
2. *"Please do not remove the shrink to protect the pcb electronics components from
   impurites or metals which result in short circuit or esc burnt damage."* — the
   **heat-shrink over the board stays on**. In a workshop with chips,
   tools and a relay block next to it, that is no platitude: otherwise the board lies
   open under a structure that moves metal parts.

### 10.2 Connection diagram

```
                        ┌──────── BATTERY ────────┐
                        │   +  red      −  black  │   8…60 V  (here 36 V)
                        └────────────┬────────────┘
                                     │
   ┌────────┐                 ┌──────┴────────────────────┐              ┌────────┐
   │ 4 SWD  │────────────────▶│                           │◀─────────────│ 1 CAN  │
   └────────┘                 │   Mini FSESC 4.20         │              └────────┘
   ┌────────┐                 │   VESC-HW V4.20           │              ┌────────┐
   │ 3 COMM │────────────────▶│   50 A / 150 A peak       │◀─────────────│ 2 SENSE│
   └────────┘                 │                           │              └────────┘
                              │        [ Micro-USB ]      │
                              └────┬────────┬────────┬────┘
                                 C1│      B1│      A1│   3 phases, ≥ 4 mm²
                                   └────────┼────────┘    (brake relays in between, §9)
                                            ▼
                                        hub motor
```

Assignment of the four pin headers, in the order of the manual:

| | 1 CAN | 2 SENSE | 3 COMM | 4 SWD |
|---|---|---|---|---|
| | − | − | 5V | NC |
| | CAL | H3 | 3.3V | RST |
| | CAH | H2 | − | DIO |
| | 5V | H1 | ADC | − |
| | | TMP | TX | CLK |
| | | 5V | RX | 3.3V |
| | | | ADC2 | |

> Which end of the header is pin 1 is not evident from the drawing. Before
> first connecting, re-measure 5 V and ground at the connector.

The receiver is drawn in the manual with **GND / 5V / SIN**, i.e. the
PWM setpoint input. Of no relevance for the test stand — control is via UART.

### 10.3 What of it is used

| Connection | Counterpart | Note |
|---|---|---|
| BATTERY + / − | power supply 36 V via emergency stop and fuse | §1, §6 |
| C1 / B1 / A1 | motor phases, brake relays in between | §9. Direction of rotation by software, not by phase swap |
| COMM `TX` | ESP32 `PIN_DRIVE_RX` = GPIO 21 | 3.3 V level, no converter needed |
| COMM `RX` | ESP32 `PIN_DRIVE_TX` = GPIO 18 | |
| COMM `−` | common ground, star point per §5.5 | the only connection between logic and power ground |
| COMM `5V` | **do not connect** | the logic side hangs on USB (§5.1); two feeding sources would be a short circuit |
| SENSE | Hall cable of the motor | wire colors and assignment in §11; the white wire to `TMP` only after A5 |
| CAN, SWD | free | CAN only needed when a second controller is added; SWD only for flashing the bootloader |

The ESP32 additionally taps one Hall phase at `PIN_MOTOR_HALL` = GPIO 7 (§4).
**TBD:** At what level the Hall lines sit at the SENSE input is not
documented. If there is a pull-up to 5 V, the line must not go directly to the
S3 (§5.2) — measure on the connected controller, if necessary fit a divider or level shifter.

### 10.4 Limits

- **Regeneration.** The controller brakes electrically and pushes the energy back into the bus;
  a lab power supply does not absorb it (§6). The cutoff voltage is
  programmable, the overvoltage case nevertheless remains the controller fault case.
- **Sequence when braking.** First disable the driver, then let the relays drop out
  — the phases must not be shorted while the controller is driving
  ([10-firmware](10-firmware.md) N2).
- **The supplied phase and battery cables are too thin** for the 60 A peak
  of the short-circuit brake (§9.1).

### 10.4a USB does NOT power the logic

Demonstrated on 2026-11-17: with the power input disconnected this
controller disappears **completely from the USB bus**. The logic hangs on the power input, USB delivers
only data.

That is a difference from the original VESC 4.12, which also powers its logic from the 5 V of the USB.
**Do not generalize** — it applies to this Flipsky specimen.

Two consequences:

1. **For diagnostics:** only insofar as nothing works without the power input. The
   **emergency stop however does not disconnect the power input**, only controller↔motor — during the
   braking test on 2026-11-17 the controller nevertheless disappeared from the USB bus, so it
   reset. Suspicion and test procedure in [07-safety](07-safety.md) §4.
2. **For flashing:** the power input must be present during it. A small
   pack suffices (2S, 8.4 V were used) — without a motor there is no load, so
   no voltage dip. `l_min_vin` only blocks the drive, not the logic.

#### 10.4b Four voltage limits, and which one takes effect first

From the loaded configuration (`hardware/vesc-config-fw7.01/mcconf.xml`).
**The controller already responds far below the voltage at which it can
drive** — "the controller answers" therefore does not mean "the controller can drive":

| Limit | Value | Effect |
|---|---|---|
| `l_min_vin` | **8.0 V** | below this the drive blocks hard, undervoltage fault |
| `l_battery_cut_start` | **32.0 V** | below this the current is **reduced** |
| `l_battery_cut_end` | **30.0 V** | below this **zero current** — the drive does nothing any more |
| `l_max_vin` | 48.0 V | upper limit; the firmware itself allows 57 V (§10.1) |
| `l_battery_regen_cut_start` / `_end` | 42.5 / 44.0 V | regeneration is throttled (§3.3 in [07-safety](07-safety.md)) |

> **Measured on 2026-11-18: 8.2 V on the bus.** That is 0.2 V above `l_min_vin` and
> 21.8 V below `l_battery_cut_end`. The controller answered cleanly (FW 7.1, HW 410,
> no fault), but would have delivered **not one ampere**, and every load dip of
> 0.2 V would have made it drop out with undervoltage. To drive, the bus must be
> above 32 V, nominally 36 V.

### 10.5 Configuration as set on 2026-11-17

The controller contained a **foreign configuration**: gear ratio 2.77 instead of the
factory setting 3.0, `foc_openloop_rpm` 700 instead of 1500, and R/L/flux linkage from
a detection on a **different motor**. So a foreign drive had once been
measured in.

> *Earlier versions called this wholesale a "skateboard configuration" and claimed
> no value in it belonged to this setup. Overreaching:* `si_motor_poles` = 14,
> `si_wheel_diameter` = 0.083 m and the speed-controller gains are
> **factory settings** of the VESC firmware — demonstrated on the controller freshly flashed to 7.01.
> Listing in [03-parameters](03-parameters.md) §4.1.

The version found is saved byte for byte under `hardware/vesc-backup/`
(firmware 5.02, mcconf 458 B, appconf 419 B). **For restoring it is only good on
5.02** — see §10.5a.

**Detected** (`foc_detect_apply_all 50`, motor without drum):

| | |
|---|---|
| Phase resistance | 238.7 mΩ |
| Inductance | 403.8 µH *(measurement scatters, see [03-parameters](03-parameters.md) §4.1)* |
| Flux linkage | 17.287 mWb |
| Hall sensors | **found**, `foc_sensor_mode` = Hall |
| `foc_hall_table` | 255, **68, 0, 34, 134, 102, 167**, 255 — exact 60° division |

**Set by hand:**

| Setting | before | now | Reason |
|---|---|---|---|
| `si_motor_poles` | 14 | **30** | measured, [06-open-questions](06-open-questions.md) |
| `si_gear_ratio` | 2.769 | **1.0** | direct drive |
| `si_wheel_diameter` | 0.083 m | **0.9030 m** | drum, from `DRUM_CIRCUMFERENCE_M` |
| `timeout_msec` | 1000 | **300** | second dead-man level; firmware repeats every 150 ms, console tool every 100 ms |
| `timeout_brake_current` | 0.0 A | **0.0 A** | *see warning below* |
| `l_current_max` | 63.8 A | **20 A** | drive current; 0.389 Nm/A → 7.8 Nm |
| `l_current_min` | −63.8 A | **−3 A** | braking current stays small because braking regenerates |
| `l_in_current_max` | 99 A | **8 A** | protects the power supply (10 A) **independently** of the motor current |
| `l_in_current_min` | −60 A | **−1 A** | regeneration tight; does not help however, [07-safety](07-safety.md) §3.1 |
| `l_abs_current_max` | 130 A | **30 A** | panic threshold, must lie above the setpoint |

> **`timeout_brake_current` must stay 0.** Set to 1.0 A as a test and taken back
> again, for two reasons. First, **the state latches**: after the first
> timeout the controller calls the brake permanently, the power stage keeps running permanently
> (measured 120 mA at 40 V = 4.8 W, FET temperature rose to 32.9 °C) and a setpoint
> does not release it, because 300 ms later the next timeout takes effect. Second, and this weighs
> more heavily: **a permanently shorted motor cannot turn freely.** The
> spin-down test is a core method of this test stand, and the pole-count measurement
> would also have become impossible with it. The brake belongs on the relays (§9), not in the
> controller timeout.

**Motor current is not power-supply current.** The two stand in the ratio of the
duty cycle, because power is conserved. At 3 A in the motor and
duty cycle 0.26, 0.78 A flow from the power supply; at 99 km/h and duty cycle
0.85, 4.4 A in the motor become 3.2 A at the power supply (both measured and cross-checked at the
power supply). Whoever measures at the power supply and infers the motor current from it underestimates
it drastically at low speed.

### 10.5a Firmware 7.01 — and what it can do that 5.02 could not

Upgraded on **2026-11-17** from 5.02 to **7.01** (VESC Tool 7.01, bundled image
`40_o_47_o_48_o_410_o_411_o_412_o_DAS_RS.bin`, hardware 410). Procedure: first the
bundled **bootloader**, then the firmware — the bootloader is the safety net;
without it, after a failed flash only SWD remains.

**The update restores the factory state, it does not leave it.** Flipsky
writes on the product page of the Mini FSESC 4.20: *"The current factory-installed
firmware is version 7.0 (the latest version). If you require a different version,
please specify this in a note when placing your order."* The hardware is thus intended for 7.x
— including the current measurement, and that is the question that is legitimate with
clones.

That this specimen arrived with **5.02 and a detection on a foreign motor**
means conversely: it is not a current factory unit. What else deviates from the factory state
is unknown — the comparison in §10.5 shows only what *we* changed.

> The image reports `isTestFw: 1`, the version is called `7.01-1+f83a3ae1`. So it is
> not a release declared stable, but the development version matching this VESC Tool
> — the tool was built from source. Flipsky names
> **7.0**, i.e. the same generation. Whoever wants the designated release flashes the
> image of the official VESC Tool release; writing back the
> configuration is then uncritical, because the XML files address field names.

**Two things that tipped the balance:**

| Field | 5.02 | 7.01 factory | set |
|---|---|---|---|
| `foc_short_ls_on_zero_duty` | **does not exist** | 0 | **1** |
| `l_battery_regen_cut_start` / `_end` | **do not exist** | 1000 / 1100 V | **42.5 / 44 V** |

The first switches the lower switches permanently on at duty cycle 0 — the
**true bridge short circuit**, dissipative, without involvement of the bus. Exactly that was missing
in all four braking tests in [07-safety](07-safety.md) §3.1 and §3.2c. The
second turns the overvoltage case from a drop-out into a **throttling**.

**Neither is tested yet** — the braking test needs the motor. [TBD]

### Configuration does not survive the firmware change — the XML does

The signature of the configuration structure changed from 2211848314 to 471314414, the
`mcconf` from 458 to 493 bytes. The saved **binary files are thus worthless** for
restoring.

The way out: VESC Tool reads and writes the configuration as **XML via field names**,
without a GUI and without byte offsets:

```sh
"VESC Tool" --offscreen --getMcConf  mcconf.xml
"VESC Tool" --offscreen --setMcConf  mcconf.xml
"VESC Tool" --offscreen --getAppConf appconf.xml
"VESC Tool" --offscreen --setAppConf appconf.xml
```

This is how all values measured on 2026-11-17 were written back **without a motor** — including
Hall table, R, L and flux linkage, which would otherwise have needed a new detection.
The observer gain is not guessed in the process but derived: `g = 1e3/λ²`, checked at two
independent data points of the old firmware. **31 fields read back, all
confirmed.** Stored in `hardware/vesc-config-fw7.01/`.

> From now on the **XML** is the authoritative backup, not the binary file.

### 10.6 The relay block must preserve the phase sequence

When the emergency-stop relay block (§9) was first inserted, **two phases were swapped**.
The picture: 10 A motor current, 68 W input power, speed oscillating by ±5 rpm, **no
fault code** — the controller energizes the wrong winding, a rotating field without
net torque results. It hums and gets warm.

Checks, from harmless to elaborate:

1. **Without a meter:** relays dropped out, turn the drum by hand. It must turn *noticeably
   harder* than free — otherwise the NC contacts are not shorting the coils.
2. **Meter, relays dropped out:** between every two **motor** terminals ≈ 0.2 Ω
   (coils shorted); from every **controller** terminal against everything **infinite**. Continuity here
   means: the short-circuit node is also on the controller side, and the controller drives into
   a short circuit.
3. **Meter, relays energized:** exactly one one-to-one assignment, and
   the same as with direct wiring.
4. **Via the controller:** `measure_res_ind` must reproduce the direct value —
   251.9 mΩ against 237.2 mΩ direct showed the 14.7 mΩ of contact resistance and proved
   that all three phases conduct. A *swap* does **not** show up in this; for that
   `foc_sensors_detect_apply` is needed: if the direction of the angle sequence in
   `foc_hall_table` changes, two phases are swapped.

> **A newly detected table heals the swap, but is the worse solution.** With
> the drum mounted, the detection judders over the cogging torques because it turns slowly in
> open-loop operation: the 60° division afterwards scattered from 40° to 74°. Angle errors
> make torque ripple, and that ends up as noise on the load cell — where
> ΔCrr = 0.0005 is supposed to be resolved. The right way is: swap wires, then write back the
> reference table or re-detect **without** the drum.

## 11 Motor connection: phases and Hall connector

Recorded on the opened motor (photos of 2026-11-14). **None of it has been
measured through** — the assignment below is inferred from the wire colors and the routing of the
leads. Before first connecting, §11.3 applies; a swapped
Hall supply destroys the sensors.

### 11.1 What comes out of the motor

- **Three thick wires with blade connectors: yellow, green, blue** — the phases.
- **One white JST connector with six thin wires:** white, black, green, blue,
  yellow, red (order at the connector, as photographed).
- In the stator sits a board with three Hall ICs on the pitch circle. The
  colored wires and red/black lead to it; the **white wire does not go to the
  Hall board but in a fiberglass sleeve into the winding pack** — exactly where
  a temperature sensor would sit.

### 11.2 Inferred assignment

```
   JST 6-pin at the motor           FSESC, header 2 SENSE
   ┌─────────────────────┐
   │ 1  white   TMP ?    │ ─────▶ TMP   (connect only after A5)
   │ 2  black   GND      │ ─────▶ −
   │ 3  green   Hall     │ ─────▶ H3 ┐
   │ 4  blue    Hall     │ ─────▶ H2 ├ assignment arbitrary, the VESC tool
   │ 5  yellow  Hall     │ ─────▶ H1 ┘ detects the Hall table itself
   │ 6  red     +5 V     │ ─────▶ 5V
   └─────────────────────┘

   Phases (blade connectors):  yellow · green · blue  →  A1 · B1 · C1 of the controller,
   order arbitrary — the direction of rotation is set in software (§10.1).
```

Where the assignment comes from:

| Wire | Interpretation | Reasoning |
|---|---|---|
| yellow, green, blue | the three Hall signals | the same three colors as the phases; usual on the hoverboard wheel |
| red, black | supply of the Hall board, 5 V and GND | at the board, not at the windings |
| white | temperature sensor | the only remaining wire, and it leads into the winding, not to the Hall board |

> The same-colored Hall wire does **not** necessarily belong to the same-colored phase.
> Which end of the connector is pin 1 is likewise not fixed — the color is authoritative,
> not the position.

> **"Arbitrary" applies only until detection — after that the order is fixed.**
> On 2026-11-23 the motor energized after rewiring but did not turn: ERPM
> stayed exactly 0, the current rose on the ramp up to 6.4 A. The Hall table in the controller
> was unchanged, but Hall or phase leads were placed differently than on 11-17. Fixed
> via the firmware passthrough: `:foc_sensors_detect_apply 10`, new table in
> `hardware/vesc-config-fw7.01/README.md`. **Whoever re-plugs the motor connector or the
> phases re-detects the Hall table afterwards.** Without the VESC tool this is done with
> `tools/konsole.py ":foc_sensors_detect_apply 10"`; beforehand `":hall_analyze 10"`
> shows whether the sensors deliver six states. **And afterwards check the direction of
> rotation:** with the new table the drum ran backwards with unchanged
> `VESC_DIRECTION_SIGN`; the sign has been at +1 since (`e1`).

### 11.3 Test procedure before connecting

Without supply, with the multimeter:
1. Measure **white against black** — that settles A5:

   | Reading | Interpretation |
   |---|---|
   | ≈ 10 kΩ, **falls** on warming | NTC 10 k, presumably B 3950 |
   | ≈ 100 Ω / 1 kΩ, **rises** on warming | PT100 / PT1000 or KTY |
   | ≈ 0 Ω, opens on warming | thermal switch, not a sensor |
   | open | re-measure white against red and against the Hall wires before "no sensor" is accepted |

   Warming with a hair dryer to hand-warm is enough; what matters is the **sign**
   of the change.
2. **red against black:** it must not be low-impedance. Expected is the
   input resistance of the Hall board, not a short circuit.
3. Only then apply **5 V to red/black**, current-limited. Measure each of the three Hall wires
   against black and slowly turn the wheel by hand: the level must
   toggle. Count edges per revolution — that is `MOTOR_PULSES_PER_REV`
   ([03-parameters](03-parameters.md) §3).
4. **Note the high level.** If it is at 5 V, the wire must not go directly to
   `PIN_MOTOR_HALL` = GPIO 7; the ESP32-S3 is not 5 V tolerant (§5.2). The same
   applies to the tap at the SENSE input of the controller (§10.3).

### 11.4 Where the white wire goes

**Decided: to `TMP` of the controller.** There is only one evaluation point, because a
resistive sensor does not tolerate two pull-ups — with both, both measurements would be
wrong.

| | **Sensor at `TMP` of the controller** | Sensor at the ESP32 (ADC1) |
|---|---|---|
| Evaluation | the controller limits itself, the ESP32 reads the value via UART ([10-firmware](10-firmware.md) D25) | ESP32 via ADC1, characteristic in the firmware |
| Prerequisite | sensor type must match the characteristics of the VESC tool | free characteristic, but own circuitry and calibration |
| | **chosen** | – |

The reason is not convenience but where the shutdown sits: overtemperature
must block the **driver**, and the driver is the controller. If the sensor hangs on the
ESP32, the protective function runs over UART and thus over a link that can
fail. At the controller it limits even when the firmware has stalled.

Price for it: the sensor type must match a characteristic of the VESC tool. That is the
remainder of A5 — **which** sensor it is remains open and will be measured per §11.3.
`PIN_MOTOR_PTC` has been removed from `config.h`, GPIO 1 is free.

### 11.5 Re-sorting instead of soldering

The simplest way, **if both connectors are the same system**: pull the contacts out of the
cavities and reinsert them in the right order. No soldering, no
additional plug point, reversible at any time.

Two things must be established beforehand:

1. **Pitch and contact type of both connectors.** Caliper on the white motor connector
   and on the supplied VESC sensor cable. Same system → re-plug. Different
   systems → the contacts do not fit into the other housing, then the sensor cable
   is spliced on and re-sorted during splicing. **TBD**, neither measured yet.
2. **Which end of the `SENSE` header is pin 1** (§10.2). Re-measure 5 V and ground on the connected controller
   — otherwise the sorting comes out neatly mirrored.

Target order is that of the controller: `−` · `H3` · `H2` · `H1` · `TMP` · `5V`.

| Cavity | today | future |
|---|---|---|
| 1 | white | **black** — GND |
| 2 | black | green — Hall |
| 3 | green | blue — Hall |
| 4 | blue | yellow — Hall |
| 5 | yellow | **white** — TMP |
| 6 | red | **red** — 5 V |

> The three Hall wires may be in any order among themselves; the VESC tool detects the
> Hall table itself (§11.2). Only **black, white and red** must be right. In practice
> it is a rotation: everything except red moves forward one cavity, white moves to the end.

When pulling:

- **The yellow potting at the cable entry must go first.** As long as it holds,
  no contact comes out of the cavity. Break it open with the blade tip or soften it with isopropanol
  — do not pull on the cable.
- Press down the latching tongue with a fine needle or an extraction tool, then
  pull the wire out **toward the rear**.
- A broken latching tongue means: the contact no longer holds by itself. Bend the tongue
  back up or secure the contact in the housing.
- Photograph beforehand. Afterwards **check each wire individually for continuity** before
  voltage is applied.

## 12 MVP wiring

First build for commissioning: board, load cell with HX711, LCD, D-pad and the
UART link to the controller. **The motor does not turn in this.** Emergency stop, fuse and the
three brake relays (§9) are not installed; without them there is only free spin-down of 3 to
6 minutes ([07-safety](07-safety.md) §3). The controller is wired and the
UART connection checked, nothing more.

Speed in the MVP comes **via UART from the controller** (ERPM). The Hall sensors at GPIO 4/5/6/7
are not yet needed for this.

### 12.1 Block diagram

```
       USB-C power supply 5 V/1 A ──► [USB ]  ┐  both sockets may
       Notebook (flashing)        ──► [COM ]  ┘  feed at the same time (§5.1)
                                         │
         ┌───────────────────────────────┴──────────────────────────┐
         │            ESP32-S3-DevKitC-1  N16R8                     │
         │     5V pin = VBUS (output)   ·   3V3 pin = LDO           │
         │                                                          │
 HX711 ──┤ GPIO15  DOUT ◄───────────────────────  DAT               │
         │ GPIO16  SCK  ──100 Ω─────────────────►  CLK              │
         │ 3V3 ─────────────────────────────────►  VCC + VDD        │
         │ GND ─────────────────────────────────►  GND              │
         │                        screw terminal ──► load cell 5 kg │
         │                                                          │
 LCD   ──┤ GPIO8  SDA ──┬── 3.3 kΩ ──┐                              │
         │ GPIO9  SCL ──┼── 3.3 kΩ ──┤                              │
         │ 3V3 ─────────┴────────────┴──► backpack VCC  (§12.3)     │
         │ GND ─────────────────────────► backpack GND              │
         │                                                          │
 D-pad ──┤ GPIO11 up     GPIO12 down     GPIO13 in                  │
         │ GPIO14 out    GPIO39 OK    all against GND, internal PU  │
         │                                                          │
 FSESC ──┤ GPIO18  TX ──────────────────►  COMM  RX                 │
         │ GPIO21  RX ◄─────────────────── COMM  TX                 │
         │ GND ──────────────────────────  COMM  −   ← star point   │
         │                                 COMM  5V  ← NOT          │
         └──────────────────────────────────────────────────────────┘
```

### 12.2 Wiring list

| | from → to | Cable | Length | Connector |
|---|---|---|---|---|
| 1 | load cell → HX711 | the 4 wires of the cell (E+/E−/S+/S−) | **10 cm** | **none** — directly into the screw terminal, §5.4 |
| 2 | HX711 → ESP32 | **LiYCY 6×0.14 mm²**, shielded | per build | **JST-XH 6-pin**, assignment per §5.4 |
| 3 | ESP32 → LCD | 4-core, any | 30 cm | uncritical, digital |
| 4 | ESP32 → D-pad | 6-core (5 signals + common ground) | 50 cm | any |
| 5 | ESP32 → FSESC `COMM` | 3-core, twisted, **separate from the phases** | keep short | pin header on the controller |
| 6 | power supply → ESP32 | USB-C | – | one of the two sockets |

On #2: fix the connector assignment **now already** per §5.4, even if pin 1 (5 V)
stays empty in the MVP — then the five-pin module fits unchanged later. Shield as a
short pigtail to GND only at the ESP32 end. Solder in the 100 Ω per cell input and 100 nF
differentially across INA+/INA− right away; they cost nothing and one does not have to
go back in when the controller starts to interfere.

At the D-pad **no capacitors and no external resistors** — debouncing in
software, no RC in the signal path ([10-firmware](10-firmware.md) B3), the internal pull-up
suffices.

### 12.3 Three specs for the MVP

**HX711 entirely at 3.3 V**, i.e. VCC and VDD together on 3V3. That is a deliberate
deviation from §5.2 and applies only to commissioning. Reasons: the existing
breakout may be the four-pin one, where both pins are connected — then 3.3 V is
the only way that does not put 5 V on GPIO 15. It costs **3.6 dB SNR**, but
**no counts and no calibration factor**: the bridge hangs on AVDD, and AVDD is
at the same time the ADC reference, so the measurement runs ratiometrically (§5.2). The calibration
determined today therefore also holds after the change to a five-pin module.
For the measurement run the switch is made — with 12 N of signal from the best racing tire, the
3.6 dB are not given away.

**LCD level**, order to try (the assessment of the three paths is in §5.6,
open question B3):

1. **Backpack VCC to 3.3 V instead of 5 V.** Two minutes, no component, reversible — and
   **within the specification**, because the 4k7 on the backpack then pull to
   3.3 V and the PCF8574 only requires 0.7 · 3.3 = 2.31 V as high. Open is whether
   the contrast of the HD44780 suffices at 3.3 V and how dim the backlight becomes; its
   series resistor is designed for 5 V. The specimen decides that.
2. If that is not enough: **unsolder the 4k7**, 2 × 3.3 kΩ to 3.3 V at the ESP32, LCD stays
   on 5 V. **Unsolder first, then connect** — as long as the 4k7 to 5 V hang on the bus,
   5 V is present at GPIO 8 and 9.
3. **Level shifter** (BSS138), if one is available: specification-compliant and good contrast,
   costs two plug points.

Beforehand measure SDA against VCC on the unpowered backpack — whether the pull-ups are
fitted at all. And scan the I²C address, `LCD_I2C_ADDR` is TBD (0x27 or 0x3F).

**Five buttons, not four.** Four are the mandatory minimum
([10-firmware](10-firmware.md) B1), the fifth is "should" there. With four,
"confirm" would have to go onto a key combination or a long press, and B5 explicitly demands
that the step size does *not* go via a hidden combination. With the
five-way tactile switch the fifth contact is in the component anyway; it costs one wire and
GPIO 39. The pin count still works out exactly (§4).

### 12.4 Before first power-on

1. Controller to its micro-USB socket, then **re-measure 5 V and ground at the `COMM`
   header.** The manual drawing does not show which end is pin 1
   (§10.2) — otherwise the wiring comes out neatly mirrored.
2. `COMM 5V` stays free. Two feeding sources would be a short circuit (§10.3).
3. `COMM −` is the **only** connection between logic and power ground (§5.5).
   On the workbench there is a second ground path via the notebook if ESP32 and
   controller hang on the same computer — harmless for testing; for the measurement run the
   notebook comes off.
4. Check connector #2 **wire by wire for continuity** before voltage is
   applied. 5 V and 3.3 V are on the same connector; a connector offset by one pin
   destroys either the HX711 or the ESP32 (§5.4).

## 13 Web UI over WiFi

The test stand spans its **own access point** and serves the UI
itself — no internet, no router, no app. Operation is on the phone or the iPad; the
LCD (§5.6) is thus **optional** and only shows the same thing again.

The page is the same file that also runs as a draft in the browser:
`firmware/web/index.html`. `firmware/web/embed.py` bakes it gzip-compressed into
`src/webui_html.h` (34 KB → 11 KB) and runs as a `pre:` script on every build, so it
cannot go stale. No LittleFS, no second flash operation.

### 13.1 Constants

| Constant | Value | Note |
|---|---|---|
| `WEBUI_SSID` | `Pruefstand` | |
| `WEBUI_PASSWORD` | `<see firmware/src/secrets.h>` | **WPA2**, not open — otherwise anyone in range can drive the motor. **Exactly 8 characters, the minimum**; below that the AP stays open without it being noticed. Deliberately trivial: it protects against accident, not against intent, and it is never typed — it is in the QR code. The literal lives in `firmware/src/secrets.h` (git-ignored, template `secrets.example.h`). If it is changed, **`hardware/qr-wlan.*` must be regenerated**, see §13.4. |
| `WEBUI_AP_IP` | **4.3.2.1** | **not** 192.168.x.x, reasoning §13.3 |
| `WEBUI_CHANNEL` | 6 | |
| `WEBUI_MAX_CLIENTS` | 4 | the default of `softAP()` is also 4 |
| `WEBUI_PORT` | 80 | 443 stays **closed**: the HTTPS checks of iOS and Android cannot be answered anyway, and a refused connection is faster than a timeout |
| `WEBUI_HOSTNAME` | `tire` | mDNS → `tire.local`. **Short, because it is typed** — on a phone every character is one too many. Convenience, not a reliable path: Android cannot do mDNS in the browser (§13.5). |
| `LINK_TIMEOUT_MS` | 2000 ms | **safety limit**, see [07-safety](07-safety.md) §4 |

### 13.2 No captive portal — and why

A captive portal was built, tried and **removed** again. The measurements of
2026-11-15 and the supporting evidence:

- The portal window of iOS (Captive Network Assistant) **is not a browser**: no
  address field, no tabs, it belongs to the Settings app. A page in it can
  **not** hand over to Safari — there is no interface for that. The scheme
  `x-safari-http://` was tried on the device and does not work; in Safari's plist
  only `x-safari-https` is registered anyway, undocumented and version-dependent.
- **iOS disconnects the network** as soon as the window closes, because a network without internet is
  considered dispensable. Measured: connection held 15 s, with grace period 40 s, then gone.
  For a 30-minute warm-up whose dead-man switch depends on exactly this connection,
  that is unusable.
- In the portal window `fetch` **works**, but does not trigger a re-check — a
  login via JavaScript leaves the user sitting in front of a *Cancel* button, and
  Cancel disconnects.

**Instead:** The stand answers the connectivity check the way the operating system
expects it (Apple's success page byte-exact with trailing `\n`, Android's bare
`204`, Microsoft's 22 characters). Then the network counts as fine, **nothing pops up,
and nothing disconnects**. The UI is opened via the QR code (§13.4) or
any arbitrary address in the browser — the DNS answers everything with its own.

> The price is stated in RFC 8952 §1: As long as the phone is connected, it considers the
> connection usable and does not switch to mobile data. Messages and mail
> therefore stand still. After the measurement, WiFi off.

### 13.3 Why 4.3.2.1 and not 192.168.4.1

Android's `NetworkMonitor` discards the connectivity check as soon as the DNS resolves a probe host
to a **private** address, and does **not even send** the HTTP request:

```java
if (mPrivateIpNoInternetEnabled && probeType == PROBE_HTTP
        && (proxy == null) && hasPrivateIpAddress(resolvedAddr)) {
    return CaptivePortalProbeResult.PRIVATE_IP;
}
```

`hasPrivateIpAddress()` covers 10/8, 172.16/12, **192.168/16** and 169.254/16. The
result on Android would be "Connected, no internet" — without any possibility to
heal it. That is why the arduino-esp32 example also uses `8.8.4.4` and WLED `4.3.2.1`.

The address space belongs to someone else. Unpleasant, but without consequence with no internet at the stand.

### 13.4 QR codes

Two stickers, generated with `qrencode`, as PNG for printing and SVG for scaling:

```sh
qrencode -o hardware/qr-wlan.png -s 10 -m 2 'WIFI:T:WPA;S:Pruefstand;P:<password>;;'
qrencode -t SVG -o hardware/qr-wlan.svg -s 10 -m 2 'WIFI:T:WPA;S:Pruefstand;P:<password>;;'
```

| File | Payload | does |
|---|---|---|
| `hardware/qr-wlan.png` | `WIFI:T:WPA;S:Pruefstand;P:<password>;;` | camera offers "Join WiFi network" |
| `hardware/qr-ui.png` | `http://4.3.2.1/` | opens the UI in the **real browser**. Deliberately the **IP**, not `tire.local`: Android cannot do mDNS in the browser, and a QR code that leads nowhere on half the devices is worse than a number. |

`hardware/qr-wlan.*` is **not committed**, because it contains the password; it is
generated locally from `firmware/src/secrets.h` with the commands above.

Scan twice the first time. After that the phone connects by itself, and
**"Add to Home Screen"** turns the UI into an icon that starts it full-screen without
browser bars (`apple-mobile-web-app-capable` is set).

Readable at arm's length from about 3 cm edge length, in workshop light rather 4–5 cm. The white
quiet zone outside must remain, otherwise the camera does not find the position markers.

> **The password is in the image.** Whoever changes `WEBUI_PASSWORD` and leaves the QR code
> in place has a sticker that leads nowhere — and only notices at the stand. Both
> lines belong together. Afterwards **scan once with the phone**: the
> camera must offer the WiFi. The content was not machine-verified on
> 2026-11-18; no decoder is installed here.

### 13.5 How the stand is reachable

Three paths, and they use **different** mechanisms — that is the reason why
one always works:

| Input | who resolves | works |
|---|---|---|
| `http://4.3.2.1/` | nobody, direct | **always**, on every device |
| `http://tire.local/` | **mDNS/Bonjour** in the device | Apple, Windows 10+. **Not Android in the browser.** |
| any other name | the **DNS server of the stand**, which answers everything with 4.3.2.1 | everywhere the browser treats the input as an address and not as a search |

> **`.local` never goes via the DNS server.** Apple and Windows treat this suffix
> specially and ask exclusively via mDNS. The wildcard DNS of the stand therefore
> cannot help out there — both mechanisms run side by side and together cover
> more than either alone.

**A typed foreign IP address does not work and cannot work.** It asks no
DNS: the device sends the packet to the router — i.e. to the stand —, but the
destination address in it is a foreign one, and the network stack discards it. The only remedy
would be to accept packets for foreign destination addresses; the Arduino stack does not offer that,
and it would be the opposite of simple. It is not needed either, as long as names
work.

**Every unknown path** shows the UI (`onNotFound`). Whatever someone types behind
the address, they want to come here.

**A foreign name is redirected to `tire.local`** (302), so that afterwards a memorable
name stands in the address bar. **Two cases are never redirected:**

1. **The IP.** It depends on nothing — no DNS, no mDNS. Bending it to a name
   would mean making the emergency fallback of all things dependent on name resolution.
   `http://4.3.2.1/` always delivers the UI directly.
2. **The hostnames of the connectivity checks** (`captive.apple.com` and relatives). A
   redirect there is exactly the signal "captive portal" — and then iOS starts again
   to throw away the network after a short time (§13.2). The probe paths do have their own
   handlers, but an *unknown* path on a probe host would otherwise run into this.

### 13.6 Why "respond to all IP addresses" does not work

The obvious idea is to let the web server listen on every address and redirect from
there. **The web server already does that** — it binds to `0.0.0.0:80`, i.e. to
all addresses of the device. That is not the limitation.

The limitation lies two layers deeper:

1. Someone types `1.2.3.4`. The device sees: not in its own network → to the router.
2. The router is the test stand. The frame arrives at **its MAC address**, but carries
   **destination address 1.2.3.4** in the packet.
3. lwIP checks: "Is this one of my addresses?" No → **discarded, at the IP layer**.
4. TCP never sees the packet. No listener can accept it.

Looked up on 2026-11-18 in the shipped network stack what there would be against it:

| Mechanism | Finding |
|---|---|
| `LWIP_HOOK_IP4_INPUT` — hooks in **before** the destination address check | exists only as an empty template in `opt.h`. **Compile-time switch of lwIP itself**, and the Arduino core ships `liblwip.a` with 4.3 MB **precompiled**. |
| IP forwarding | `CONFIG_LWIP_IP_FORWARD is not set` — and without an uplink there would be no destination anyway. |
| NAPT (`ip_napt_enable`) | header files present, but behind `#if IP_NAPT` in the same precompiled library. Would not help either: only what has a route gets translated. |
| Proxy ARP (with netmask 0.0.0.0) | present in lwIP only for PPP, not for Ethernet/WiFi. |

**So no, not without a custom SDK** — and it is not needed either: the
wildcard DNS covers every *name*, and names are what gets typed.

> **Tested on 2026-11-18, iPhone:** `1.2.3.4` hangs until the browser gives up —
> exactly the predicted behavior. The packet goes out and is discarded.
### 13.7 Limits

- `WebServer` from the Arduino core can, according to its own source comment, serve **only one
  client at a time**. During a salvo of probes it swallows requests; the UI
  tolerates that because it only counts dropouts as a break after `LINK_TIMEOUT_MS`.
  Only when live values are to run over WebSocket does ESPAsyncWebServer pay off.
- **The access point is openly accessible**, even with WPA2 — whoever reads the password from the
  sticker can drive. For a stand with a motor that is a deliberate
  trade-off, not negligence: the emergency stop is hardware, the brake closed when de-energized.

## 14 Test-run variant

Standalone firmware for the **first test runs**: motor controller **and load cell**,
nothing else. No Hall sensors on the drum, no LCD, no state machine, no
measurement operation with plateaus and report. Three wires to the controller — `COMM` TX, RX and
ground (§12.2) —, and it delivers speed, bus voltage, currents, both temperatures and
the fault code by itself.

> It was first called the **controller variant** and only drove the drive. On 2026-11-18 the
> load cell was added, along with a Setup page for drum circumference, lever arms and calibration.
> With that it is the tool for the first runs, no longer only for commissioning
> the drive train.

**The speed comes from the controller speed**, not from the drum marks: the
Hall sensors on the drum are not yet wired. With the pole-pair count measured on 2026-11-17,
`ERPM / 15` is the drum speed — a calculation, not an estimate. As soon as
the marks are in place, the comparison of both sources is the plausibility check that
`SLIP_TOLERANCE` provides for.

Purpose: commission the drive train and get to know it **before** a single
measured value is due. The measurement chain is still TBD in essential points — cell
uncalibrated, lever arm not measured —, whereas the controller is completely there.

> **This variant has served its purpose.** With it and the tools under `tools/`,
> pole count, R, L, flux linkage, Hall table, idle-loss curve,
> idle losses of the drum and four braking distances were measured on 2026-11-17. The sections below are
> therefore partly superseded and updated.

That is no detour: [10-firmware](10-firmware.md) §2 (T1…T8) and §5.5 (D23…D28)
already demand exactly these functions.

    pio run -e esp32s3-vesc -t upload      # controller variant
    pio run -e esp32s3 -t upload           # back to the main firmware

### 14.1 What it can do

**Two pages.** *Measurement* shows speed, load cell (kg, force, torque,
rolling force), currents, power, bus voltage and both temperatures; below it a grid
with all steps from `V_MIN_KMH` to 100 km/h in increments of `SPEED_STEP_KMH`, plus
±5 for fine adjustment. *Setup* holds drum circumference, both lever arms and the
calibration of the cell.

> **Steps above `V_MAX_KMH` are shown locked in the UI** and the firmware rejects
> them. The buttons are still there so that it is visible what is missing: the release per
> [07-safety](07-safety.md) §5.

**STOP brakes** with `VESC_BRAKE_CURRENT_STOP` = 20 A — the spec reads
*fastest possible stop*. If the controller drops out in the process because a power supply instead of a
battery hangs on the bus, the drum coasts down and the UI says so. **The fault case
is thus at the same time the fallback path.** Next to it stands *Coast*, which always works.

Commands, identical over serial console and web UI ([10-firmware](10-firmware.md) G5):

| | |
|---|---|
| `v<kmh>` | target speed at the drum surface |
| `s` | STOP, brakes with 20 A |
| `c` | let coast |
| `t` | tare the load cell (only at standstill) |
| `k<kg>` | calibrate with known weight on the calibration arm |
| `d<m>` `a<m>` `b<m>` | drum circumference · lever arm · calibration arm |
| `z` | reset settings to the defaults from `config.h` |
| `i` | **status**: controller, load cell, sensors, operator link |
| `g` | zero the sensor counters (wiring test, see below) |
| `m0` / `m1` | idle / measurement run: drives through the measurement speeds in sequence |
| `R` / `Rk` / `Rs` / `Rn` / `Rx` / `Rm<k>=<v>` | Guided measurement run (`run.h`, [11-run-review](11-run-review.md) §3): start, main button, secondary button, again, abort, metadata (t tire, w wheel, b bar, c °C, l kg, p mm, n note, d Unix time). In the web UI behind *Measurement*. Report and data under `/runs/` |
| `h<kmh>` / `h-<kmh>` | add / remove measurement speed (list in NVS, sorted) |
| `l<N>` | wheel load; without it no measurement run starts |
| `n<A0> <B0>` / `w<A1> <B1>` | set idle-loss and wheel-loss model by hand; `m0` fits the idle-loss model itself, `u` the wheel-loss model |
| `u` / `y<kg>` | assistant spin-down + idle (drum up, lift wheel, both at once) / wheel mass for J_Rad |
| `j<s>` | live telemetry to the console, every 200 ms for s seconds — for logging with `tools/konsole.py` |
| `:<text>` | **passthrough** to the controller's terminal, as in the VESC tool: `:faults`, `:hall_analyze 5`. Answers as `# VESC> …` |
| `><hex>` | raw packet to the controller, answer as `# V <hex>`. `>0e` fetches the motor configuration, `>11` the app configuration |
| `?` | help |

`i` is the command for connecting. The UI shows the same, but at the stand
one stands in front of it with the screwdriver — and if one of the two connections
is missing, the browser is exactly what one does not have at that moment. If something is missing, `i`
names the GPIOs and the suspects along with them.

**Wiring test of the Hall sensors.** Under *Setup → Hall sensors* (and in the
`Geber` line of `i`) there is per channel — drum marks, index, wheel, motor Hall —
the pulse counter since the last zeroing, the time since the last pulse and the
level at the pin. Turn drum and wheel by hand: every counter must run along.
The level alone proves nothing, because the pull-up pulls an unwired sensor
high just like one without a magnet in front of it; `low` means magnet in front of the sensor,
and the LED in the housing then lights up. The test counters are separate from the measurement counters;
`g` zeroes them at any time.

**Wheel circumference from the speed ratio, integrated.** The Measurement page shows under
*Wheel* the drum and the wheel speed in rpm and the **effective
rolling circumference** of the wheel in mm. At the contact patch the circumferential speeds are
equal, so

```
U_Rad = U_Trommel · drum revolutions / wheel revolutions     (same time window)
```

The wheel revolutions are exact — the window begins and ends on a pulse of the
wheel sensor —, the drum revolutions are integrated over time
(`revsrc::revPerSec`). The window **slides** over the last
`WHEEL_CIRC_WINDOW_REVS` = 10 wheel revolutions and is recalculated at every pulse:
continuous, but smoothed. Until it is full, the counter is shown below; if the
wheel stands still longer than `WHEEL_CIRC_GAP_MS`, it expires. So the value does not flutter with
every pulse, as the instantaneous value of 2026-11-22 did. Measured at 15 km/h:
1527 to 1531 mm, from window to window.
The circumference lies below the tape-measure value because of the tire deflection and is exactly the
quantity that belongs in the evaluation. `i` prints the same line on the console.

> **The console in this variant is on the native USB socket**, not on
> UART0: `ARDUINO_USB_CDC_ON_BOOT=1` in `platformio.ini`. On 2026-11-18 only
> one port was there, esptool talked to the chip over it, but `Serial` went to the
> `COM` socket, where no cable was plugged in — so nothing came. With the flag the
> console reports on the same port. §5.1 describes the reverse case of
> 2026-11-15, when `COM` worked.

> **First connection test, 2026-11-18**, in two steps:
>
> 1. Load cell **answers** (−6871 counts, peak-to-peak 369, uncalibrated).
>    Controller **not** — and never since start, so not a lost
>    packet but no connection. Cause was too low a battery voltage: below
>    `l_min_vin` not even its logic runs.
> 2. With more voltage it answers immediately (FW 7.1, HW 410, no fault) — at
>    8.2 V however still without any ability to drive, see §10.4b. Exactly
>    for that `i` does not distinguish the two: it shows the voltage, and the
>    judgement stays with the human.

### 14.1a Calibration of the load cell

Two steps, both via the Setup page or the console:

1. **Tare** with the arm unloaded. Averages `TARE_SAMPLES` raw values, about 2.5 s.
2. Hang a **known weight** on the calibration arm and enter the weight.

The calculation is

```
F_Zelle    = m · g · L_cal / L_cell
N_per_cnt = F_Zelle / (raw − zero)
```

The weight hangs on the **longer** arm, the cell sits on the shorter — that is why a
handy weight produces twice the force at the cell (§Parameters `CALIB_ARM_M`).

> **As long as it is not calibrated, there are no forces, only raw values.** The UI
> then shows dashes and "not calibrated". That is intentional: a number that
> looks like a measurement is worse than a gap.

### 14.1b Where the values determined on the setup live

Drum circumference, both lever arms and the calibration factor are measured **on the setup**,
not at the desk. A calibration run that forces compiling and flashing is
not done — and a calibration run not done is exactly the source of invented
numbers that the project rule is written against.

That is why these four values live in the **NVS** of the ESP32 (`firmware/src/settings.*`).

> **The rule is nevertheless upheld.** `config.h` supplies the defaults, and the
> UI shows after every calibration the finished line for entry into `config.h`
> and [03-parameters](03-parameters.md) §2. The NVS is the working memory, **the repo
> remains the source of truth**. Whoever swaps the board loses the NVS — and notices
> it immediately, because the firmware then reports "not calibrated" again.

Implausible values are rejected instead of accepted: a mistyped drum circumference of
28.37 m is not to apply silently.

### 14.2 ERPM, and why the conversion is now permissible

The controller calculates in **electrical** revolutions; mechanically that is
`ERPM / pole pairs`.

Originally **only** ERPM was shown here, because the pole-pair count was not measured
and every conversion would have been an invented number. The controller's `si_motor_poles`
was no good for that: it is what someone typed in — and stood at the
**factory value 14**, while in reality there are 30 poles.

**Since 2026-11-17 the pole-pair count is measured: 15** (900 tachometer steps over 10
marked hand revolutions, [03-parameters](03-parameters.md) §3). The conversion is thus
a calculation and no longer an estimate, and `VESC_ERPM_MAX` is **derived** instead of
guessed:

```
VESC_ERPM_MAX = MAX_DRUM_RPM · MOTOR_POLE_PAIRS = 353 · 15 = 5295
```

Previously it stood at 2400, from the assumption that 7 pole pairs were the smallest plausible value.
The assumption was meant to be conservative and was off by a factor of 2 — the drum would
only have reached 160 rpm with it.

### 14.3 Motor temperature — sensor confirmed, query not yet updated

Originally the field was **not queried at all**, because the controller without a sensor on the
`TMP` input delivers a fantasy number **without an error message**, typically a large
negative value. A dash is more honest than a number that looks like a measurement.

**A5 has been answered since 2026-11-17:** it is a 10 k NTC in the stator, triply
confirmed ([06-open-questions](06-open-questions.md) A5), and `m_motor_temp_sens_type` is set
correctly. The value is thus real — that very fantasy number of −75.5 °C with an open
terminal was the *proof* of it.

The variant nevertheless still does not query the field (bit 1 missing in the mask of
`vesc/vesc.cpp`). That is now a **follow-up**, no longer a spec. [TBD]

### 14.4 Two dead-man switches

| | takes effect when | Effect |
|---|---|---|
| **in this firmware** | the operator computer is silent longer than `LINK_TIMEOUT_MS` | drive is released, drum coasts down |
| **in the controller** | no setpoint arrives for longer than `timeout_msec` | the controller shuts off itself |

The second also takes effect if **this firmware** stalls — that is why the
setpoint is only repeated while driving, and otherwise nothing at all is sent.

> **`COMM_GET_VALUES` does NOT reset the controller's timeout.** Only setpoints and
> `COMM_ALIVE` do. Whoever thinks they can keep the drive alive with value queries
> is mistaken — and only notices when the motor shuts off in the middle of the run.

### 14.5 One-time setup in the VESC tool via USB

Without these steps the controller does not answer at all or answers wrongly:

1. `app_to_use` = **UART** (or PPM+UART / ADC+UART). A configured external
   IMU also prevents the interface from starting — shared pins.
2. Baud rate **115200**.
3. Motor type **FOC**, sensor mode **Hall**. Sensorless there is no torque control
   below `foc_openloop_rpm`.
4. `si_motor_poles` = **30** (measured, §10.5).
5. `timeout_msec` to **300**.
6. Run motor and Hall detection once — or write the saved values back as XML,
   which works **without a motor** (§10.5a).

> **Item 5 was wrong here.** It read: "`timeout_brake_current` explicitly above
> zero — the default 0.0 A means *let coast*, not brake." That sounds
> cautious and is **harmful** for this test stand. Set to 1.0 A as a test and
> taken back on 2026-11-17: the state **latches**, the power stage keeps running
> permanently (4.8 W, FET at 32.9 °C), and a permanently shorted motor
> **cannot turn freely** — but the spin-down test is a core method of this
> stand. Reasoning in §10.5. The value stays **0**; braking is via the relays or
> via `VESC_BRAKE_CURRENT_STOP`.

The firmware version is reported by the variant itself at start (`COMM_FW_VERSION`). It
decides whether `foc_short_ls_on_zero_duty` and `l_battery_regen_cut_start/end`
exist at all — both only from **6.05**. The controller has been running on
**7.01** since 2026-11-17; both are present and set (§10.5a).
