// How far the wheel has been turned since the last continuous scroll tick was
// sent. Only touched by the core 0 wheel modes, unlike the shared
// lastEncoderAngle which Momentum mode also uses from core 1.
float wheelLastAngle = 0.0f;

// Moves the menu selection by one entry per step. menuScroll() only ever moves a
// single entry no matter what it is handed, so a run of steps has to be fed to
// it one at a time.
void menuScrollSteps(int steps){
  if(steps == 0){
    return;
  }

  int count = abs(steps);
  if(count > HAPTIC_MAX_STEPS_PER_LOOP){
    count = HAPTIC_MAX_STEPS_PER_LOOP;
  }

  int8_t direction = (steps > 0) ? 1 : -1;

  for(int i = 0; i < count; i++){
    menuScroll(direction);
  }
}

// ---------------------------------------------------------------------------
// Wheel domain output
//
// Nothing a wheel domain sends goes out from the wheel mode itself any more.
// Two things went wrong when it did:
//
//   * TinyUSB throws a HID report away while the previous one is still sitting
//     in the endpoint waiting for the host to poll it. "press, delay(5),
//     release" per detent is two reports well inside one polling interval, so
//     ticks went missing, and how many went missing depended on how fast the
//     wheel was turned. A quick flick forward and a careful turn back lost
//     different numbers of ticks, which is exactly the drift that shows up when
//     turning the wheel back and forth.
//   * A run of up to HAPTIC_MAX_STEPS_PER_LOOP taps blocked core 0, and core 0
//     is the FOC loop.
//
// So taps are queued here and drained one report per pass of loop(), only ever
// when the endpoint is actually free.
// ---------------------------------------------------------------------------

#define WHEEL_TAP_QUEUE_LEN 64

// Written by whichever core is running the wheel mode, drained on core 0. Only
// the producer ever moves the tail and only the consumer ever moves the head,
// which keeps it sound even for Momentum, the one mode that scrolls from core 1.
uint8_t wheelTapQueue[WHEEL_TAP_QUEUE_LEN];
volatile uint8_t wheelTapHead = 0; // slot being sent right now
volatile uint8_t wheelTapTail = 0; // next free slot
bool wheelTapHeld = false;         // the tap at the head is pressed, not released

bool wheelTapPending(){
  return wheelTapHead != wheelTapTail;
}

// Presses or releases one wheel domain key. Consumer usages and keyboard keys
// look the same to the caller. Returns false when the key maps to nothing this
// firmware can send, so the caller can drop it instead of waiting for a release
// that will never come.
bool sendWheelKeyState(uint8_t key, bool pressed){
  uint16_t consumer = convertConsumerKeycode(key);
  if(consumer != 0){
    uint16_t report = pressed ? consumer : 0;
    usb_keyboard.sendReport(REPORT_ID_CONSUMER, &report, sizeof(report));
    return true;
  }

  uint8_t keycode[6] = { 0 };
  int modifier = checkModifiers(key);

  if(modifier == 0){
    keycode[0] = convertKeycode(key);
    if(keycode[0] == 0){
      return false;
    }
  }

  if(pressed){
    usb_keyboard.keyboardReport(REPORT_ID_KEYBOARD, modifier, keycode);
  } else {
    usb_keyboard.keyboardRelease(REPORT_ID_KEYBOARD);
  }

  return true;
}

void wheelTapQueuePush(uint8_t key){
  uint8_t next = (uint8_t)((wheelTapTail + 1) % WHEEL_TAP_QUEUE_LEN);

  if(key == 0 || next == wheelTapHead){
    // A full queue means the wheel is being spun far faster than the PC can be
    // told about. 63 taps is well past a full turn of a bounded domain, so this
    // only ever bites on an unbounded one that is being flicked.
    return;
  }

  wheelTapQueue[wheelTapTail] = key;
  wheelTapTail = next;
}

void wheelTapQueuePop(){
  wheelTapHead = (uint8_t)((wheelTapHead + 1) % WHEEL_TAP_QUEUE_LEN);
}

// Sends at most one report per call, and only when the endpoint has drained.
// A report that has drained is a report the host has already polled, so the
// press and the release can never be merged into nothing.
void wheelTapTick(){
  if(!wheelTapPending() || !usb_keyboard.ready()){
    return;
  }

  uint8_t key = wheelTapQueue[wheelTapHead];

  if(!wheelTapHeld){
    if(sendWheelKeyState(key, true)){
      wheelTapHeld = true;
    } else {
      wheelTapQueuePop(); //nothing sendable, do not sit on it
    }
    return;
  }

  sendWheelKeyState(key, false);
  wheelTapHeld = false;
  wheelTapQueuePop();
}

