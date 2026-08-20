// ---------------------------------------------------------------------------
// SmartKnob style haptic motor model. See haptics.h for the parameter list.
// Everything in here runs on core 0 from loop().
// ---------------------------------------------------------------------------

// The encoder and the motor do not necessarily agree on which way is positive.
// The model itself works in the encoder frame, this returns the sign needed to
// push a torque back out into the motor frame.
//
// Careful: motor.sensor_direction is Direction::UNKNOWN (== 0) until initFOC()
// has aligned the sensor, and motor.shaft_angle is
// sensor_direction * encoder.getAngle() - sensor_offset, so shaft_angle is
// stuck at zero for as long as the direction is unknown. That is exactly why
// nothing in the counting path is allowed to read shaft_angle. For the torque
// sign, an unknown direction is treated as CW.
int8_t hapticSensorSign(){
  return (motor.sensor_direction == Direction::CCW) ? -1 : 1;
}

bool hapticIsDetentPosition(int32_t position){
  if(hapticActivePositionCount == 0){
    return true; //every position is a detent
  }

  for(uint8_t i = 0; i < hapticActivePositionCount; i++){
    if((int32_t)hapticDetentPositions[i] == position){
      return true;
    }
  }
  return false;
}

// Picks the starting position of a bounded range. Centred, but for sparse
// detents the nearest listed detent is used so the wheel starts in a notch.
int32_t hapticStartPosition(){
  if(hapticNumPositions <= 1){
    return 0;
  }

  int32_t centre = hapticNumPositions / 2;

  if(hapticActivePositionCount == 0){
    return centre;
  }

  int32_t best = hapticDetentPositions[0];
  int32_t bestDistance = abs(best - centre);

  for(uint8_t i = 1; i < hapticActivePositionCount; i++){
    int32_t candidate = hapticDetentPositions[i];
    int32_t distance = abs(candidate - centre);
    if(distance < bestDistance){
      best = candidate;
      bestDistance = distance;
    }
  }

  return constrain(best, (int32_t)0, hapticNumPositions - 1);
}

float hapticPositionWidth(uint16_t detentsPerTurn){
  if(detentsPerTurn < 1){
    detentsPerTurn = 1;
  }
  return TWO_PI / (float)detentsPerTurn;
}

// Loads the model with the profile for the requested wheel mode and re-centres
// it on the current shaft position.
void hapticBegin(uint8_t mode){
  motor.disable();
  motor.controller = MotionControlType::torque;
  motor.voltage_limit = hapticVoltageLimit;
  motor.move(0); //refreshes shaft_velocity, returns early while disabled

  // Defaults, individual modes override what they need.
  hapticActiveMode = mode;
  hapticWidth = hapticPositionWidth(clickyDetents);
  hapticNumPositions = 0;
  hapticActiveDetent = 0.0f;
  hapticActiveEndstop = hapticEndstopStrength;
  hapticActiveSnapPoint = hapticSnapPoint;
  hapticActivePositionCount = 0;
  hapticActiveFriction = 0.0f;

  switch(mode){
    case WHEEL_CLICKY:
      // Unbounded detents at the standard pitch.
      hapticActiveDetent = clickyStrength;
      break;

    case WHEEL_TWIST:
      // A single position with endstops on both sides behaves as a spring that
      // always pulls the wheel back to where it started.
      hapticNumPositions = 1;
      hapticWidth = radians(twistRangeDeg);
      hapticActiveEndstop = twistStrength;
      break;

    case WHEEL_ENDSTOP:
      // Free inside the range, hard walls at both ends.
      hapticNumPositions = hapticRange;
      break;

    case WHEEL_FRICTION:
      hapticActiveFriction = frictionStrength;
      break;

    case WHEEL_SNAP:
      // A snap point of ~0.5 means the shaft is always driven towards the
      // nearest position instead of being held over centre.
      hapticWidth = hapticPositionWidth(snapDetents);
      hapticActiveDetent = snapStrength;
      hapticActiveSnapPoint = snapPoint;
      break;

    case WHEEL_MAGNETIC:
      // Bounded range, but only the listed positions get detent torque.
      hapticWidth = hapticPositionWidth(magneticDetents);
      hapticNumPositions = magneticDetents;
      hapticActiveDetent = magneticStrength;
      hapticActivePositionCount = hapticDetentPositionCount;
      break;

    default:
      break;
  }

  if(hapticNumPositions > 0){
    hapticPosition = hapticStartPosition();
  } else {
    hapticPosition = 0;
  }

  hapticDetentCentre = encoder.getAngle(); //encoder frame, see haptics.h

  hapticPID.P = hapticActiveDetent * 4.0f;
  hapticPID.I = 0.0f;
  hapticPID.D = HAPTIC_PID_DAMPING;
  hapticPID.output_ramp = 10000.0f;
  hapticPID.limit = HAPTIC_DETENT_VOLTAGE;
  hapticPID.reset();

  motor.enable();
}

