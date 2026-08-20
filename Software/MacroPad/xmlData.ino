uint16_t read16(File &f) {
  return f.read() | (f.read() << 8);
}

uint32_t read32(File &f) {
  return (uint32_t)f.read() |
         ((uint32_t)f.read() << 8) |
         ((uint32_t)f.read() << 16) |
         ((uint32_t)f.read() << 24);
}

uint8_t parseWheelMode(const char *mode) {
  for (uint8_t i = 0; i < WHEEL_MODE_COUNT; i++) {
    if (strcmp(mode, wheelModeNames[i]) == 0) return i;
  }
  return WHEEL_CLICKY;
}

// Reads a comma separated list of detent positions, e.g. "0,6,12,18".
uint8_t parseDetentPositions(const char *text, uint8_t *positions, uint8_t maxCount) {
  uint8_t count = 0;
  const char *cursor = text;

  while (count < maxCount && *cursor) {
    while (*cursor && (*cursor < '0' || *cursor > '9')) cursor++;
    if (!*cursor) break;

    long value = strtol(cursor, (char **)&cursor, 10);
    if (value >= 0 && value < 256) {
      positions[count] = (uint8_t)value;
      count++;
    }
  }

  return count;
}

uint8_t parseLEDMode(const char *mode) {
  int8_t idx = ledModeIndex(mode);
  return idx >= 0 ? (uint8_t)idx : 5;
}

uint16_t countProfiles(const char *filename) {
  File file = SD.open(filename);
  if (!file) return 0;

  uint16_t count = 0;

  file.find("<Profiles>"); //Skip this as it will find it as a profile otherwise

  while (file.find("<Profile")) {
    file.find("name=\"");
    file.readBytesUntil('"', profileNames[count], sizeof(profileNames[count]));
    profileName[sizeof(profileNames[count])-1] = '\0';
    count++;
  }

  //count--; //Subtract one because it finds <Profiles> as a profile.

  file.close();
  return count;
}

bool loadProfile(const char *filename, uint16_t index) {
  File file = SD.open(filename);
  //if (!file) return false;
  if (!file){
    sdDetected = false;
    return false;
  }

  file.find("<Profiles>"); //Skip this as it will find it as a profile otherwise
  
  for (uint16_t i = 0; i <= index; i++) { // Seek to requested profile. 
    if (!file.find("<Profile")) {
      file.close();
      return false;
    }
  }

  // --- Profile name ---
  memset(profileName, 0, sizeof(profileName));
  file.find("name=\"");
  file.readBytesUntil('"', profileName, sizeof(profileName));
  profileName[sizeof(profileName)-1] = '\0';

  // --- WheelMode ---
  char wheelModeBuf[16];
  file.find("<WheelMode>");
  size_t len = file.readBytesUntil('<', wheelModeBuf, sizeof(wheelModeBuf) - 1);
  wheelModeBuf[len] = '\0';
  wheelMode = parseWheelMode(wheelModeBuf);

  //Key to hold while scrolling
  if (file.find("<WheelKey>")) {
    wheelAction = (uint8_t)file.parseInt();
    file.find("</WheelKey>");
  }

  // Clear macros
  memset(macroAction, 0, sizeof(macroAction));
  memset(macroDelay, 0, sizeof(macroDelay));

  // --- Macro Buttons ---
  for (uint8_t btn = 0; btn < 6; btn++) {
    if (!file.find("<MacroButton")) break;

    for (uint8_t act = 0; act < 3; act++) {
      if (!file.find("<Action")) {
        macroAction[btn][act] = 0;
        macroDelay[btn][act] = 0;
        continue;
      }

      // Read delay
      file.find(">");
      macroDelay[btn][act] = (uint8_t)file.parseInt();

      // Read value
      file.find(",");
      macroAction[btn][act] = (uint8_t)file.parseInt();
      file.find("</Action>");
    }

    if (file.find("<Label>")) {
      size_t len = file.readBytesUntil('<', buttonLabel[btn], sizeof(buttonLabel[btn]) - 1);
      buttonLabel[btn][len] = '\0';
    }
    
    
  }

  file.close();
  return true;
}

