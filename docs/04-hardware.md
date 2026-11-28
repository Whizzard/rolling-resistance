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
   E-stop       ─┤ GPIO2  (in)     GPIO40 (out) ── brake     │
   feedback      │                   release (§9)           │
                 │                 BLE ──── pressure sensor │
                 └──────────────────────────────────────────┘

   Power path (common ground with the logic side, star point §5.5; logic 5 V from the
   bus via an XL7015 buck into the 5V pin, §5.1):

   Power supply 36 V ──[fuse]── motor controller ── relay block ── hub motor
                                                       │ coils in series on the bus
                                       [E-stop] ── one wire ── 2N5551 at the ESP32, GPIO40 (§9.1)
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
| 3 × automotive changeover relays 12 V/40 A | short-circuit brake, §9 | **installed** 2026-11-17 |
| 2N5551, 1N4007, 220 Ω / 10 kΩ, fuse 500 mA | low-side switch of the relay chain, §9.1 (decided 2026-11-25) | on hand (assortment) except the fuse |
| NTC for the stator axle | only if the 6th wire at the Hall connector is not a sensor (A5) | open |
| Mating connector Julet 9-pin | or cut off the connector and fit own connectors | open |

| Power supply | 36–42 V, ≥ 10 A | open |
| XT60E-M panel-mount connector (Amass) | supply lead from the power supply at the frame; holder §7.6 | holder designed 2026-11-22 |
| ESP32 board | **ESP32-S3-DevKitC-1 N16R8** | procured |
| 5 V supply for the logic | **XL7015 buck module on the bus**, 5–80 V in, 0.8 A, set to 5 V, into the 5V pin (§5.1) | in use since 2026-11-25 |
| Five-way tactile switch | navigation button, 5 contacts + common ground (e.g. ALPS SKQU); **no** analog joystick breakouts | open |
| HX711 module | **separate VCC/VDD** (five-pin, SparkFun form factor) — four-pin modules are ruled out, see §5.2 | open |
| Hall sensors | 3 × TLE4905L (marks, index, wheel), unipolar | on hand |
| Magnets | 8 + 1, Ø10 × 3 mm with Ø3 countersunk hole | on hand |
| Power supply | 36 V, ≥ 10 A (better 15 A) — reserve for the current limit when starting | open |
| Brake resistor + chopper | **dropped** — braking is done by short circuit, §9 | – |
| Pillow-block bearings for the stator axle | 2 pieces, on the cradle frame | open |
| Timing belt | **dropped with direct drive** — only needed if A6 turns out negative | – |
| Emergency stop | positively opening, NC contact **in the relay coil chain** (§9.1); a second NC contact would give a direct feedback | open |
| Second HX711 | same form factor, for the later wheel-load measurement | later |
| BLE pressure sensor | **SKS AirSpy** at the valve, protocol from `~/git/airspy` | on hand |
| microSD card + passive microSD-to-SD adapter | adapter used as the card socket, wires soldered to its contacts, §15 | open |
| 4 × 10 kΩ, 10 µF + 100 nF | pull-ups and decoupling at the SD adapter, §15 | assortment |

## 4 Pin assignment

Board: **ESP32-S3-DevKitC-1**, module WROOM-1 **N16R8** (16 MB flash, 8 MB octal PSRAM).
Reconciled with `firmware/src/config.h` and `firmware/platformio.ini`.

The authoritative source is `hardware/schematic.py` — it also wires the peripherals
that the firmware does not yet touch.

| GPIO | Constant | Function | Direction | Note |
|---|---|---|---|---|
| 2 | `PIN_ESTOP_FB` | emergency-stop feedback | in, pull-up | **display** only; the disconnection happens in hardware. **Not yet wired** (`ESTOP_FB_WIRED` = false); until then the firmware detects the emergency stop from the missing motor current ([07-safety](07-safety.md) §4). Option in §9.1: divider from the drain node of the brake MOSFET, meaningful only while GPIO40 is low |
| 4 | `PIN_MARK` | speed marks drum | in, interrupt | **8** marks/revolution |
| 5 | `PIN_INDEX` | index mark drum | in, interrupt | 1/revolution, triggers the averaging |
| 6 | `PIN_WHEEL` | speed of the test wheel | in, interrupt | **1** magnet/revolution (`WHEEL_MARKS_PER_REV`); provides the effective wheel circumference (§14.1), later the wheel spin-down test |
| 7 | `PIN_MOTOR_HALL` | motor Hall (one phase) | in, interrupt | also the high-resolution speed source |
| 8 | `PIN_I2C_SDA` | I²C SDA | bidir | S3 default assignment; free for the IR sensor |
| 9 | `PIN_I2C_SCL` | I²C SCL | out | ditto |
| 10 | `PIN_SD_CS` | microSD, chip select (SD contact 1) | out | 10 kΩ pull-up at the adapter, §15 |
| 11 | `PIN_SD_MOSI` | microSD, MOSI (SD contact 2, CMD) | out | SPI2 via IO-MUX, §15 (since 2026-11-25; D-pad up until 2026-11-22) |
| 12 | `PIN_SD_SCK` | microSD, clock (SD contact 5) | out | §15 |
| 13 | `PIN_SD_MISO` | microSD, MISO (SD contact 7, DAT0) | in | 10 kΩ pull-up at the adapter, §15 |
| ~~14~~ | ~~`PIN_BTN_OUT`~~ | ~~D-pad out~~ | — | **free since 2026-11-22** |
| 15 | `PIN_HX711_DOUT` | HX711 #1 DOUT (torque) | in | |
| 16 | `PIN_HX711_SCK` | HX711 SCK | out | **shared clock of both HX711** |
| 17 | `PIN_HX7112_DOUT` | HX711 #2 DOUT (wheel load) | in | second cell not yet fitted |
| 18 | `PIN_DRIVE_TX` | UART1 TX → controller | out | |
| 21 | `PIN_DRIVE_RX` | UART1 RX ← controller | in | |
| ~~39~~ | ~~`PIN_BTN_OK`~~ | ~~D-pad center~~ | — | **free since 2026-11-22** |
| 40 | `PIN_BRAKE_RELEASE` | low-side transistor (2N5551) in the coil ground of the brake relays, in series with the emergency stop (§9.1) | out | **high = relays on = brake released.** 10 kΩ base–emitter: braked on reset. Not yet driven by the firmware; sequence in §9.4 |
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

That leaves **GPIO 1, 14, 39, 41, 42 and 47** free (as of 2026-11-25). The microSD took
GPIO 11…13 when the D-pad was dropped, not three of the last free pins: 10…13 are the
IO-MUX pins of SPI2 (FSPICS0/FSPID/FSPICLK/FSPIQ), §15. If anything else is added later,
the I²C bus is already there.

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

