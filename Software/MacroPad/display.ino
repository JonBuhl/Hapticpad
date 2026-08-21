

void loadButtonIcons(){
  char filePath[maxProfileNameLength + 6];

  snprintf(filePath, sizeof(filePath), "%s/1.bmp", profileName);
  loadBMP15x15(filePath, icon1);

  snprintf(filePath, sizeof(filePath), "%s/2.bmp", profileName);
  loadBMP15x15(filePath, icon2);

  snprintf(filePath, sizeof(filePath), "%s/3.bmp", profileName);
  loadBMP15x15(filePath, icon3);

  snprintf(filePath, sizeof(filePath), "%s/4.bmp", profileName);
  loadBMP15x15(filePath, icon4);

  snprintf(filePath, sizeof(filePath), "%s/5.bmp", profileName);
  loadBMP15x15(filePath, icon5);

  snprintf(filePath, sizeof(filePath), "%s/6.bmp", profileName);
  loadBMP15x15(filePath, icon6);
}

void drawGrid() { //Draws button grid
  u8g2.drawLine(0, 26, 128, 26); //X Start, Y Start, X Finish, Y Finish
  u8g2.drawLine(0, 39, 128, 39); //X Start, Y Start, X Finish, Y Finish
  u8g2.drawLine(42, 0, 42, 26);
  u8g2.drawLine(86, 0, 86, 26);
  u8g2.drawLine(42, 39, 42, 64);
  u8g2.drawLine(86, 39, 86, 64);
}

void drawActiveProfile(){ //Draws all icons and labels from config.xml for active profile
  u8g2.setFont(u8g2_font_5x7_tf);

  // While a wheel domain is running, its button's label takes the place of the
  // profile name so it is obvious what the wheel is doing.
  int8_t domain = activeWheelDomain;
  if(domain >= 0 && domain < 6){
    char domainLine[40];
    snprintf(domainLine, sizeof(domainLine), "> %s", buttonLabel[domain]);
    u8g2.drawStr( 0, 36, domainLine);
  } else {
    u8g2.drawStr( 0, 36, profileName);//.c_str());
  }

  //Draw Icons
  drawIcon15x15(13, 0, icon1);
  drawIcon15x15(58, 0, icon2);
  drawIcon15x15(102, 0, icon3);
  drawIcon15x15(13, 49, icon4);
  drawIcon15x15(58, 49, icon5);
  drawIcon15x15(102, 49, icon6);

  //Draw Labels
  u8g2.setFont(u8g2_font_u8glib_4_tf);

  int x = 20;
  int y = 22;

  for(int i = 0; i < 6; i++){
    if(i > 2){
      y = 47;
    }
    if(x > 108){
      x = 20;
    }
    u8g2.drawStr(x - (u8g2.getStrWidth(buttonLabel[i]) / 2),y,buttonLabel[i]); 
    x += 44;
  }
}

void drawProfileMenu(uint8_t selection){
  int y = 34 - selection * 9;
  u8g2.setFont(u8g2_font_5x7_tf);
  for(int i = 0; i < totalProfiles; i++){
    if(i == selection){
      u8g2.drawBox(0, y - 8, u8g2.getStrWidth(profileNames[i]) + 2, 9);
      u8g2.setDrawColor(0);
      u8g2.drawStr( 1, y, profileNames[i]);
      u8g2.setDrawColor(1);
    } else if(y > 0){
      u8g2.drawStr( 1, y, profileNames[i]);
    }
    y += 9;
  }
}

void drawRootMenu(uint8_t selection){
  u8g2.setFont(u8g2_font_5x7_tf);
  char storageLabel[24];
  snprintf(storageLabel, sizeof(storageLabel), "USB Storage %s", usbStorageMode ? "On" : "Off");
  const char *options[5] = {"Profile Menu", "RGB Modes", "Colors", storageLabel, "Haptic Test"};
  int y = 26 - selection * 12; // keep selected entry anchored while list scrolls

  for(uint8_t i = 0; i < 5; i++){
    const char *label = options[i];
    if(i == selection){
      u8g2.drawBox(0, y - 8, u8g2.getStrWidth(label) + 2, 9);
      u8g2.setDrawColor(0);
      u8g2.drawStr(1, y, label);
      u8g2.setDrawColor(1);
    } else if(y > 0){
      u8g2.drawStr(1, y, label);
    }
    y += 12;
  }
}

void drawRGBMenu(uint8_t selection){
  u8g2.setFont(u8g2_font_5x7_tf);
  int y = 18 - selection * 10; // scroll list like profile menu

  for(uint8_t i = 0; i < ledModeMenuCount; i++){
    const char *modeName = ledModeToString(ledModeMenu[i]);

    if(i == selection){
      u8g2.drawBox(0, y - 8, u8g2.getStrWidth(modeName) + 2, 9);
      u8g2.setDrawColor(0);
      u8g2.drawStr(1, y, modeName);
      u8g2.setDrawColor(1);
    } else if(y > 0){
      u8g2.drawStr(1, y, modeName);
    }
    y += 10;
  }
}

void drawColorMenu(uint8_t selection){
  u8g2.setFont(u8g2_font_5x7_tf);
  const char *labels[2] = {"Primary", "Secondary"};
  const uint8_t *colors[2] = {primaryColour, secondaryColour};
  int y = 18 - selection * 10; // scroll list like profile menu

  for(uint8_t i = 0; i < 2; i++){
    char line[48];
    char channelMarker = ' ';
    if(menuPage == MENU_COLOR && colorEditActive && i == selection){
      channelMarker = (colorEditChannel == 0) ? 'R' : (colorEditChannel == 1 ? 'G' : 'B');
    }

    snprintf(line, sizeof(line), "%s R%u G%u B%u %c", labels[i], colors[i][0], colors[i][1], colors[i][2], channelMarker);

    if(i == selection){
      u8g2.drawBox(0, y - 8, u8g2.getStrWidth(line) + 2, 9);
      u8g2.setDrawColor(0);
      u8g2.drawStr(1, y, line);
      u8g2.setDrawColor(1);
    } else if(y > 0){
      u8g2.drawStr(1, y, line);
    }
    y += 10;
  }
}

void drawHapticMenu(uint8_t selection){
  u8g2.setFont(u8g2_font_5x7_tf);

  if(hapticTestActive){
    char line[32];
    snprintf(line, sizeof(line), "Testing: %s", wheelModeNames[hapticTestMode]);
    u8g2.drawStr(1, 12, line);
    u8g2.drawStr(1, 30, "Turn the wheel to");
    u8g2.drawStr(1, 40, "feel this mode.");
    u8g2.drawStr(1, 58, "BACK to return");
    return;
  }

  int y = 18 - selection * 10; // keep selected entry anchored while list scrolls

  for(uint8_t i = 0; i < WHEEL_MODE_COUNT; i++){
    const char *modeName = wheelModeNames[i];

    if(i == selection){
      u8g2.drawBox(0, y - 8, u8g2.getStrWidth(modeName) + 2, 9);
      u8g2.setDrawColor(0);
      u8g2.drawStr(1, y, modeName);
      u8g2.setDrawColor(1);
    } else if(y > 0){
      u8g2.drawStr(1, y, modeName);
    }
    y += 10;
  }
}

void drawUsbStorageScreen(){
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(10, 18, "USB Storage Mode");
  u8g2.drawStr(10, 32, "SD shared over USB");
  u8g2.drawStr(10, 46, "Eject on PC, then");
  u8g2.drawStr(10, 60, "hold both menu btns");
}