bool loadSettings(const char *filename){
  File file = SD.open(filename);

  if (!file){
    sdDetected = false;
    return false;
  }

  if (!file.find("<Settings>")){
    return false;
  }

  // --- LED Mode ---
  char buffer[64];
  file.find("<LED_Mode>");
  size_t len = file.readBytesUntil('<', buffer, sizeof(buffer) - 1);
  buffer[len] = '\0';
  ledMode = parseLEDMode(buffer);
  Serial.print("LED Mode = ");
  Serial.println(ledMode);

  // Reset LED menu to defaults in case the XML does not override it
  ledModeMenuCount = LED_MODE_COUNT;
  for(uint8_t i = 0; i < LED_MODE_COUNT; i++){
    ledModeMenu[i] = i;
  }

  file.find("<LED_Primary>");
  primaryColour[0] = (uint8_t)file.parseInt(); //Red
  primaryColour[1] = (uint8_t)file.parseInt(); //Green
  primaryColour[2] = (uint8_t)file.parseInt(); //Blue

  file.find("<LED_Secondary>");
  secondaryColour[0] = (uint8_t)file.parseInt(); //Red
  secondaryColour[1] = (uint8_t)file.parseInt(); //Green
  secondaryColour[2] = (uint8_t)file.parseInt(); //Blue

  if(file.find("<LED_Menu>")){
    ledModeMenuCount = 0;

    while(ledModeMenuCount < LED_MODE_COUNT && file.find("<Mode>")){
      len = file.readBytesUntil('<', buffer, sizeof(buffer) - 1);
      buffer[len] = '\0';
      int8_t idx = ledModeIndex(buffer);

      if(idx >= 0){
        bool alreadyAdded = false;
        for(uint8_t j = 0; j < ledModeMenuCount; j++){
          if(ledModeMenu[j] == (uint8_t)idx){
            alreadyAdded = true;
            break;
          }
        }

        if(!alreadyAdded){
          ledModeMenu[ledModeMenuCount] = (uint8_t)idx;
          ledModeMenuCount++;
        }
      }
    }

    if(ledModeMenuCount == 0){
      for(uint8_t i = 0; i < LED_MODE_COUNT; i++){
        ledModeMenu[i] = i;
      }
      ledModeMenuCount = LED_MODE_COUNT;
    }
  }

  // Legacy PID tuning. Clicky and Twist now use the haptic model below, these
  // are still read so older config files keep loading. Momentum still uses them.
  if(file.find("<Clicky_P>"))   Clicky_P = file.parseFloat();
  if(file.find("<Clicky_I>"))   Clicky_I = file.parseFloat();
  if(file.find("<Twist_P>"))    Twist_P = file.parseFloat();
  if(file.find("<Twist_I>"))    Twist_I = file.parseFloat();
  if(file.find("<Momentum_P>")) Momentum_P = file.parseFloat();
  if(file.find("<Momentum_I>")) Momentum_I = file.parseFloat();

  // --- Haptic settings ---
  // These are all optional. The tags are searched for in file order, so a
  // config without them simply keeps the defaults from haptics.h.
  if(file.find("<Haptic_VoltageLimit>"))    hapticVoltageLimit = file.parseFloat();
  if(file.find("<Haptic_DetentStrength>")){
    hapticDetentStrength = file.parseFloat();
    clickyStrength = hapticDetentStrength; //per mode tag below can still override
  }
  if(file.find("<Haptic_EndstopStrength>")) hapticEndstopStrength = file.parseFloat();
  if(file.find("<Haptic_SnapPoint>"))       hapticSnapPoint = file.parseFloat();
  if(file.find("<Haptic_Range>"))           hapticRange = (uint8_t)file.parseInt();

  if(file.find("<Haptic_MagneticPositions>")){
    len = file.readBytesUntil('<', buffer, sizeof(buffer) - 1);
    buffer[len] = '\0';
    uint8_t count = parseDetentPositions(buffer, hapticDetentPositions, HAPTIC_MAX_DETENT_POSITIONS);
    if(count > 0){
      hapticDetentPositionCount = count;
    }
  }

  if(file.find("<Clicky_Detents>"))    clickyDetents = (uint16_t)file.parseInt();
  if(file.find("<Clicky_Strength>"))   clickyStrength = file.parseFloat();
  if(file.find("<Twist_Strength>"))    twistStrength = file.parseFloat();
  if(file.find("<Twist_Range>"))       twistRangeDeg = file.parseFloat();
  if(file.find("<Friction_Strength>")) frictionStrength = file.parseFloat();
  if(file.find("<Snap_Strength>"))     snapStrength = file.parseFloat();
  if(file.find("<Snap_Detents>"))      snapDetents = (uint16_t)file.parseInt();
  if(file.find("<Snap_Point>"))        snapPoint = file.parseFloat();
  if(file.find("<Magnetic_Strength>")) magneticStrength = file.parseFloat();
  if(file.find("<Magnetic_Detents>"))  magneticDetents = (uint16_t)file.parseInt();

  validateHapticSettings();

  // core 0 only rebuilds the haptic model when the wheel mode changes, so poke
  // it here or freshly loaded detent counts and strengths never take effect.
  wheelModeChanged = true;

  rgbMenuSelection = findLedMenuIndex(ledMode);

  file.close();
  return true;
}