> **As built on 2026-11-25 (statement of the builder): the logic side is fed via the
> 5V pin, not via USB-C.** The text below describes the USB variant it was designed for
> and still holds for flashing and the console. What changes with the 5V pin:
>
> - The 5V pin sits **behind** the two Schottky diodes (D1, D7). A notebook on `COM`
>   is therefore still protected; its USB diode merely stays reverse-biased as long as
>   the pin carries more than about 4.7 V. Both may be connected at once.
> - What is lost is the protection **in front of** the pin: reverse polarity or
>   overvoltage go straight into the LDO and onto everything on `VCC_5V` (Hall sensors,
>   HX711 VCC). A fused, polarity-safe connector on the supply lead is the remedy.
> - The 5 V come from an **XL7015 buck module on the bus** (statement of the builder,
>   2026-11-25; module data per seller and XLSEMI datasheet: 5–80 V in, 5–20 V out
>   adjustable, 0.8 A, 150 kHz, **non-isolated**, input and output negative are one
>   node). Tap the input behind the 15 A fuse, as for the relay chain (§9.1).
> - Consequently the ESP32 ground reaches the star point twice, via COMM − and via the
>   converter's negative. Keep both leads short and run them together; the HX711 keeps
>   its ferrite and 100 µF at AVDD (§5.5), because 150 kHz switching ripple now sits on
>   the bridge supply too — the HX711 is ratiometric and cancels most of it, not all.
> - The ESP32 is thus **not** independent of the power path, contrary to the earlier
>   wording in [07-safety](07-safety.md) §4: bus gone → logic gone → no drum sensor
>   during a spin-down after a power cut. The relay brake does not care (de-energized =
>   braked), and the controller's logic hangs on the bus anyway (§10.4a).
> - Budget: around 150 mA average, 350 mA peak with WiFi (table below); the 0.8 A of
>   the converter suffice.

The logic side was designed to be powered **via USB-C**, not via the 5V pin. Espressif calls the
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
| TLE4905L | V_S = 5 V (minimum 3.8 V) | open collector, **pull-up 470 Ω to 5 V and Zener diode 3.3 V to ground at the ESP32** → ≈ 3.2 V, cathode to the signal. Soldering and connection template: `hardware/hall-plan.pdf` |
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
  **SparkFun SEN-13879 (installed):** the RATE solder jumper looks open but is closed by a
  trace under the solder mask (10 SPS from the factory). The stand ran at 10.5 SPS until
  2026-11-26 (95.4 ms between values in the raw log of run 2026-11-26-1542); the builder
  cut the trace that day, since then 83.8 values/s (console `i`). Noise at standstill
  after the cut: σ 801 counts per value above 6 Hz, 111 counts averaged over 8 values,
  i.e. the same as one value at 10 SPS (σ 124 at the tare of that run); no mains line
  in the spectrum. A lightly damped 4 Hz oscillation of the beam (decay over about 15 s
  after a touch) dominates the peak-to-peak value.
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

`hall-housing.FCStd` is a **PartDesign body with feature tree** and a
spreadsheet `Parameter`; all sketches are fully constrained and bound to the
table via expressions. Dimensions are changed **in the table**, not in the sketches.

| | |
|---|---|
| Feature tree | BaseBlock · Cavity · ComponentSpace · **LedgeChamfers · CableRamp** · SnapNoses · SpringRelief · ScrewHole · CableGroove (+round) · CableTrough (+round) · LED_Hole |
| `hardware/hall_housing.py` | builds the tree from scratch, exports `.step` |

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
| Feature tree | BaseBlock · Cavity · ComponentSpace · **CornerLedges · LedgeChamfers · CableRamp** · SnapNoses · SpringRelief · ScrewHole · CableNeck · CableGroove · CableTrough (+round) · MouthChamfers |
| `hardware/hx711_housing.py` | builds the tree from scratch, exports `.step` |

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
| Feature tree | Plate · M4_Thread · M6_Clearance · M6_Counterbore · Fillet |
| `hardware/esp_bracket.py` | builds the tree from scratch, exports `.step` |

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
| M4 threads | **M4×0.7 directly in the plate**, modeled (FreeCAD hole as in `estop-bracket.FCStd`), on the holes of the housing; just under six turns |
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
| Feature tree | BasePlate · EndWall · Rib_p/_m · Cutout · EarHoles · M6_Clearance · M6_Counterbore · Chamfers |
| `hardware/xt60_holder.py` | builds the tree from scratch, exports `.step` |

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
| Feature tree | Plate · Seat · M6_Hole · Channel_1 (· Channel_2) |
| `hardware/insulating_plate.py` | builds **both** plates, exports `.step` |

**No dimensions of its own.** The script reads contour, hole position and channel position when building
from the parameter lists of `hx711_housing.py` and `hall_housing.py`. If a housing changes,
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
2026-11-23) go onto the profiles. One clip per vial: a **2.0 mm base plate** with **two
M6 button heads** in the profile slot, one per end, the vial in between and taped to the
floor of the pocket with double-sided tape. The plate is as thick as the material under
the screw head, so the vial lies in the same plane as the screw seat and the underside is
a single surface. No counterbore, the head stands on top.

Second version, 2026-11-24: the first one with 1.5 mm walls and snap hooks on top was
flimsy. Now the walls are flush with the top of the vial, thicker, and braced from the
outside.

Outer dimensions **71.3 × 18.6 × 17.1 mm**, 4.6 cm³, hole spacing **56.8 mm**.

| | |
|---|---|
| Feature tree | BasePlate · Rails · EndWalls · Supports · Supports_Clear · M6_Holes |
| `hardware/level_clip.py` | builds the tree from scratch, exports `.step` |

**The vial remains visible from three sides**, from above and from both long sides. It
is located at the end faces and by the longitudinal bars, and held by the tape:

| | |
|---|---|
| End walls | 2.0 mm thick, **15.1 mm high = flush with the vial**, no hooks |
| Buttresses | one wedge on each side of every screw, on the longitudinal bars, 1.6 mm wide, from the top of the end wall down to the plate end; the screw head sits between them with 1 mm of air to the wall |
| Longitudinal bars | 1.6 mm thick, **2.0 mm high**, full length; align crosswise, stiffen the plate and carry the buttresses |
| Clearance | 0.3 mm in length and width |

> **Do not overtighten.** Under the head there is 2.0 mm of plastic on 52 mm² of ring
> area; hand-tight is enough, the clamping holds, and too much torque presses the head
> into the material and warps the plate on which the vial lies.

