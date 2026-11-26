#pragma once
//
// Motor controller over UART -- the bare essentials.
//
// The setpoint is the SPEED in ERPM, because that is how the controller computes, and it
// has its own PID loop for it: it holds a plateau itself, the ESP32 does not have to
// readjust. The mechanical speed would be ERPM / number of pole pairs -- but that is not
// measured (MOTOR_PULSES_PER_REV is TBD), so nothing is converted here.
// Better to show ERPM than an invented speed.
//
// Nine values are read (VESC_POLL_MS): FET and motor temperature, motor and
// input current, duty cycle, speed, bus voltage, fault code and status byte.
//
// The motor temperature was left out at first, because without a sensor on the
// TMP input the controller delivers a fantasy number for it WITHOUT an error message.
// Since A5 was answered on 2026-11-17 (10 k NTC in the stator, confirmed three times),
// the value is real and is requested.
//
// DEAD MAN: the controller shuts off if no setpoint arrives for longer than timeout_msec.
// So the setpoint is repeated every VESC_ALIVE_MS while driving.
// CAUTION: value requests do NOT reset the timeout -- only setpoints.
// When not driving, nothing is sent either; if the firmware then hangs,
// the controller's timeout takes effect. That is the second, independent level.
//
#include <Arduino.h>

namespace vesc {

// Command ids, numeric values from datatypes.h of the VESC firmware.
enum : uint8_t {
  COMM_FW_VERSION          = 0,
  COMM_SET_DUTY            = 5,
  COMM_SET_CURRENT         = 6,
  COMM_SET_CURRENT_BRAKE   = 7,
  COMM_SET_RPM             = 8,
  COMM_TERMINAL_CMD        = 20,   // text command to the controller's terminal
  COMM_PRINT               = 21,   // text answer of the terminal
  COMM_ALIVE               = 30,
  COMM_GET_VALUES_SELECTIVE = 50,
};

struct Values {
  bool     seen   = false;   // at least one packet received
  uint32_t ageMs  = 0;       // since the last packet
  float    tempFet = 0.0f;   // °C
  float    tempMotor = 0.0f; // °C, 10 k NTC in the stator (A5)
  float    currentMotor = 0.0f;  // A, mean since the last request
  float    currentIn    = 0.0f;  // A, same. COMPUTED on hardware 4.x, not measured
  float    duty   = 0.0f;    // −1 … +1
  int32_t  erpm   = 0;
  int32_t  tacho  = 0;       // controller tacho counter, 6 steps per electrical revolution, also counts when turned by hand
  float    vIn    = 0.0f;    // V
  uint8_t  fault  = 0;
  uint8_t  status = 0;       // bit0 controller timeout, bit1 kill switch
  // For the raw data (rawlog.h): packet counter, micros() on reception and the
  // fields in the resolution in which the controller sends them.
  uint32_t count = 0;
  uint32_t rxUs  = 0;
  struct { int16_t tFet10 = 0, tMot10 = 0, duty1000 = 0; int32_t iMot100 = 0, iIn100 = 0; uint16_t vIn10 = 0; } raw;
};

void begin();
void service();              // call cyclically; non-blocking

const Values& values();
bool connected();            // a packet within the last second

// Setpoint. Limited to ±VESC_ERPM_MAX and repeated every VESC_ALIVE_MS
// until release() comes.
void setErpm(int32_t erpm);
int32_t targetErpm();
bool driving();
bool braking();              // SET_CURRENT_BRAKE applied (command s)

// Duty cycle as the manipulated variable, −1 … +1. Repeated every VESC_ALIVE_MS like
// the speed setpoint.
//
// This path is used because the VESC speed controller is tuned uselessly for this
// motor and s_pid_min_erpm = 900 blocks the lowest test step
// (docs/06-open-questions.md A15). Via the duty cycle the stand was driven up to
// 99.2 km/h on 2026-11-17, strictly linear to the no-load characteristic.
void setDuty(float duty);
float targetDuty();

// Braking with a given current. The ONSET regenerates for milliseconds;
// on the power supply overvoltage from ~56 km/h (docs/07-safety.md §3.2d). So only
// call below BRAKE_MAX_KMH. A duty-cycle-0 command would not be a short in FOC,
// but a PI ramp with regeneration -- that is why it does not exist here.
void setBrakeCurrent(float amps);

// Drive without current, drum coasts freely. That is the normal way to stop
// and corresponds to IDrive::stop() -- NO substitute for the emergency stop.
void release();

// Firmware version of the controller. Decides among other things whether
// foc_short_ls_on_zero_duty exists (only from 6.05).
bool fwKnown();
uint8_t fwMajor();
uint8_t fwMinor();
const char* hwName();        // "410" on the FSESC 4.20

// Fault code in plain text. Unknown codes are reported as such and not
// mapped to a wrong name.
const char* faultText(uint8_t code);

// The controller reports a fault code only for m_fault_stop_time_ms (500 ms), then
// it is 0 again. Too short for an eye -- so the last code stays here:
// lastFault() and how long ago that was. Every new fault goes to the console as
// "# VESC FAULT". The terminal delivers the history (":faults").
uint8_t  lastFault();
uint32_t lastFaultAgeMs();   // 0xFFFFFFFF if none ever came
uint32_t faultCount();

// PASS-THROUGH for troubleshooting via the USB console (docs/04-hardware.md 14.1):
//   terminal(text)   sends a text command to the controller's terminal, as the
//                    VESC Tool does ("faults", "hall_analyze 5", ...). The
//                    answers come as COMM_PRINT and are written to the console
//                    as "# VESC> ...".
//   raw(payload)     sends an arbitrary payload (id + data). Answers the driver
//                    does not process itself appear as "# V <hex>" -- this way
//                    e.g. COMM_GET_MCCONF (14) can be fetched.
// Both are tools, not an operating path: the firmware evaluates none of it.
void terminal(const char* text);
void raw(const uint8_t* payload, size_t len);

}  // namespace vesc