// Keeps a hand edited config from asking the motor for something silly.
void validateHapticSettings(){
  hapticVoltageLimit   = constrain(hapticVoltageLimit, 0.5f, 5.0f);
  hapticDetentStrength = constrain(hapticDetentStrength, 0.0f, 5.0f);
  hapticEndstopStrength = constrain(hapticEndstopStrength, 0.0f, 5.0f);
  hapticSnapPoint      = constrain(hapticSnapPoint, HAPTIC_MIN_SNAP_POINT, 2.0f);
  snapPoint            = constrain(snapPoint, HAPTIC_MIN_SNAP_POINT, 2.0f);

  clickyStrength   = constrain(clickyStrength, 0.0f, 5.0f);
  twistStrength    = constrain(twistStrength, 0.0f, 5.0f);
  frictionStrength = constrain(frictionStrength, 0.0f, 5.0f);
  snapStrength     = constrain(snapStrength, 0.0f, 5.0f);
  magneticStrength = constrain(magneticStrength, 0.0f, 5.0f);

  twistRangeDeg = constrain(twistRangeDeg, 5.0f, 180.0f);

  clickyDetents   = constrain(clickyDetents, 4, 400);
  snapDetents     = constrain(snapDetents, 4, 400);
  magneticDetents = constrain(magneticDetents, 4, 255);

  if(hapticRange < 2) hapticRange = 2;

  // Drop any magnetic detent positions that fall outside the magnetic range.
  uint8_t kept = 0;
  for(uint8_t i = 0; i < hapticDetentPositionCount; i++){
    if(hapticDetentPositions[i] < magneticDetents){
      hapticDetentPositions[kept] = hapticDetentPositions[i];
      kept++;
    }
  }
  hapticDetentPositionCount = kept;
}

bool loadBMP16x16(const char *filename, uint8_t icon[16][2]) {
  File bmp = SD.open(filename);
  if (!bmp) return false;

  if (read16(bmp) != 0x4D42) { // "BM"
    bmp.close();
    return false;
  }

  bmp.seek(10);
  uint32_t pixelOffset = read32(bmp);

  bmp.seek(18);
  int32_t width  = read32(bmp);
  int32_t height = read32(bmp);

  bmp.seek(28);
  uint16_t depth = read16(bmp);

  if (width != 16 || abs(height) != 16 || depth != 1) {
    bmp.close();
    return false;
  }

  bool bottomUp = height > 0;

  for (int row = 0; row < 16; row++) {
    int srcRow = bottomUp ? (15 - row) : row;
    bmp.seek(pixelOffset + srcRow * 4);

    uint8_t rowData[4];
    bmp.read(rowData, 4);

    // Copy only the first 2 bytes (16 pixels)
    icon[row][0] = rowData[0];
    icon[row][1] = rowData[1];
  }

  bmp.close();
  return true;
}

bool loadBMP15x15(const char *filename, uint8_t icon[15][2]) {
  File bmp = SD.open(filename);
  if (!bmp) return false;

  // Check BMP signature
  if (read16(bmp) != 0x4D42) { // "BM"
    bmp.close();
    return false;
  }

  // Pixel data offset
  bmp.seek(10);
  uint32_t pixelOffset = read32(bmp);

  // Width / height
  bmp.seek(18);
  int32_t width  = read32(bmp);
  int32_t height = read32(bmp);

  // Bit depth
  bmp.seek(28);
  uint16_t depth = read16(bmp);

  if (width != 15 || abs(height) != 15 || depth != 1) {
    bmp.close();
    return false;
  }

  bool bottomUp = height > 0;

  // Each BMP row is padded to 4 bytes
  const uint8_t bmpRowSize = 4;

  for (int row = 0; row < 15; row++) {
    int srcRow = bottomUp ? (14 - row) : row;
    bmp.seek(pixelOffset + srcRow * bmpRowSize);

    uint8_t rowData[4];
    bmp.read(rowData, bmpRowSize);

    // Copy only the first 2 bytes (15 pixels)
    icon[row][0] = rowData[0];
    icon[row][1] = rowData[1];
  }

  bmp.close();
  return true;
}

void drawIcon15x15(int x, int y, const uint8_t icon[15][2]) {
  u8g2.drawBitmap(x, y, 2, 15, (const uint8_t *)icon);
}

void drawIcon16x16(int x, int y, const uint8_t icon[16][2]) {
  u8g2.drawBitmap(x, y, 2, 16, (const uint8_t *)icon);
}