> The head diameter 10.5 mm is the standard value ISO 7380 for M6 and determines only the
> length of the pads; other screws are entered as `head_d` in the table.

> **Print:** flat, base plate on the bed. No overhangs, the wedges slope outward and
> down.

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
| Feature tree | Washer · Nose · Fillet · Hole |
| `hardware/spacer_washer.py` | builds the tree from scratch, exports `.step` |
| Bar as printed | 7.8 mm wide: nominal 8.0 minus 0.2 print allowance (`nose_fit`), parallel flanks |
| Bar pieces | 2 × 15.1 mm long; outside they end exactly on the circumference, inside 4.5 mm from the axis |

> **Print:** bar-free side on the bed, the bar stands on top. No overhangs. If
> the bar sits too tight in the slot, increase `nose_fit`, do not file the bar down.

### 7.10 Clip for the 1 kg calibration weight

The certified 1 kg weight has a waisted neck (just under 33 mm) under a 45 mm head, the
neck centre about 15 mm below the top of the head. The clip snaps onto the neck from the
side and hangs the weight from the 2 mm wire of the beam rider (§7.11). The weight only
has to hang freely under the rider, so there is no cross member over the head.

| | |
|---|---|
| Collar | C-ring, inside Ø 33.6, 4 mm wall, 8 mm high, opening 30 mm towards +y: it springs open 3 mm when pushed onto the neck and then holds by form. The head rests on the collar top |
| Posts | two 8 × 8 mm posts outside the head (1.5 mm air, 48 mm clear) from the collar lugs up to 3 mm above the head; there they bend inward at 45° (from z 26 to 40, the slanted part stays 8 mm thick) to 20.3 mm clear, i.e. rider width plus 1 mm air on each side, and run vertically to 85 mm |
| Pivot | Ø 2.3 hole through both posts along x, 77 mm above the collar bottom. The 2 mm wire lies across the tube: post, rider hump, post; secure it outside on both sides. The tube runs lengthwise between the posts (bottom edge 37 mm above the collar bottom, rider legs end at 28.3 mm since the beam rider was shortened on 2026-11-26, before at 25 mm; head top at 23 mm). When the weight swings, the posts move beside the rider, never over it, so the rider's clamp screws are never in the way |
| Outer size | 64 × 35.2 × 85 mm, 14.6 cm³ |
| `hardware/weight_clip.py` | builds the tree, exports `.step`; rider and tube dimensions are parameters |

> **Print:** standing on the collar. Posts vertical or 45°, wire hole short and
> horizontal. No support.

#### Dual clip for 0.2 / 0.5 / 1.0 kg

A second clip, `hardware/weight_clip_dual.py` (the clip above stays as it is), for the
linearity check with three masses ([09-measurement-theory](09-measurement-theory.md) §5.1).
It keeps the lower seat for the **1000 g and 500 g** weights (the 500 g head fits like the
1000 g, builder 2026-11-26). Above it, the **200 g** weight hangs on **three claws**
(builder's design 2026-11-26): three curved arches rise from the collar, one at each side
of the opening and one at the back, bow around the 1000 g head and its path in from +y,
and each ends in a claw that reaches into the 200 g neck groove. There is no ring at the
top. One hangs either a weight in the lower seat or the 200 g on the claws. Same pivot
interface (Ø 2.3 wire hole, 20.3 mm clear between the posts at the top), only taller.
As of 2026-11-26.

200 g weight, builder's caliper measurements 2026-11-26: neck Ø 21 (a round groove,
about 8 long), head Ø 27, body Ø 28, overall height 46, neck centre to top edge 8.
**Assumption** (not measured): the groove is modelled as a semicircle of radius 4 (half
the neck length) whose bottom is the neck Ø.

| | |
|---|---|
| Lower seat | as the clip above: collar inside Ø 33.6, opening 30 to +y, side webs, side posts 8 × 8 mm at 48 mm clear |
| Arches | each a web on the bed from the collar, a vertical 6 × 6 mm column outside the 1000 g head (front columns centred at x ±27.6, y 9.5, turned towards their claw, clear of the side posts; back column centred at y −27), then a branch rising inward at exactly **45°** to the claw. The branch underside is the extended lower flank of the claw, one straight line from the column to the tip; the inside corner column/branch is filleted (R 4), the outer corners rounded (R 3) |
| Claws | nose with its tip on r = 10.8 (groove bottom 10.5 + 0.3 play), 1.0 mm high at the tip, 45° flanks, top 3.0 mm above the neck centre; trimmed so that, with the weight seated, they stay clear of head (Ø 27) and body (Ø 28). Tips at 27.9°, 152.1° and 270° around the neck: the three contact points enclose the neck (largest gap 124°), so the weight is held and centred on the wire axis |
| Snap | the front claws stand **19.1 mm** apart, from the lower collar's ratio 30 / 33 applied to the 21 mm groove: **1.9 mm snap travel** at the groove bottom. Sliding the weight in from +y, head, neck and body push each front claw out by up to **1.5 mm** (computed by the generator over the whole path); the front arches flex like a 34 mm cantilever, 6 mm thick: about **1.1 % strain** at the root, fine for PETG (2 % as a limit for repeated snapping) |
| Heights | 200 g neck centre z 42.5 (set by "branch underside reaches the column at 3 mm above the 1000 g head"), head top z 50.5; its body hangs down to z 4.5 through the lower collar (Ø 28 < 33.6, and through the 30 mm opening when put in); pivot z 104.5 (clip above: 77), posts to z 112.5 |
| Clearances (checked by the generator) | 1000 g head at rest and on its way in, 3 mm free above it; 200 g weight at rest (no claw inside head, groove or body), its body down through the collar; spread of the front claws over the insertion path; rider legs (12 mm below the tube, as the first rider; now 8.7, so more room) 2 mm above the 200 g head; front columns separate from the side posts |
| Load line | the clip swings freely about the wire, so clip and weight settle with their common centre of gravity under the wire: the torque is m · g · L_cal for each weight, L_cal set by the wire hole in the rider. All seats are centred on the wire axis; the clip hangs within 0.2° of plumb |
| Tare | rider and clip hang on the arm permanently, also while zeroing; only the weights go on and off. The clip mass is in the tare, m in m · g · L_cal is the weight alone |
| Size | 64 × 44.4 × 112.5 mm, 24.3 cm³; about 31 g as solid PETG (1.27 g/cm³, typical datasheet value; with infill less, weigh it) |
| Overhangs | none beyond 45°: checked on every face and on 0.3 mm slices (each layer within 45° of the one below); only the wire hole is a short horizontal bridge, as in the clip above |
| Model | Part solids (booleans), not a sketch tree: the claws lie in inclined planes and are trimmed by a revolved envelope. Change values in `PARAMS` in the generator; the spreadsheet in the FCStd documents them. Objects: Arch_FR · Arch_FL · Arch_B · WeightClipDual |
| `hardware/weight_clip_dual.py` | builds the part, exports `.step` |

