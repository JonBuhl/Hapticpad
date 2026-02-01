

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
  u8g2.drawStr( 0, 36, profileName);//.c_str());

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

void drawProfileMenu(){
  int y = 34 - activeProfile * 9;
  u8g2.setFont(u8g2_font_5x7_tf);
  for(int i = 0; i < totalProfiles; i++){
    if(i == activeProfile){
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
  const char *options[2] = {"Profile Menu", "RGB Modes"};
  int y = 26;

  for(uint8_t i = 0; i < 2; i++){
    if(i == selection){
      u8g2.drawBox(0, y - 8, u8g2.getStrWidth(options[i]) + 2, 9);
      u8g2.setDrawColor(0);
      u8g2.drawStr(1, y, options[i]);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(1, y, options[i]);
    }
    y += 12;
  }
}

void drawRGBMenu(uint8_t selection){
  u8g2.setFont(u8g2_font_5x7_tf);
  int y = 18;

  for(uint8_t i = 0; i < ledModeMenuCount; i++){
    const char *modeName = ledModeToString(ledModeMenu[i]);

    if(i == selection){
      u8g2.drawBox(0, y - 8, u8g2.getStrWidth(modeName) + 2, 9);
      u8g2.setDrawColor(0);
      u8g2.drawStr(1, y, modeName);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(1, y, modeName);
    }
    y += 10;
  }
}