// One scroll tick becomes one tap of the direction's action. Positive scroll is
// the direction that would otherwise scroll up, so that is the WheelUp action.
void wheelDomainOutput(int scroll){
  int8_t domain = activeWheelDomain;
  if(domain < 0 || domain >= 6){
    return;
  }

  uint8_t key = (scroll > 0) ? buttonWheelUp[domain] : buttonWheelDown[domain];
  if(key == 0){
    return;
  }

  int count = abs(scroll);
  if(count > HAPTIC_MAX_STEPS_PER_LOOP){
    count = HAPTIC_MAX_STEPS_PER_LOOP;
  }

  for(int i = 0; i < count; i++){
    wheelTapQueuePush(key);
  }
}

// Every wheel mode funnels its ticks through here, so this is the one place that
// decides where they end up.
void wheelScrollOutput(int scroll){
  if(scroll == 0){
    cancelWheelAction();
    return;
  }

  // The haptic test page runs a mode purely so it can be felt. Nothing is sent
  // to the PC and nothing moves the mode list underneath it.
  if(hapticTestActive){
    cancelWheelAction();
    return;
  }

  // While the menu is open the wheel drives the selection, never the PC.
  // menuWheel() normally gets here first, but routing it centrally also covers
  // the window where core 1 has opened the menu and core 0 is still running the
  // profile's own wheel mode.
  if(profileSelectMenu){
    cancelWheelAction();
    menuScrollSteps(scroll);
    return;
  }

  // An active wheel domain with direction actions taps keys instead of
  // scrolling. Domains without them fall through and scroll as usual, just with
  // the domain's own haptic feel.
  if(wheelDomainSendsKeys(activeWheelDomain)){
    cancelWheelAction();
    wheelDomainOutput(scroll);
    return;
  }

  scroll = constrain(scroll, -127, 127); //mouseScroll takes an int8_t

  wheelActionCheck();
  usb_mouse.mouseScroll(0, scroll, 0);
}

// Shared driver for every detent based mode. The haptic model reports how many
// detent positions were crossed, and each one becomes exactly one scroll tick.
void detentWheel(uint8_t mode){
  if(wheelModeChanged){
    hapticBegin(mode);
    wheelModeChanged = false;
  }

  int delta = hapticStep();

  if(delta != 0){
    // Same direction convention as the original firmware, where the scroll was
    // the negated change in wheel position.
    wheelScrollOutput(-delta);
  } else {
    cancelWheelAction();
  }
}

void notchyWheel(){
  detentWheel(WHEEL_CLICKY);
}

void endstopWheel(){
  detentWheel(WHEEL_ENDSTOP);
}

void snapWheel(){
  detentWheel(WHEEL_SNAP);
}

void magneticWheel(){
  detentWheel(WHEEL_MAGNETIC);
}

// Spring loaded: the wheel is pulled back to where it started and the scroll
// rate rises with how far it is deflected.
void twistScroll(){
  if(wheelModeChanged){
    hapticBegin(WHEEL_TWIST);
    targetAngle = encoder.getAngle();
    mouseTimer = millis();
    wheelModeChanged = false;
  }

  hapticStep(); //single position range, so the model only ever applies the spring

  float deflection = hapticDeflection();

  if(fabsf(deflection) > 0.2f){
    interval = 50.0f / fabsf(deflection);

    if(mouseTimer + (long)interval < (long)millis()){
      wheelScrollOutput(deflection > 0 ? -1 : 1);
      mouseTimer = millis();
    } else {
      cancelWheelAction();
    }
  } else {
    cancelWheelAction();
  }
}

// Turns wheel movement into continuous scroll ticks. Only the part of the
// movement that became whole ticks is consumed, so the leftover carries into the
// next call instead of being rounded away.
void continuousScrollOutput(){
  float delta = encoder.getAngle() - wheelLastAngle;
  int scroll = (int)(-delta * WHEEL_TICKS_PER_RADIAN);

  if(scroll != 0){
    wheelLastAngle -= (float)scroll / WHEEL_TICKS_PER_RADIAN;
    wheelScrollOutput(scroll);
  } else {
    cancelWheelAction();
  }
}

