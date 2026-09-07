// ---- Smart sleep ----
// After a stretch with nothing happening the OLED and the LED ring are turned
// off, and the next button press brings both back. The point is the pad sitting
// on a desk overnight: a static screen is the worst thing you can leave on an
// OLED, and the ring is a lamp nobody asked for.
//
// Only the two light sources sleep. The motor keeps running its haptic model
// and the wheel keeps scrolling the PC, so a sleeping pad is still a working
// pad, it is just dark. Turning the wheel counts as activity and holds sleep
// off, but it does not wake the pad once it is asleep - that is a button's job,
// so a wheel that gets knocked in passing does not light the room up.

// Milliseconds of the configured timeout. Kept as a function rather than a
// second global so there is only ever one number to keep in step.
unsigned long sleepTimeoutMs(){
  return (unsigned long)sleepTimeoutMinutes * 60000UL;
}

// Called from both cores: core 0 on every debounced button edge, core 1 when
// the wheel has moved. It only stores a timestamp, and a 32 bit aligned store
// is atomic on the RP2040, so the volatile is all the protection it needs.
void noteActivity(){
  lastActivityTime = millis();
}

bool anyButtonDown(){
  for(int i = 0; i < buttonCount; i++){
    if(lastButtonState[i]){
      return true;
    }
  }
  return false;
}

void enterSleep(){
  if(sleepActive){
    return;
  }
  sleepActive = true;

  // setPowerSave() sends the panel's own display off command rather than just
  // clearing the buffer, so the OLED really does stop driving pixels. It talks
  // to the display directly, which is why this is only ever called from core 1
  // and never from inside a firstPage()/nextPage() loop.
  u8g2.setPowerSave(1);

  FastLED.clear();
  FastLED.setBrightness(0);
  FastLED.show();
}

void wakeFromSleep(){
  if(!sleepActive){
    return;
  }
  sleepActive = false;
  sleepWakeRequest = false;
  // The press that woke the pad is swallowed, otherwise the first touch after a
  // break fires a macro or steps the profile. Held until every button is up
  // again, so a long press on the way in cannot leak through either.
  sleepWakeGuard = true;
  noteActivity();

  u8g2.setPowerSave(0);
  // Start the animation from the top rather than resuming mid fade, and put the
  // brightness back for the modes that do not set it themselves.
  resetLedAnimationState();
  FastLED.setBrightness(ledBrightness);
}

// Has the wheel moved since the last look? The angle is kept local on purpose:
// the shared encoderAngle globals belong to the core 0 wheel modes and writing
// them from here would race against them. encoder.getAngle() only reads the
// interrupt driven pulse count, which is why it is safe to call from core 1 at
// all - loop1() already does exactly this for Momentum mode.
bool wheelMoved(){
  static float lastSleepAngle = 0;
  static bool seeded = false;

  float angle = encoder.getAngle();

  if(!seeded){
    lastSleepAngle = angle;
    seeded = true;
    return false;
  }

  // About a degree. Wide enough that a single count of quadrature chatter never
  // reads as a turn, narrow enough that a deliberate nudge always does.
  if(fabs(angle - lastSleepAngle) < SLEEP_WHEEL_DEADBAND) {
    return false;
  }

  lastSleepAngle = angle;
  return true;
}

// Run once per core 1 frame, right after the buttons have been read.
void sleepTick(){
  if(sleepTimeoutMinutes == 0){
    // Sleep switched off in the config. If it was turned off while the pad was
    // already asleep, do not leave it dark.
    if(sleepActive){
      wakeFromSleep();
    }
    return;
  }

  if(wheelMoved()){
    noteActivity();
  }

  if(sleepActive){
    return; //only buttonRead() comes back from here
  }

  if(millis() - lastActivityTime >= sleepTimeoutMs()){
    enterSleep();
  }
}
