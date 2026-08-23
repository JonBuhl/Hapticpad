#pragma once

#include <Arduino.h>
#include <SimpleFOC.h>

// ---------------------------------------------------------------------------
// SmartKnob style haptics
//
// The motor is driven in torque (voltage) mode. A software model decides how
// much torque to apply based on where the shaft sits relative to a "detent
// centre". Positions are counted as the shaft crosses from one detent to the
// next, which gives us exactly one scroll tick per detent crossing.
//
// The model is deliberately close to scottbez1/smartknob's motor task so the
// tuning values behave in a familiar way:
//   - position_width  : angular size of one position
//   - num_positions   : 0 = unbounded, >0 = bounded range with endstops
//   - detent_strength : torque gain pulling towards the nearest detent centre
//   - endstop_strength: torque gain used once the shaft is past the last
//                       position of a bounded range
//   - snap_point      : how far (in widths) the shaft has to travel before the
//                       detent centre moves on. >1 gives an over centre
//                       "click", 0.5 makes the knob snap to the nearest
//                       position.
//   - detent_positions: optional sparse list. Positions that are not in the
//                       list get no detent torque (magnetic detents).
// ---------------------------------------------------------------------------

#define WHEEL_CLICKY        0
#define WHEEL_TWIST         1
#define WHEEL_MOMENTUM      2
#define WHEEL_FREE_SCROLL   3
#define WHEEL_ENDSTOP       4
#define WHEEL_FRICTION      5
#define WHEEL_SNAP          6
#define WHEEL_MAGNETIC      7
#define WHEEL_MODE_COUNT    8

// Pseudo mode used internally while the on screen menu is open.
#define WHEEL_MODE_MENU     255

// Stored per macro button: the button has no <WheelMode> of its own, so it is a
// plain macro button rather than a wheel domain.
#define WHEEL_DOMAIN_NONE   255

const char *wheelModeNames[WHEEL_MODE_COUNT] = {
  "Clicky", "Twist", "Momentum", "Free", "Endstop", "Friction", "Snap", "Magnetic"
};

#define HAPTIC_MAX_DETENT_POSITIONS 16

// Dead zone keeps the motor quiet when the shaft is parked on a detent centre.
#define HAPTIC_DEAD_ZONE_PERCENT     0.2f
#define HAPTIC_DEAD_ZONE_RAD         (1.0f * PI / 180.0f)
// Above this shaft speed the position reading is not trustworthy, so no torque.
#define HAPTIC_IDLE_VELOCITY_LIMIT   60.0f
// Speed (rad/s) at which the friction torque reaches its configured value.
#define HAPTIC_FRICTION_VELOCITY     1.0f
// Damping term of the haptic PID, tames the detent overshoot.
#define HAPTIC_PID_DAMPING           0.04f
// Torque ceiling for detents. Endstops are allowed the full voltage limit.
#define HAPTIC_DETENT_VOLTAGE        3.0f
// Voltage limit used by the non haptic modes (Momentum), as per the original firmware.
#define MOTOR_BASE_VOLTAGE_LIMIT     3.0f
// PWM carrier frequency for the 6-PWM driver. SimpleFOC default (~20 kHz) sits
// inside the audible range and makes the motor whine under load; 50 kHz is
// above it. Applied once before driver.init() in setup().
#define MOTOR_PWM_FREQUENCY          50000
// A snap point of exactly 0.5 would sit right on the boundary between two
// positions and could rattle between them, so keep a little hysteresis.
#define HAPTIC_MIN_SNAP_POINT        0.55f
// A single pass of the model consumes every position the wheel crossed since
// the last one. This caps how many that can be so a flick of the wheel cannot
// dump hundreds of scroll ticks into the PC in one go.
#define HAPTIC_MAX_STEPS_PER_LOOP    32
// Scroll ticks per radian for the continuous modes (Free, Friction). Matches
// the feel of the original firmware.
#define WHEEL_TICKS_PER_RADIAN       10.0f

// ---- User configurable haptic settings (see <Settings> in config.xml) ----
float hapticVoltageLimit   = 3.0f;   // <Haptic_VoltageLimit>
float hapticDetentStrength = 2.5f;   // <Haptic_DetentStrength>
float hapticEndstopStrength = 3.0f;  // <Haptic_EndstopStrength>
float hapticSnapPoint      = 1.1f;   // <Haptic_SnapPoint>
uint8_t hapticRange        = 24;     // <Haptic_Range>

uint8_t hapticDetentPositions[HAPTIC_MAX_DETENT_POSITIONS] = {0, 6, 12, 18};
uint8_t hapticDetentPositionCount = 4;  // <Haptic_MagneticPositions>

uint16_t clickyDetents   = 40;    // <Clicky_Detents>
float clickyStrength     = 2.5f;  // <Clicky_Strength>
float twistStrength      = 2.0f;  // <Twist_Strength>
float twistRangeDeg      = 60.0f; // <Twist_Range>
float frictionStrength   = 1.0f;  // <Friction_Strength>
float snapStrength       = 3.0f;  // <Snap_Strength>
uint16_t snapDetents     = 20;    // <Snap_Detents>
float snapPoint          = 0.55f; // <Snap_Point>
float magneticStrength   = 2.5f;  // <Magnetic_Strength>
uint16_t magneticDetents = 24;    // <Magnetic_Detents>

// ---- Live state of the haptic model ----
// Everything below is in the ENCODER frame (encoder.getAngle()), never in the
// motor frame. encoder.getAngle() is valid whether or not the motor is enabled
// and whether or not FOC alignment succeeded, so counting positions from it
// cannot be broken by the motor. The only place the motor frame is used is when
// the torque is finally handed to motor.move(), see hapticStep().
float hapticWidth = 0.157f;          // radians per position
int32_t hapticNumPositions = 0;      // 0 = unbounded
float hapticActiveDetent = 0.0f;     // detent gain in use
float hapticActiveEndstop = 0.0f;    // endstop gain in use
float hapticActiveSnapPoint = 1.1f;
uint8_t hapticActivePositionCount = 0; // 0 = every position is a detent
float hapticActiveFriction = 0.0f;

float hapticDetentCentre = 0.0f;
int32_t hapticPosition = 0;
uint8_t hapticActiveMode = WHEEL_CLICKY;

PIDController hapticPID = PIDController(4.0f, 0.0f, HAPTIC_PID_DAMPING, 10000.0f, HAPTIC_DETENT_VOLTAGE);

// ---- Haptic test page ----
volatile bool hapticTestActive = false;
volatile uint8_t hapticTestMode = WHEEL_CLICKY;
uint8_t hapticMenuSelection = 0;