> **Print:** standing on the collar, like the clip above. No support. PETG (the front
> arches have to spring). **Check before trusting it:** the 200 g groove shape (semicircle
> assumed): the weight must click in with all three claws in the groove.

#### Lift for the 1000 g weight (draft)

Builder's idea 2026-11-27: put the weight on and take it off by a servo, so the
calibration runs without a hand at the arm. **Draft, not printed, not tested.** As of
2026-11-27.

| | |
|---|---|
| Hanger | swings on the rider's wire like the clip above. The weight stands with its flat bottom on a **cross** (8 × 6 mm arms along x and y); four **centring fences** at the arm ends (0/90/180/270°), 0.4 mm radial play, the lead-in over the full fence thickness (a ridge, no ledge). The x arms carry the posts: vertical outside the body, 45° inwards, vertical to the Ø 2.3 wire hole; taller than the clip's, so the lifted head passes under the rider legs |
| Cradle | four stubs turned by **45°** pass between the hanger's arms and lift the weight by its bottom. On each a **ramp** from the seat (0.4 mm play) rising at 45° all the way out to the column: the weight cannot sit on a ramp, it slides into the seat. A flat ring ties the stub ends; the columns rise from the stub ends, per side two columns join at 45° in a **head** with a vertical hole for a **3 mm steel guide wire** and, beside it, a horizontal hole for the **string** at x ±25 (50 mm apart, in line with the pendulum wire) |
| Base | hangs under the 40 × 40 profile (slot 8) of the traverse, which runs **parallel above the arm**; **2 × M6 button head** along the arm (y +10 and +40 from the horn plane, both clear of the servo) in flat counterbores from below as the cups, so it slides along the arm to the rider's position. The guide wires pass the arm in front and behind (x ±29.7). Holds the two guide wires in vertical **through holes** (parallel to the M6, orthogonal to the servo shaft; they lie outside the profile, so the wire goes in from above), clamped by a horizontal M3 set screw, one **turning pin** per side (3 mm steel) over which the string turns from horizontal to vertical, and the servo **lying flat**: shaft horizontal and parallel to the arm, body long side across, the double horn level across the arm at rest, centred between the wires (shaft, horn and guide wires are three different axes). The servo frame is open at the bottom, the servo goes in from below, 4 screws through the tabs. The strings leave the horn in opposite directions, so turning it pulls both by the same length |
| States | lowered: the cradle is 9 mm below the weight and touches nothing, the weight hangs only on the hanger. Raised by 20 mm: the weight sits in the cradle 11 mm above the hanger's cross. Either way the receiving ramps or fences catch and centre the weight |
| Checked by the generator | at least 3 mm between cradle, hanger and weight in both states; hanger may swing 4.5° before it touches the lowered cradle; raised cradle 5 mm below the base; no overhang beyond 45° on any part, no bridge; guide wires 4.5 mm and pin cheeks 2 mm clear of the servo; string pull with the horn hole at r 14: 17.9 mm at 90°, 23.4 mm at 120° |
| Height | the profile underside must be **at least 81 mm above the pendulum wire** (otherwise the guide wires get shorter and the base lower: change `base_gap`) |
| Assumed, TBD | weight body Ø 48 and height 58 scaled from the 200 g weight (× 1.71); servo MG996R class, typical listing dimensions, horn plane to tab face 16, tab face to top face 10, body below the tabs 27; horn hole radius 14; string Ø 1 |
| Print | hanger standing on its cross, cradle on its ring, base upside down (profile face on the bed); no supports |
| `hardware/weight_lift.py` | builds hanger, cradle and base, checks clearances and overhangs, exports `weight-lift-hanger.step`, `weight-lift-cradle.step`, `weight-lift-base.step`; the FCStd holds the assembly (cradle lowered and raised, servo envelope, guide wires) |

### 7.11 Rider for the balance beam

Slides along the calibration arm, a rectangular tube **15 wide × 25 high**, like a
saddle, and is clamped with **two M6 across below the tube**, one at each end (±22 mm):
through-hole Ø 6.4 on the front leg, a **modelled M6 thread** (PartDesign hole, ISO
profile, as in the ESP clamp) in the back leg. With 0.1 to 0.15 mm layers the thread
prints usably; otherwise run a tap through once, it finds the turns.
Tightening pulls the legs together and clamps the tube. The screws sit outside the range
the clip's posts sweep when the weight swings. The legs are only 1.5 mm thick; at each
screw a 2.5 mm boss (13 mm long from the end, 45° run-in) carries thread and screw head;
it runs from the leg bottom to 2.3 mm above the tube bottom, 5.5 mm each side of the
screw axis.
On top, centred, a **hump over the full width of the rider** (12 mm long on top, 45°
flanks both sides) with the Ø 2.3 hole for the 2 mm wire, **15 mm above the tube top,
axis across the tube, parallel to the clamp screws**. The hole runs through the whole
width, so the wire bears over 18 mm and has no long free span. The hole centre is the
point of application along the arm; drill the hole to size after printing.