// Constant resistance, continuous scrolling.
void frictionWheel(){
  if(wheelModeChanged){
    hapticBegin(WHEEL_FRICTION);
    wheelLastAngle = encoder.getAngle();
    wheelModeChanged = false;
  }

  motor.move(hapticFrictionTorque());

  continuousScrollOutput();
}

// Menu navigation. The wheel runs the Clicky haptic model so the list is
// physically clicked through, and every detent crossing moves the selection by
// exactly one entry. This is deliberately the same counting path the detent
// scroll modes use, rather than a second one quantising the raw encoder angle.
void menuWheel(){
  if(wheelModeChanged){
    hapticBegin(WHEEL_CLICKY);
    cancelWheelAction();
    wheelModeChanged = false;
  }

  int delta = hapticStep();

  cancelWheelAction();

  if(delta != 0){
    // Negated to match the scroll output convention, so a turn that would have
    // scrolled a page up moves up the list.
    menuScrollSteps(-delta);
  }
}

void wheelActionCheck(){
  wheelKeyTimer = millis();
  if(wheelAction != 0 && !wheelKeyPressed){
    int modifier = checkModifiers(wheelAction);
    uint8_t keycode[6] = { 0 };
    uint16_t consumer = convertConsumerKeycode(wheelAction);
    if(consumer != 0){
      sendConsumerKey(consumer);
      return;
    }
    if(modifier != 0){
      usb_keyboard.keyboardReport(REPORT_ID_KEYBOARD, modifier, 0);
    } else {
      keycode[0] = convertKeycode(wheelAction);
      usb_keyboard.keyboardReport(REPORT_ID_KEYBOARD, 0, keycode);
    }
    wheelKeyPressed = true;
    delay(10); //Slight delay to ensure keyboard report has been sent before mouse move
  }
}

void cancelWheelAction(){
  if(wheelKeyPressed && wheelKeyTimer + 100 < millis()){
    usb_keyboard.keyboardReport(REPORT_ID_KEYBOARD, 0, 0);
    wheelKeyPressed = false;
  }
}

void freeScroll(){
  if(wheelModeChanged){
    motor.disable();
    motor.move(0); //keeps the motor state fresh, applies nothing while disabled
    decelDetected = false;
    decelerating = false;
    target_velocity = 0;
    last_velocity = 0;
    wheelLastAngle = encoder.getAngle();
    cancelWheelAction();
    wheelModeChanged = false;
  }

  continuousScrollOutput();
}

void freeSpinning(){
  if(wheelModeChanged){
    motor.disable();
    motor.controller = MotionControlType::velocity;
    motor.voltage_limit = MOTOR_BASE_VOLTAGE_LIMIT; //haptic modes may have raised it
    motor.PID_velocity.I = Momentum_I; //0;
    motor.PID_velocity.P = Momentum_P; //0.3f;
    decelDetected = false;
    decelerating = false;
    target_velocity = 0;
    last_velocity = 0;
    lastEncoderAngle = encoder.getAngle();
    wheelModeChanged = false;
  }

  float vel = encoder.getVelocity(); //Read current encoder velocity

  if(!decelerating){
    if(abs(last_velocity - vel) > 0.1){//Detected deceleration
      if(decelDetected){
        if(debounceTimer + 100 < millis()){
          last_velocity = vel;
          if(vel > 0){
            target_velocity = vel + 2; //Set current velocity as target
          } else {
            target_velocity = vel - 2; //Set current velocity as target
          }
          if(abs(target_velocity) > 6){
            motor.enable();
          }
          debounceTimer = millis();
        }
      } else {
        decelDetected = true;
        debounceTimer = millis();
      }
    } else {
      decelDetected = false;
      last_velocity = vel;
    }
  }

  if(motor.enabled){
    if(vel > 0.2){ //Moving in the positive direction
      if(aceltimer + 400 < micros()){
        target_velocity -= 0.02;
        aceltimer = micros();
        decelerating = true;
      }
    } else if(vel < -0.2){ //Moving in the negative direction
      if(aceltimer + 400 < micros()){
        target_velocity += 0.02;
        aceltimer = micros();
        decelerating = true;
      }
    } else {
      motor.disable();

      encoderAngle = encoder.getAngle();
      lastEncoderAngle = encoderAngle;

      decelerating = false;
      debounceTimer = millis();
      last_velocity = vel;
      target_velocity = 0;
    }
    motor.move(target_velocity);
  }
}
