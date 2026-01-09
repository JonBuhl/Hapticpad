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
  if (strcmp(mode, "Clicky") == 0)   return WHEEL_CLICKY;
  if (strcmp(mode, "Twist") == 0)    return WHEEL_TWIST;
  if (strcmp(mode, "Momentum") == 0) return WHEEL_MOMENTUM;
  return 3;
}

uint8_t parseLEDMode(const char *mode) {
  if (strcmp(mode, "Breath") == 0)   return 1;
  if (strcmp(mode, "Bands") == 0)    return 2;
  if (strcmp(mode, "Halo") == 0) return 0;
  if (strcmp(mode, "Rainbow") == 0) return 3;
  if (strcmp(mode, "Solid") == 0) return 4;
  if (strcmp(mode, "Off") == 0) return 5;
  return 5;
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
  char buffer[16];
  file.find("<LED_Mode>");
  size_t len = file.readBytesUntil('<', buffer, sizeof(buffer) - 1);
  buffer[len] = '\0';
  ledMode = parseLEDMode(buffer);
  Serial.print("LED Mode = ");
  Serial.println(ledMode);

  file.find("<LED_Primary>");
  primaryColour[0] = (uint8_t)file.parseInt(); //Red
  primaryColour[1] = (uint8_t)file.parseInt(); //Green
  primaryColour[2] = (uint8_t)file.parseInt(); //Blue

  file.find("<LED_Secondary>");
  secondaryColour[0] = (uint8_t)file.parseInt(); //Red
  secondaryColour[1] = (uint8_t)file.parseInt(); //Green
  secondaryColour[2] = (uint8_t)file.parseInt(); //Blue

  file.find("<Clicky_P>");
  Clicky_P = file.parseFloat();
  file.find("<Clicky_I>");
  Clicky_I = file.parseFloat();
  file.find("<Twist_P>");
  Twist_P = file.parseFloat();
  file.find("<Twist_I>");
  Twist_I = file.parseFloat();
  file.find("<Momentum_P>");
  Momentum_P = file.parseFloat();
  file.find("<Momentum_I>");
  Momentum_I = file.parseFloat();

  file.close();
  return true;
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