**Snap claws (2026-11-26).** Like the vial saddle (§7.12), each leg has a small claw at
the tube bottom, over the full length, so the rider clicks onto the arm when pushed on
from above and sits there without the screws; **the M6 screws then only clamp**. Claw
dimensions are those of the saddle; unlike the saddle, the legs are not preloaded
(0.3 mm play across stays), so the rider still slides along the arm with the screws
loose. Because the claws now do the locating, the **M6 moved up to the claws** (builder,
2026-11-26): the axis is 3.2 mm below the tube instead of 6.5, the through-hole is tangent
to the tube bottom, and the screw runs directly under the tube. The legs got shorter by
the same 3.3 mm. To give the screw more material, **inner bosses** (builder: "the same
pads inside") mirror the outer bosses to the inside of each leg at each screw: same
13 mm along the tube with the 45° run-in, from the leg bottom up, but only 1.5 mm deep,
i.e. to the claw tip line, and only up to the claw top line, so they keep the claws'
play to the tube and touch it nowhere the claws don't. The holes run through them at the
claw height; the claws run on unchanged over the 30 mm between the bosses.

| | |
|---|---|
| Size | 56 mm along the tube, 18.3 wide (23.3 over the bosses), 53.7 high; legs 1.5 mm, 8.7 mm below the tube, top plate 4 mm, 0.3 mm play across; 19.0 cm³ |
| Claws | reach 1.5 mm under the tube (from the leg inside), 0.8 mm end face, 45° lead-in underneath; the top face rises 0.4 mm from the tip to the leg (about 15°), as in `hardware/level_saddle.py` |
| Claw height | chosen so that, with the top plate resting on the tube and the legs unloaded, the slope just touches the tube's lower edge: tip 0.36 mm below the tube bottom (`claw_gap`, derived from claw depth, rise and play). No vertical play, no preload; tightening the screws pulls the legs in, and the slope turns that into a pull that holds the top plate on the tube |
| Mounting | press straight on: each leg springs 1.35 mm over 25 mm, about 0.5 % strain at the root (PETG). To remove, loosen the screws and push one leg outward at its lower edge |
| M6 height | axis 3.2 mm below the tube bottom (`screw_below = m6_thru / 2`): through-hole top on the tube bottom in the model, 0.2 mm below it with the top plate resting on the tube, the M6 shank 0.4 mm below it. Over the holes only a skin of the claw slope remains (under 0.5 mm) |
| Inner bosses | at each screw on both legs: 13 mm along the tube (45° run-in towards the middle, as outside), 1.5 mm deep (leg inside to the claw tip line), from 8.7 mm below the tube up to the claw top line (0.16 mm below the tube bottom in the model, the claw slope above). At the bottom a 45° lead-in over 1.5 mm, so the tube top edge spreads the legs when pushing on, by the same 1.35 mm as at the claws |
| Walls | 2.3 mm of leg under the through-hole (`under_wall`, kept from the first version: 12 − 6.5 − 3.2), so the legs end 8.7 mm below the tube (`leg_below`, derived). Outer bosses from the leg bottom to 2.3 mm above the tube bottom, 5.5 mm each side of the axis |
| Thread length | **5.5 mm** (outer boss 2.5 + leg 1.5 + inner boss 1.5), was 4 mm |
| Feature tree | Sk_Outer · Block · Sk_BeamSpace · BeamSpace · Sk_Hump · Hump · Sk_Bosses · Bosses · Sk_Claws · Claws · Sk_Inner_Bosses · Inner_Bosses · Sk_Inner_Lead · Inner_Lead · Sk_Wire · WireHole · Sk_M6_Clearance · M6_Clearance · Sk_M6_Thread · M6_Thread |
| `hardware/beam_rider.py` | builds the tree, exports `.step` |

> **Print:** lying on its end face, so the profile reads as a U on the bed. All walls
> vertical; hump and bosses (outer and inner) have 45° run-ins on the side facing the bed; the claws are
> profile edges; the cross holes are short bridges. No support. PETG for the claws.

### 7.12 Saddle for a second vial on the calibration arm

A second **block level 15.1 × 15.1 × 40 mm** (same type as §7.8) sits on the calibration
arm itself and shows the arm's inclination along its length. The holder is a **U like the
rider (§7.11), pushed onto the 15 × 25 tube from above, without screws**: the cheeks are
preloaded against the tube and a small claw on each cheek hooks under the tube. The vial
lies on top, located only by a **3 mm rim** on the long sides, no end walls, no high side
walls, and is **glued** to the deck. The profile is constant over the 40 mm length.

| | |
|---|---|
| Deck | 2.5 mm, rests **without clearance** on the tube top; that is the vial's reference plane |
| Rim | 1.95 mm thick, 3 mm high, flush with the cheeks outside; 0.3 mm play for the vial |
| Cheeks | 2.0 mm, tilted inward: 15.3 clear at the deck, **14.4 at the claws, so 0.3 mm preload per side** on the 15 mm tube |
| Claws | reach 1.5 mm under the tube. The top face rises 0.4 mm from the tip to the cheek (about 15°): the preload pushes the claw inward and the slope turns that into a pull that presses the deck onto the tube, self-tightening and insensitive to a few tenths in tube height. Tip 0.15 mm below the tube bottom, 0.8 mm end face, 45° lead-in underneath |
| Mounting | press straight on (each cheek springs 1.8 mm over 25 mm, about 0.9 % strain at the root, fine for PETG, marginal for PLA) or hook one claw and press the other side down. To remove, push one cheek outward at its lower edge |
| Size | 40 along the tube, 19.3 wide, 33 high; 7.0 cm³ |
| Feature tree | one sketch `Sk_Profile` (closed 20-point polygon on YZ) · Saddle |
| `hardware/level_saddle.py` | builds the tree, exports `.step`; tube, vial, preload and claw are parameters |

> **Print:** lying on its end face, the U on the bed. All walls vertical, all slopes are
> profile edges, no overhang. PETG, 3 walls, brim: the footprint is only 19 × 33 mm at
> 40 mm height.

### 7.13 Cup for the check weight

A round cup that holds the check weight, screwed down with **one M6 button head** through
the centre of the floor. The floor is built like the plate of the iPad holder: a flat
counterbore from the inside takes the head completely, so the weight stands on the floor,
not on the screw. As of 2026-11-26.

| | |
|---|---|
| Interior | **Ø 50.0, 50.0 deep** from the top of the floor, as specified; no clearance added |
| Other sizes | named after the weights (the 1 kg cup is `weight-cup-1kg`). Builder 2026-11-26: inside **Ø 38 × 38** for the 500 g (`weight-cup-500g`, outside Ø 41.0 × 43.5, 13.8 cm³; fits well, builder 2026-11-28) and **Ø 29 × 28** for the 200 g (`weight-cup-200g`, Ø 32.0 × 33.5, 7.9 cm³; Ø 28 was too tight, +1 mm on 2026-11-28). Builder 2026-11-28: inside **Ø 62 × 65** for the 2 kg (`weight-cup-2kg`, Ø 65.0 × 70.5, 37.1 cm³; body Ø 60 per KERN 347-12, 60.5 scaled from the 1 kg). Same wall, floor and M6 |
| Wall | 1.5 mm |
| Floor | 5.5 mm = head height 4.0 (**assumed**, measure on the screw) + 1.5 under the head |
| M6 | Ø 6.6 through, flat counterbore Ø 13.0 × 4.0 (takes DIN 7985 dk 12 and ISO 7380 dk 10.6) |
| Size | Ø 53.0 × 55.5 mm, 23.7 cm³ |
| Feature tree | Cylinder · Interior · M6 |
| `hardware/weight_cup.py` | builds all four sizes from scratch (`weight-cup-2kg`, `weight-cup-1kg`, `weight-cup-500g`, `weight-cup-200g`), exports `.step` |

> One screw only: the cup can turn about it, which does not matter for a cup. If the
> weight sits too tight, raise `inner_d` in the table.

> **Print:** floor on the bed. No overhangs, no support.

### 7.14 Prism for measuring from a shaft

For measuring the distances from the pendulum axis to the force and load arms: on a round
shaft the centre is hard to see (builder 2026-11-27). The prism is set on the shaft from
above and marks its vertical centre plane. As of 2026-11-27, not printed yet.

| | |
|---|---|
| V | 90°, opening downwards, for shafts **Ø 16 to 20**: the symmetry plane of the V is the shaft's centre plane for every diameter (axis 11.3 / 14.1 mm below the vertex, contacts at ±5.7 / ±7.1 mm). Small relief at the vertex. The generator asserts both V faces at exactly 45° |
| Stop | vertical face at x = 0, facing left: a steel ruler lies flat on the shelf on the left and is pushed against it, so its zero sits exactly above the axis. For distances to the right, turn the prism round |
| Vial | block vial 15.1 × 15.1 × 40 (as §7.8) along the ruler, on the right; the stop is the left end wall of its pocket. Bubble centred = stop face plumb (the prism can rock on the shaft). Visible from above and both long sides, taped |
| Size | 92.8 × 20 × 36.1 mm, wings of equal length so it balances on the shaft; about 31 g solid PETG |
| Print | lying on its side: the part is one extruded profile, no overhang, no support; V faces and stop come out as smooth walls |
| `hardware/shaft_prism.py` | builds the part, exports `.step` |

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
measurement operation; the *Reset counters* button resets them. Without an intermediate magnet
the firmware stays with counting every 8th edge; nothing has to be switched.

## 9 Brake: short circuit via three relays

The motor has no mechanical brake. Braking is done by shorting the three phases
against each other; the energy stays in the copper.

**Measured on 2026-11-01:** 25 s from over 400 rpm. From that `J·ω/t` = 1.94 · 41.9 / 25 =
**3.25 Nm** mean braking torque and 68 W mean power loss — thermally
irrelevant. Compared to 3 to 6 minutes of free spin-down, a factor of 8 to 14.

### 9.1 Circuit — as proposed on 2026-11-24, not yet built

Drawings: `hardware/brake-plan.pdf` (circuit detail) and `hardware/brake-wiring.pdf`
(wire by wire). Both are generated from the `.py` file of the same name.

**Topology, decided by the builder on 2026-11-24:** the three coils sit in series on the
bus. The lower end of the chain goes to the emergency stop, and from the emergency stop
**exactly one wire** runs to a small switching module next to the ESP32, where a
transistor pulls that wire to ground. The relays are energized, i.e. the brake released, only while
**both** switches are closed: the hand (emergency stop) **and** the firmware (GPIO40
high). Whichever opens — hand, reset, firmware fault, broken wire, bus gone — the chain
drops and the motor is shorted.

```
   +Bus 36…42 V  (BATT + of the controller, i.e. behind the 15 A fuse)
        │
       F1  500 mA
        │────────────────┐
   ┌────┴────┐           │
   │ K1 K2 K3│  coils    ▲ D1  1N4007, cathode to +bus
   │ in series│          │     across the COILS only — not across S1
   └────┬────┘           │
        │────────────────┘   node A
       S1  emergency stop, positively opening NC contact, at the frame
        │
        │  wire 1 — the ONE wire to the switching module; carries bus voltage
        │           while the relays are dropped
        C
   Q1  2N5551     B ──220 Ω (R1)── GPIO40  PIN_BRAKE_RELEASE
        E              └─10 kΩ (R2)─┐
        │                           │
   Bus −  star point ───────────────┘   own wire, not through the DevKit's GND pin
```

| Part | Value | Origin | Why |
|---|---|---|---|
| K1…K3 | 3 × automotive changeover relays 12 V, contact at **87a** (NC) | installed 2026-11-17 | closed when de-energized = braked. Relays **without** built-in diode (a diode fixes the coil polarity and does not fit a series chain with one common free-wheeling diode) |
| Coils | **in series** on the bus | decision | 3 × 12 V = 36 V, no DC-DC converter (§9.2); 140 mA once |
| F1 | 500 mA fuse at the +bus tap | decision | a short in the chain or on wire 1 must not sit on the 15 A main fuse |
| D1 | 1N4007 across the chain, cathode to +bus, anode to node A | assortment | free-wheeling. **Across the coils only:** if it also spanned S1, the coil current would have no path when the button opens |
| S1 | emergency stop, positively opening **NC** contact, in the chain between node A and wire 1 | to be procured | switches 42 V DC / 0.14 A — any 22 mm mushroom switch is good for that; a block with a **second** NC contact would allow a direct feedback (§4) |
| Q1 | **2N5551**, NPN, TO-92 — **decided 2026-11-25**, on hand from the assortment | datasheet: V_CEO 160 V, I_C 600 mA, h_FE 80…250, V_CE(sat) 0.2 V at I_C = 50 mA / I_B = 5 mA, P_tot 625 mW ([onsemi](https://www.onsemi.cn/download/data-sheet/pdf/2n5550-d.pdf), [Diotec](https://diotec.com/request/datasheet/2n5551.pdf)) | 160 V leaves ample room above the 42 V bus. 140 mA × ≈ 0.3 V = 40 mW, no heat sink. Proof after assembly: U_CE < 0.3 V with the chain energized (§9.5) |
| R1 | **220 Ω** base series | calculated | (3.3 V − 0.75 V) / 220 Ω ≈ 11 mA base current, forced β ≈ 12 at 140 mA — saturated. The ESP32-S3 GPIO delivers that at its default drive strength (20 mA). The 1 kΩ named here until 2026-11-24 would not saturate: h_FE is specified only up to 50 mA |
| R2 | **10 kΩ** base–emitter | calculated | holds the base at ground on reset even against an internal 45 kΩ pull-up of the ESP32 — therefore 10 kΩ, not 100 kΩ |
| Q1 alternative | **IRL540N**, N-MOSFET, TO-220, with R1 = 100 Ω into the gate and R2 = 10 kΩ gate–source | datasheet: V_DSS 100 V, V_GS(th) 1.0…2.0 V, R_DS(on) 0.11 Ω at V_GS = 4 V ([Vishay IRL540](https://www.vishay.com/docs/91300/irl540.pdf), [Infineon IRL540N](https://www.infineon.com/part/IRL540N)) | if a logic-level MOSFET is at hand; R_DS(on) at 3.3 V is not specified, proof is U_DS < 0.5 V |
| Not suitable | 2N2222 (40 V), BC337 / BC547 (45 V), BC517 (30 V), D4184 modules (40 V), IRLZ44N (55 V), IRF520 (no logic-level gate) | datasheet | too little margin above 42 V, or not switchable with 3.3 V |
| Wire 1 | 0.5 mm², insulated like the power path | decision | carries bus voltage while the relays are dropped |
| Phase leads | ≥ 4 mm² | as before | 60 A peak |

> Until 2026-11-24 this section showed the 2N5551 with a 1 kΩ base resistor and no
> emergency stop in the chain; the manual emergency stop built on 2026-11-23 switches
> the chain by hand ([07-safety](07-safety.md) §3). The proposal above merges the two:
> the hand stays in the chain, and the firmware gets the second switch.

**Ground.** The coil current returns through the emitter of Q1 to bus −. It must **not**
run through the GND pin of the DevKit and the COMM ground to the controller — that wire
stays the only logic–power connection and carries signals. The source therefore gets its
own wire to the star point. The gate is still referenced correctly, because the ESP32
ground lies on the same node via COMM −: the 5 V regulator on the FSESC is a
non-isolated step-down, battery −, phase ground, COMM −, SENSE − and the USB shield are
one node. There are no separate ground levels on the stand.

**Option: feedback without an extra wire.** A divider from the collector node, R3 120 kΩ to
`PIN_ESTOP_FB` (GPIO2), R4 10 kΩ and a 3.3 V Zener from there to ground (42 V → 3.23 V;
48 V = `l_max_vin` → 3.69 V, hence the Zener as at the Hall inputs). The level says
something only while GPIO40 is low: HIGH = bus present **and** emergency stop not
pressed, LOW = pressed or bus gone. With GPIO40 high it is always LOW. Intended as a
check before releasing — "do not start with the emergency stop pressed" — not as an
emergency-stop detection during a run. That remains the current criterion from the
motor current ([07-safety](07-safety.md) §4).

**Failure modes of the one wire.** Broken → chain open → braked. Shorted to ground →
Q1 bypassed, but S1 still in the chain → the hand still works. Shorted to +bus → F1
blows (the chain is bridged, 140 mA becomes the fuse current) → braked.

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

### 9.4 Control by the ESP32 — firmware since 2026-11-25 (`brake.cpp`)

**Rule: the relays are energized as long as the firmware runs.** `brake::begin()` sets
GPIO40 high right after the settings are loaded. The firmware drops them for exactly one
thing: a **stop** (`s`, FAULT, end of a run), **at any speed**. Until the evening of
2026-11-25 this held only above 40 km/h (`RELAY_BRAKE_ABOVE_KMH`), where the controller
must not brake ([07-safety](07-safety.md) §3.2d); the brake comparison from 35.9 km/h then
showed that the controller with 20 A and the relay short decelerate identically
(2.8 km/h/s, 35 → 5 km/h in 13 s), because 20 A lie above the short-circuit current and
the controller ends in the hardware short anyway. The relays do the same without
regeneration and without a controller fault, so the builder decided: every stop via the
relays, the controller brakes only descents within a run. Everything else stays as it was — the
spin-down test coasts with the motor free, the zero check turns the drum by hand against
a free rotor, descents within a run brake via the controller. The price is 140 mA in the
coils while the stand is powered; the gain is that no start has to wait for contacts and
no hand check meets a shorted motor. N6 ("braked on reset") therefore holds for the
reset itself, until `begin()` runs — with R2 on the gate that is guaranteed by hardware.

| Situation | What happens |
|---|---|
| Reset, boot | GPIO40 low → braked (R2), then `begin()` → relays pull in. The first drive command is refused for `RELAY_SETTLE_MS` ("Relays settling"). |
| Stop (`s`, FAULT, end of a run), any speed | `vesc::release()` → `RELAY_DRIVER_OFF_MS` → GPIO40 low → motor shorted ("Relaisbremse: Motor kurzgeschlossen"). The controller stays released; N2 by software. |
| … while shorted | The criterion "no motor on the controller" is muted (`speedctl`), drive commands are refused ("Relaisbremse aktiv"). Speed from `revsrc`: drum marks, below 267 rpm also the controller's Hall sensors. |
| Drum at standstill for `RELAY_RELEASE_STILL_MS` | relays pull in again, the stand is ready. |
| Descent within a run (setpoint lowered) | Controller released, **never** braking via the controller. If the coasting time to the setpoint exceeds `DESCENT_MIN_COAST_S`, the relays brake **once**, full, and are pulled in again at setpoint + lead (`DESCENT_*`); the rest is coasted, then the controller takes over. At most one braking per setpoint, speed only from the drum marks. The console logs braking point and release point (`# Abstieg: …`). First attempt on 2026-11-25 read the speed from the controller, which sees 0 rpm with the motor shorted: the relays switched 26 times, every switch knocked the weighing arm into its end stop. |
| Hand emergency stop during a run | unchanged: relays drop, the controller reports no current, the criterion fires rightly. |
| `RELAY_BRAKE_WIRED` = false | the module does nothing: GPIO40 stays low, a stop brakes via the controller below `BRAKE_MAX_KMH` and coasts above. true since the rebuild of 2026-11-25. |

Constants in [03-parameters](03-parameters.md) §6: `RELAY_BRAKE_WIRED`,
`RELAY_DRIVER_OFF_MS`, `RELAY_SETTLE_MS`,
`RELAY_RELEASE_STILL_MS`. The state is in `/api/state` as `relayBrake` and in the
console status line.

Not built, as a second step: handing over to the controller brake below the threshold
(the short-circuit torque falls with speed, the 20 A do not). First the plain version
has to run.

### 9.5 Checks before the first release — and after the first programmatic brake

1. **Chain without module:** between BATT + and wire 1 about 257 Ω with S1 closed
   (3 coils in series, 140 mA at 36 V), infinite with S1 pressed. Each coil 85↔86 reads the
   same in both directions (no built-in diode).
2. **Module without chain:** GPIO40 high → collector to emitter conducts (diode test);
   GPIO40 low → open.
3. **Together, motor disconnected:** GPIO40 high → the relays click, U_CE < 0.3 V,
   chain current ≈ 140 mA. GPIO40 low → they drop. S1 pressed with GPIO40 high → they
   drop.
4. **Reset of the ESP32 with the chain energized** → they drop.
5. Only then connect the motor and repeat the phase-sequence checks of §10.6.
6. **After the first firmware-triggered brake from above 40 km/h:** `uptime` and
   `faults` in the controller terminal. A short uptime would mean the controller still
   resets although the driver was disabled first — then `RELAY_DRIVER_OFF_MS` is too
   short or the cause lies elsewhere.

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
> `tools/console.py ":foc_sensors_detect_apply 10"`; beforehand `":hall_analyze 10"`
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
| `s` | STOP via the relay brake at any speed (§9.4); `sr` existed for a few hours on 2026-11-25 for the brake comparison and is gone |
| `c` | let coast |
| `t` | tare the load cell (only at standstill) |
| `C0` `Cm<kg>` `Cz` `Ce` `Ca` `Cx` | calibration directly at the cell (since 2026-11-28; Setup → Calibration): zero with the stirrup, read a mass, zero again, evaluate, accept, cancel. `k<kg>` (single point on the calibration arm) and `K<kg>` (check-weight mass) were dropped |
| `d<m>` `a<m>` | drum circumference · cell arm (`b<m>`, calibration arm, dropped 2026-11-28) |
| `z` | reset settings to the defaults from `config.h` |
| `i` | **status**: controller, load cell, sensors, operator link |
| `g` | zero the sensor counters (wiring test, see below) |
| `m0` / `m1` | idle / measurement run: drives through the measurement speeds in sequence |
| `R` / `Rk` / `Rs` / `Rn` / `Rx` / `Rm<k>=<v>` | Guided measurement run (`run.h`, [11-review](11-review.md) §3): start, main button, secondary button, again, abort, metadata (t tire, w wheel, b bar, c °C, l kg, p mm, n note, d Unix time). In the web UI behind *Measurement*. Report and data under `/vmrr/<id>/` ([10-firmware](10-firmware.md) §11) |
| `h<kmh>` / `h-<kmh>` | add / remove measurement speed (list in NVS, sorted) |
| `l<N>` | wheel load; without it no measurement run starts |
| `n<A0> <B0>` / `w<A1> <B1>` | set idle-loss and wheel-loss model by hand; `m0` fits the idle-loss model itself, `u` the wheel-loss model |
| `u` / `y<kg>` | assistant spin-down + idle (drum up, lift wheel, both at once) / wheel mass for J_wheel |
| `j<s>` | live telemetry to the console, every 200 ms for s seconds — for logging with `tools/console.py` |
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
U_wheel = U_drum · drum revolutions / wheel revolutions     (same time window)
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

As of 2026-11-28: **directly at the cell**, Setup → Calibration. The arm is lifted off the
Bowden cable, the stirrup hangs on the cable and stays on during the zero; then 200, 500,
1000 and 2000 g go into the stirrup one after the other.

```
F_cell    = m · g
N_per_cnt = 1 / |b|      from the least-squares line counts = a + b · F_cell
M         = F_cell · L_cell
```

The cell arm therefore enters every torque. Until 2026-11-28 the weight hung on the
calibration arm (`F_cell = m · g · L_cal / L_cell`); that and the check weight were dropped.
Procedure and linearity: [09-measurement-theory](09-measurement-theory.md) §5.1.

> **As long as it is not calibrated, there are no forces, only raw values.** The UI
> then shows dashes and "not calibrated". That is intentional: a number that
> looks like a measurement is worse than a gap.

### 14.1b Where the values determined on the setup live

Drum circumference, cell arm and the calibration factor are measured **on the setup**,
not at the desk. A calibration run that forces compiling and flashing is
not done — and a calibration run not done is exactly the source of invented
numbers that the project rule is written against.

That is why these values live in the **NVS** of the ESP32 (`firmware/src/settings.*`).

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

## 15 microSD via an SD adapter

As of 2026-11-25 (drawing 2026-11-26). Drawing: `hardware/sd-plan.pdf` (source `hardware/sd_plan.py`).
Firmware: [10-firmware](10-firmware.md) §11.

The passive adapter that comes with a microSD card serves as the socket: wires are
soldered to its nine contacts, the microSD is plugged in. The adapter only passes the
contacts through — no level shifter, no regulator. The card runs on 3.3 V and the
ESP32-S3 talks 3.3 V, so it connects directly. Modules with a 5 V regulator and level
shifter are not needed and add nothing on a 3.3 V host.

### 15.1 Contacts

Seen onto the contacts, chamfered corner top left, the order is **9 1 2 3 4 5 6 7 8**:
the SD card kept the seven MMC contacts 1…7 and added 8 and 9 at the edges, 9 under the
chamfer next to 1. Count on the adapter itself before soldering.

| SD contact | SD name | SPI function | ESP32-S3 | At the adapter |
|---|---|---|---|---|
| 1 | DAT3/CD | CS | GPIO 10 | 10 kΩ to 3V3 |
| 2 | CMD | MOSI (DI) | GPIO 11 | |
| 3 | VSS1 | GND | GND | |
| 4 | VDD | 3.3 V | 3V3 | 10 µF + 100 nF to contact 3 |
| 5 | CLK | SCK | GPIO 12 | |
| 6 | VSS2 | GND | GND | |
| 7 | DAT0 | MISO (DO) | GPIO 13 | 10 kΩ to 3V3 |
| 8 | DAT1 | — | — | 10 kΩ to 3V3 |
| 9 | DAT2 | — | — | 10 kΩ to 3V3 |

- **Pull-ups.** CS high keeps the card deselected while the ESP32 boots. DAT1 and DAT2
  are unused in SPI mode; left open they can disturb the card. MISO floats while the
  card is deselected.
- **Both ground contacts** (3 and 6) are connected.
- **Decoupling** at the adapter: the card draws current peaks while writing.
- **Wires** as short as possible (under 15 cm), a ground wire next to CLK. The SPI clock
  is `SD_SPI_KHZ` = 10 MHz; halve it first if reads fail. Needed are an estimated 2.5–3.5 kB/s of
  raw data ([10-firmware](10-firmware.md) §11.3), so even 1 MHz would be enough for logging — the clock only matters for
  downloads.
- The **write-protect slider** of the adapter has no effect in SPI mode: it is only a
  switch for a card reader to read.
- **No card-detect contact.** The firmware notices insertion and removal itself by
  polling (`STORAGE_CHECK_MS`).

### 15.2 Why GPIO 10…13

They are the IO-MUX pins of SPI2 (FSPICS0, FSPID, FSPICLK, FSPIQ) and at the same time
the default SPI pins of the DevKitC-1. The signals go straight to the peripheral, not
through the GPIO matrix. GPIO 10 was reserved for CS from the start; 11…13 became free
with the D-pad on 2026-11-22.