// Constant resistance that always opposes the direction of travel. Tapered
// below HAPTIC_FRICTION_VELOCITY so the wheel does not buzz when it is idle.
float hapticFrictionTorque(){
  if(hapticActiveFriction <= 0.0f){
    return 0.0f;
  }

  float velocity = motor.shaft_velocity;

  if(fabsf(velocity) > HAPTIC_IDLE_VELOCITY_LIMIT){
    return 0.0f;
  }

  float scale = constrain(velocity / HAPTIC_FRICTION_VELOCITY, -1.0f, 1.0f);
  return -hapticActiveFriction * scale;
}

// Runs one iteration of the haptic model, drives the motor and reports how many
// positions the wheel moved since the last call, in the encoder frame.
int hapticStep(){
  float angleToCentre = encoder.getAngle() - hapticDetentCentre;
  bool unbounded = (hapticNumPositions <= 0);
  float snapDistance = hapticWidth * hapticActiveSnapPoint;
  int delta = 0;

  // Move the detent centre on once the wheel has travelled past the snap point.
  // This has to consume EVERY position that was crossed since the last call,
  // not just one: loop() runs far slower than the wheel can be flicked, so
  // stopping after a single position silently throws scroll ticks away.
  while(delta < HAPTIC_MAX_STEPS_PER_LOOP &&
        angleToCentre > snapDistance &&
        (unbounded || hapticPosition < hapticNumPositions - 1)){
    hapticDetentCentre += hapticWidth;
    angleToCentre -= hapticWidth;
    hapticPosition++;
    delta++;
  }

  while(delta > -HAPTIC_MAX_STEPS_PER_LOOP &&
        angleToCentre < -snapDistance &&
        (unbounded || hapticPosition > 0)){
    hapticDetentCentre -= hapticWidth;
    angleToCentre += hapticWidth;
    hapticPosition--;
    delta--;
  }

  // Sitting on a detent centre should not cost any current.
  float deadZoneLimit = fminf(hapticWidth * HAPTIC_DEAD_ZONE_PERCENT, HAPTIC_DEAD_ZONE_RAD);
  float deadZone = constrain(angleToCentre, -deadZoneLimit, deadZoneLimit);

  bool outOfBounds = !unbounded &&
                     ((angleToCentre > 0 && hapticPosition >= hapticNumPositions - 1) ||
                      (angleToCentre < 0 && hapticPosition <= 0));

  float strength = outOfBounds ? hapticActiveEndstop : hapticActiveDetent;
  float torque = 0.0f;

  if(strength <= 0.0f){
    // Nothing to hold on to here (free motion inside an endstop range, or a
    // gap between magnetic detents).
    hapticPID.reset();
  } else if(fabsf(motor.shaft_velocity) > HAPTIC_IDLE_VELOCITY_LIMIT){
    // Spinning too fast to trust the position, stay out of the way.
    hapticPID.reset();
  } else if(!outOfBounds && !hapticIsDetentPosition(hapticPosition)){
    // Sparse (magnetic) detents: smooth in between the listed positions.
    hapticPID.reset();
  } else {
    hapticPID.P = strength * 4.0f;
    hapticPID.limit = outOfBounds ? hapticVoltageLimit
                                  : fminf(HAPTIC_DETENT_VOLTAGE, hapticVoltageLimit);
    torque = hapticPID(-angleToCentre + deadZone);
  }

  // torque is in the encoder frame, hand it to the motor in the motor frame.
  motor.move(torque * (float)hapticSensorSign());

  return delta;
}

// How far the wheel is deflected from the centre of the current position, in
// radians (encoder frame).
float hapticDeflection(){
  return encoder.getAngle() - hapticDetentCentre;
}
