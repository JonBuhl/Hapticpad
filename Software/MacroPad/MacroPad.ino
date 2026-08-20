#include <Arduino.h>
#include <U8g2lib.h>
#include <SimpleFOC.h>
#include <Adafruit_TinyUSB.h>
#include <SdFat.h>
#include <SPI.h>
#include <SD.h>
#include <FastLED.h>
#include "menu.h"
#include "haptics.h"

const unsigned char SD_Card [] PROGMEM = {
	0xff, 0x01, 0x55, 0x01, 0x55, 0x01, 0xff, 0x01, 0x01, 0x03, 0x2d, 0x02, 0x45, 0x03, 0x49, 0x01, 
	0x2d, 0x03, 0x01, 0x02, 0xfd, 0x02, 0x01, 0x02, 0xff, 0x03
};

#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

// Wheel mode ids and the haptic model live in haptics.h

#define KEY_LEFT_CTRL 17

#define NUM_LEDS 20
#define DATA_PIN 15

CRGB leds[NUM_LEDS];

//User Configurable LED Settings
byte primaryColour[3] = {0,50,50}; //Red, Green, Blue
byte secondaryColour[3] = {0,0,0};
int ledBrightness = 100;
int ledSpeed = 50;

//LED Sequence Variables
float redScale;
float greenScale;
float blueScale;
int sequenceStep = 0;
int ledMode = 3;
unsigned long ledTimer;

const uint8_t LED_MODE_COUNT = 6;
const char *ledModeNames[LED_MODE_COUNT] = {"Halo", "Breath", "Bands", "Rainbow", "Solid", "Off"};
uint8_t ledModeMenu[LED_MODE_COUNT] = {0, 1, 2, 3, 4, 5};
uint8_t ledModeMenuCount = LED_MODE_COUNT;
uint8_t rgbMenuSelection = 3;
uint8_t colorMenuSelection = 0;
uint8_t colorEditSelection = 0;
uint8_t colorEditChannel = 0; //0=R,1=G,2=B
bool colorEditActive = false;
uint8_t *colorEditPtr = nullptr;
bool colorDirty = false;

int8_t ledModeIndex(const char *mode) {
  if (strcmp(mode, "Halo") == 0) return 0;
  if (strcmp(mode, "Breath") == 0) return 1;
  if (strcmp(mode, "Bands") == 0) return 2;
  if (strcmp(mode, "Rainbow") == 0) return 3;
  if (strcmp(mode, "Solid") == 0) return 4;
  if (strcmp(mode, "Off") == 0) return 5;
  return -1;
}

const char *ledModeToString(uint8_t mode) {
  if (mode < LED_MODE_COUNT) {
    return ledModeNames[mode];
  }
  return "";
}

uint8_t findLedMenuIndex(uint8_t mode){
  for(uint8_t i = 0; i < ledModeMenuCount; i++){
    if(ledModeMenu[i] == mode){
      return i;
    }
  }
  return 0;
}

//LED Halo Variables
int haloCount = 0; //Which is the brightest LED in the sequence

//LED Breath Variables
bool breathIncrease = true;

//LED Bands
bool evenNumber = false;
int loopCounter;

bool debug = false;

//SD Pins
const int _MISO = 4;
const int _MOSI = 3;
const int _CS = 5;
const int _SCK = 2;

bool sdDetected = false;

Adafruit_USBD_MSC usb_msc;
SdFat usbStorageFat;
SdSpiConfig usbStorageConfig(_CS, DEDICATED_SPI, SD_SCK_MHZ(12), &SPI);
uint32_t usbStorageBlockCount = 0;
bool usbStorageMode = false;
unsigned long usbStorageButtonTimer = 0;

U8G2_SSD1309_128X64_NONAME0_1_4W_SW_SPI u8g2(U8G2_R0, 28, 22, 6, 7, 8);

BLDCMotor motor = BLDCMotor(7);
BLDCDriver6PWM driver = BLDCDriver6PWM(20, 21, 18,19, 16, 17);

//float target_velocity = 6;
Commander command = Commander(Serial);

Encoder encoder = Encoder(27, 26, 1024);
void doA(){encoder.handleA();}
void doB(){encoder.handleB();}

// angle set point variable
float target_angle = 0; //Radians. 1 Rad = 57.2958 Deg
float new_target_angle;

float target_velocity = 0;
float last_velocity = 0;
float testFactor;

float angle_step = radians(360/40);
float encoderAngle;
float lastEncoderAngle;
bool keyPressed = false;
bool wheelKeyPressed = false;

//PID Values

float Clicky_P = 0.5;    //legacy, Clicky now uses the haptic model
float Clicky_I = 0;      //legacy
float Twist_P = 0.65;    //legacy, Twist now uses the haptic model
float Twist_I = 0.2;     //legacy
float Momentum_P = 0.3;
float Momentum_I = 0;
float motorP = 0.5;
float motorI = 0;
float motorD = 0;

long timer;
long aceltimer;
long mouseTimer;
float interval;
unsigned long wheelKeyTimer;
unsigned long keyTimer;

long debounceTimer;
bool decelDetected = false;
bool decelerating = false;

// wheelMode and wheelModeChanged are written on core 1 (profile/settings load,
// opening and closing the menu) and read on core 0 in loop(). Without volatile
// the compiler is free to hoist the load out of the main loop, which leaves core
// 0 running whichever mode it saw first and never noticing the menu.
volatile int wheelMode = 0;
int lastWheelMode = -1; //core 0 only
volatile bool wheelModeChanged = true;

bool FOC_Ready = false;

#define buttonCount 8 //Number of buttons connected
byte buttonPins[buttonCount] = {9, 1, 0, 12, 11, 10, 13, 14}; //1,2,3,4,5,6
int lastButtonState[buttonCount];

//Eeprom Memory
int activeProfile = 0;
//int activePage = 1;

uint8_t icon1[15][2]; //Used to store Icon 1 Data
uint8_t icon2[15][2]; //Used to store Icon 2 Data
uint8_t icon3[15][2]; //Used to store Icon 3 Data
uint8_t icon4[15][2]; //Used to store Icon 4 Data
uint8_t icon5[15][2]; //Used to store Icon 5 Data
uint8_t icon6[15][2]; //Used to store Icon 6 Data

// ---- Profile data ----
#define maxProfileNameLength 32
#define maxProfiles 256
char profileName[maxProfileNameLength];
char profileNames[maxProfileNameLength][maxProfiles];
char buttonLabel[6][32];
unsigned long profileChangeTimer;
bool profilePlusStarted = false;
bool profileMinusStarted = false;
// Set on core 1, read on core 0 to decide whether the wheel drives the menu or
// the PC. Must be volatile, see wheelMode above.
volatile bool profileSelectMenu = false;

enum MenuPage : uint8_t {
  MENU_NONE = 0,
  MENU_ROOT,
  MENU_PROFILE,
  MENU_RGB,
  MENU_COLOR,
  MENU_HAPTIC
};

volatile MenuPage menuPage = MENU_NONE;
uint8_t menuRootSelection = 0; //0 = Profile menu, 1 = RGB menu
uint8_t profileMenuSelection = 0;
bool menuReentryGuard = false; //prevents immediate re-entry after exit until buttons released

// Written on core 1 by enterMenu()/exitMenu(), read on core 0 by menuScroll().
const MenuDefinition * volatile currentMenu = nullptr;

void enterMenu(MenuPage page);
void exitMenu();
void applyProfileSelection(uint8_t selection);
void applyLedSelection(uint8_t selection);
bool enterUsbStorageMode();
void exitUsbStorageMode();
void toggleUsbStorageMode();
void setupUsbStorage();
void drawUsbStorageScreen();

unsigned long buttonPressStart[buttonCount];
bool menuButtonHandled[buttonCount];

uint8_t wheelAction;
uint8_t macroAction[6][3];   // decimal values
uint16_t macroDelay[6][3];
float targetAngle;

// ---- Global ----
uint16_t totalProfiles = 0;

File root;

enum {
  REPORT_ID_KEYBOARD = 1,
  REPORT_ID_CONSUMER,
};

uint8_t const desc_hid_report[] =
{
  TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD)),
  TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(REPORT_ID_CONSUMER))
};

uint8_t const desc_mouse_report[] =
{
  TUD_HID_REPORT_DESC_MOUSE()
};

Adafruit_USBD_HID usb_keyboard(desc_hid_report, sizeof(desc_hid_report), HID_ITF_PROTOCOL_NONE, 2, false);
Adafruit_USBD_HID usb_mouse(desc_mouse_report, sizeof(desc_mouse_report), HID_ITF_PROTOCOL_MOUSE, 2, false);

int32_t usbStorageReadCb(uint32_t lba, void* buffer, uint32_t bufsize) {
  if(!usbStorageMode || usbStorageBlockCount == 0 || !usbStorageFat.card()){
    return -1;
  }

  uint32_t blockCount = bufsize / 512;
  return usbStorageFat.card()->readSectors(lba, static_cast<uint8_t*>(buffer), blockCount) ? (int32_t)bufsize : -1;
}

int32_t usbStorageWriteCb(uint32_t lba, uint8_t* buffer, uint32_t bufsize) {
  if(!usbStorageMode || usbStorageBlockCount == 0 || !usbStorageFat.card()){
    return -1;
  }

  uint32_t blockCount = bufsize / 512;
  return usbStorageFat.card()->writeSectors(lba, buffer, blockCount) ? (int32_t)bufsize : -1;
}

void usbStorageFlushCb(void) {
  if(!usbStorageMode || !usbStorageFat.card()){
    return;
  }

  usbStorageFat.card()->syncDevice();
}

void setup() { //Core 0
  driver.voltage_power_supply = 5;
  driver.voltage_limit = 5;
  driver.init();

  encoder.init();
  encoder.enableInterrupts(doA, doB);
  motor.linkSensor(&encoder);

  driver.voltage_power_supply = 5;
  driver.init();

  motor.linkDriver(&driver);
  motor.voltage_sensor_align = 3;

  motor.PID_velocity.D = 0;

  motor.voltage_limit = MOTOR_BASE_VOLTAGE_LIMIT;

  motor.PID_velocity.output_ramp = 1000;
  motor.LPF_velocity.Tf = 0.025f;//0.01f;
  motor.P_angle.P = 20;
  motor.velocity_limit = 4;

  motor.init();
  motor.initFOC();

  Serial.begin(115200);
  if(debug){
    while(!Serial){}
  }
  Serial.println("Motor ready!");
  FOC_Ready = true;
}

void setup1(){ //core 1
  usb_keyboard.begin();
  usb_mouse.begin();

  FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);

  SPI.setRX(_MISO);
  SPI.setTX(_MOSI);
  SPI.setSCK(_SCK);

  if(debug){
    while(!Serial){}
  }

  initialiseSD();
  setupUsbStorage();

  loadSettings("/config.xml");
  readLastState();

  applyLedMode(ledMode);

  u8g2.begin();

  for(int i = 0; i < buttonCount; i++){
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  while(!FOC_Ready){delay(10);}
}

void buttonRead(){ //Read button inputs and set state arrays.
  for (int i = 0; i < buttonCount; i++){
    int input = !digitalRead(buttonPins[i]);
    bool prevState = lastButtonState[i];
    if (input && !prevState){
      buttonPressStart[i] = millis();
    }
    lastButtonState[i] = input;
    if(!input){
      menuButtonHandled[i] = false;
    }
  }

  if(usbStorageMode){
    if(lastButtonState[6] && lastButtonState[7]){
      if(usbStorageButtonTimer == 0){
        usbStorageButtonTimer = millis();
      } else if(usbStorageButtonTimer + 2000 < millis()){
        exitUsbStorageMode();
      }
    } else {
      usbStorageButtonTimer = 0;
    }
    return;
  }

  if(profileSelectMenu){
    handleMenuButtons();
  }

  // avoid immediate re-entry after closing menu until both buttons are released
  if(menuReentryGuard){
    if(!lastButtonState[6] && !lastButtonState[7]){
      menuReentryGuard = false;
    }
  }

  if(sdDetected && !profileSelectMenu){
    for(int i = 0; i < 6; i++){
      if(lastButtonState[i]){
        macroOutput(i);
        keyPressed = true;
        keyTimer = millis();
      }
    }
    if(!menuReentryGuard && lastButtonState[6]){ //Next Profile
      if(!profilePlusStarted){
        profilePlusStarted = true;
        profileChangeTimer = millis();
      }
      if(profilePlusStarted && profileChangeTimer + 800 < millis()){
        openMenu();
      }
    } else {
      if(profilePlusStarted){
        if(activeProfile < totalProfiles - 1){
          activeProfile++;
          loadProfile("/config.xml", activeProfile);
          storeLastProfile();
          Serial.print("Stored last profile = ");
          Serial.println(activeProfile);
          loadButtonIcons();
          delay(100);
        }
      }
    }
    if(!menuReentryGuard && lastButtonState[7]){ //Previous Profile
      if(!profileMinusStarted){
        profileMinusStarted = true;
        profileChangeTimer = millis();
      } else if(profileMinusStarted && profileChangeTimer + 800 < millis()){
        openMenu();
      }
    } else {
      if(profileMinusStarted){
        if(activeProfile > 0 ){
          activeProfile--;
          loadProfile("/config.xml", activeProfile);
          storeLastProfile();
          Serial.print("Stored last profile = ");
          Serial.println(activeProfile);
          loadButtonIcons();
          delay(100);
        }
      }
    }
    if(!lastButtonState[7] && !lastButtonState[6]){
      profileMinusStarted = false;
      profilePlusStarted = false;
    }
  } else if(!sdDetected && !profileSelectMenu){
    if(lastButtonState[4]){
      initialiseSD();
      delay(100);
    }

  }

  if(keyPressed && keyTimer + 50 < millis()){
    usb_keyboard.keyboardRelease(REPORT_ID_KEYBOARD);
    keyPressed = false;
  }
}

void initialiseSD(){
  SD.end(true); //Attempt to close any previous SD sessions

  if (!SD.begin(_CS)) {
    Serial.println("initialization failed!");
    return;
  } else {
    sdDetected = true;
    Serial.println("SD Initialised!");
  }

  totalProfiles = countProfiles("/config.xml");
  if(debug){
    Serial.print("Profiles found: ");
    Serial.println(totalProfiles);
  }

  activeProfile = readLastProfile();
  Serial.print("Active Profile = ");
  Serial.println(activeProfile);

  loadProfile("/config.xml", activeProfile);
  loadButtonIcons();
}

void setupUsbStorage(){
  usb_msc.setID("CNCDan", "HapticPad SD", "1.0");
  usb_msc.setReadWriteCallback(usbStorageReadCb, usbStorageWriteCb, usbStorageFlushCb);
  // Set a safe default; real capacity is updated when entering storage mode
  usb_msc.setCapacity(usbStorageBlockCount == 0 ? 8 : usbStorageBlockCount, 512);
  usb_msc.setUnitReady(false);
  usb_msc.begin();
}

bool enterUsbStorageMode(){
  if(usbStorageMode){
    return true;
  }

  storeLastState();
  SD.end(true);
  delay(5);

  if(!usbStorageFat.begin(usbStorageConfig)){
    initialiseSD();
    return false;
  }

  if(usbStorageBlockCount == 0 && usbStorageFat.card()){
    usbStorageBlockCount = usbStorageFat.card()->sectorCount();
  }

  if(usbStorageBlockCount == 0){
    if(usbStorageFat.card()){
      usbStorageFat.card()->syncDevice();
    }
    initialiseSD();
    return false;
  }

  usb_msc.setCapacity(usbStorageBlockCount, 512);
  usb_msc.setUnitReady(true);
  TinyUSBDevice.detach();
  delay(30);
  TinyUSBDevice.attach();
  usbStorageMode = true;
  sdDetected = false;
  hapticTestActive = false;
  profileSelectMenu = false;
  menuPage = MENU_NONE;
  currentMenu = nullptr;
  return true;
}

void exitUsbStorageMode(){
  if(!usbStorageMode){
    return;
  }

  usb_msc.setUnitReady(false);
  usbStorageMode = false;
  usbStorageButtonTimer = 0;
  if(usbStorageFat.card()){
    usbStorageFat.card()->syncDevice();
  }

  initialiseSD();
  totalProfiles = countProfiles("/config.xml");
  if(activeProfile >= (int)totalProfiles){
    activeProfile = totalProfiles > 0 ? (int)totalProfiles - 1 : 0;
  }
  loadSettings("/config.xml");
  loadProfile("/config.xml", activeProfile);
  loadButtonIcons();
}

void loop() {
  motor.loopFOC();

  if(usbStorageMode){
    lastWheelMode = -1; //re-initialise the wheel when storage mode ends
    return;
  }

  // Work out which behaviour the wheel should be running right now. The haptic
  // test page overrides the profile, and an open menu overrides both.
  int effectiveMode;
  if(hapticTestActive){
    effectiveMode = hapticTestMode;
  } else if(profileSelectMenu){
    effectiveMode = WHEEL_MODE_MENU;
  } else {
    effectiveMode = wheelMode;
  }

  if(lastWheelMode != effectiveMode){
    wheelModeChanged = true;
  }
  lastWheelMode = effectiveMode;

  switch (effectiveMode)
  {
  case WHEEL_MODE_MENU:
    menuWheel(); // menu navigation only, no PC scroll
    break;
  case WHEEL_CLICKY:
    notchyWheel();
    break;
  case WHEEL_TWIST:
    twistScroll();
    break;
  case WHEEL_MOMENTUM:
    freeSpinning();
    break;
  case WHEEL_FREE_SCROLL:
    freeScroll();
    break;
  case WHEEL_ENDSTOP:
    endstopWheel();
    break;
  case WHEEL_FRICTION:
    frictionWheel();
    break;
  case WHEEL_SNAP:
    snapWheel();
    break;
  case WHEEL_MAGNETIC:
    magneticWheel();
    break;

  default:
    break;
  }
}

void resetLedAnimationState(){
  sequenceStep = 0;
  haloCount = 0;
  breathIncrease = true;
  evenNumber = false;
  loopCounter = 0;
  ledTimer = millis();
}

void applyLedMode(uint8_t mode){
  if(mode >= LED_MODE_COUNT){
    mode = LED_MODE_COUNT - 1;
  }
  ledMode = mode;
  calculateColourMultiplier();
  rgbMenuSelection = findLedMenuIndex(ledMode);
  resetLedAnimationState();
}

uint8_t rootMenuCount(){
  return 5;
}

uint8_t profileMenuCount(){
  return totalProfiles > 255 ? 255 : (uint8_t)totalProfiles;
}

uint8_t rgbMenuCount(){
  return ledModeMenuCount;
}

uint8_t colorMenuCount(){
  return 2;
}

uint8_t hapticMenuCount(){
  return WHEEL_MODE_COUNT;
}

void enterRootMenu(){
  menuRootSelection = 0;
}

void enterProfileMenu(){
  if(totalProfiles == 0){
    profileMenuSelection = 0;
    return;
  }
  if(activeProfile < (int)totalProfiles){
    profileMenuSelection = activeProfile;
  } else {
    profileMenuSelection = totalProfiles - 1;
  }
}

void enterRGBMenu(){
  rgbMenuSelection = findLedMenuIndex(ledMode);
}

void enterColorMenu(){
  colorMenuSelection = 0;
  colorEditActive = false;
  colorEditChannel = 0;
  colorEditSelection = 0;
  colorEditPtr = nullptr;
}

void enterHapticMenu(){
  hapticTestActive = false;
  if(hapticMenuSelection >= WHEEL_MODE_COUNT){
    hapticMenuSelection = 0;
  }
}

void confirmRoot(uint8_t selection){
  switch (selection)
  {
  case 0:
    enterMenu(MENU_PROFILE);
    break;
  case 1:
    enterMenu(MENU_RGB);
    break;
  case 2:
    enterMenu(MENU_COLOR);
    break;
  case 3:
    toggleUsbStorageMode();
    break;
  case 4:
    enterMenu(MENU_HAPTIC);
    break;
  default:
    break;
  }
}

void confirmHaptic(uint8_t selection){
  if(selection >= WHEEL_MODE_COUNT){
    return;
  }

  hapticTestMode = selection;
  hapticTestActive = true; //core 0 picks this up and runs the mode without any output
}

void confirmProfile(uint8_t selection){
  applyProfileSelection(selection);
}

void confirmRGB(uint8_t selection){
  applyLedSelection(selection);
}

void confirmColor(uint8_t selection){
  colorEditSelection = selection;
  colorEditChannel = 0;
  colorEditActive = true;
  colorEditPtr = (selection == 0) ? primaryColour : secondaryColour;
  colorDirty = false;
}

const MenuDefinition menuDefinitions[] = {
  {MENU_ROOT, MENU_NONE, &menuRootSelection, rootMenuCount, drawRootMenu, confirmRoot, enterRootMenu},
  {MENU_PROFILE, MENU_ROOT, &profileMenuSelection, profileMenuCount, drawProfileMenu, confirmProfile, enterProfileMenu},
  {MENU_RGB, MENU_ROOT, &rgbMenuSelection, rgbMenuCount, drawRGBMenu, confirmRGB, enterRGBMenu},
  {MENU_COLOR, MENU_ROOT, &colorMenuSelection, colorMenuCount, drawColorMenu, confirmColor, enterColorMenu },
  {MENU_HAPTIC, MENU_ROOT, &hapticMenuSelection, hapticMenuCount, drawHapticMenu, confirmHaptic, enterHapticMenu}
};

const MenuDefinition* getMenuDefinition(MenuPage page){
  for(size_t i = 0; i < sizeof(menuDefinitions)/sizeof(menuDefinitions[0]); i++){
    if(menuDefinitions[i].id == page){
      return &menuDefinitions[i];
    }
  }
  return nullptr;
}

void clampMenuSelection(const MenuDefinition *menu){
  if(!menu || !menu->selection || !menu->countFn){
    return;
  }

  uint8_t count = menu->countFn();
  if(count == 0){
    *menu->selection = 0;
    return;
  }

  if(*menu->selection >= count){
    *menu->selection = count - 1;
  }
}

void enterMenu(MenuPage page){
  currentMenu = getMenuDefinition(page);
  menuPage = page;

  if(currentMenu && currentMenu->enterFn){
    currentMenu->enterFn();
  }

  clampMenuSelection(currentMenu);
}

void menuScroll(int8_t scroll){
  if(!currentMenu || scroll == 0 || hapticTestActive){
    return;
  }

  if(menuPage == MENU_COLOR && colorEditActive && colorEditPtr){
    int8_t delta = (scroll < 0) ? 5 : -5; //wheel direction mapped to +/-
    int16_t value = colorEditPtr[colorEditChannel] + delta;
    value = constrain(value, 0, 255);
    colorEditPtr[colorEditChannel] = (uint8_t)value;
    calculateColourMultiplier();
    colorDirty = true;
    return;
  }

  uint8_t count = currentMenu->countFn ? currentMenu->countFn() : 0;
  if(count == 0){
    return;
  }

  int16_t selection = *currentMenu->selection;
  selection += (scroll < 0 ? 1 : -1);
  selection = constrain(selection, 0, count - 1);
  *currentMenu->selection = (uint8_t)selection;
}

void menuHandleConfirm(){
  if(hapticTestActive){
    return; //already running a test, BACK is the only way out
  }

  if(menuPage == MENU_COLOR && colorEditActive){
    colorEditChannel = (colorEditChannel + 1) % 3; //cycle R,G,B
    return;
  }

  if(currentMenu && currentMenu->confirmFn){
    currentMenu->confirmFn(*currentMenu->selection);
  }
}

void menuHandleBack(){
  if(hapticTestActive){
    hapticTestActive = false; //stop the test, back to the mode list
    return;
  }

  if(menuPage == MENU_COLOR && colorEditActive){
    if(colorDirty){
      storeLastState();
      colorDirty = false;
    }
    colorEditActive = false;
    colorEditChannel = 0;
    colorEditPtr = nullptr;
    return;
  }

  if(menuPage == MENU_ROOT || !currentMenu || currentMenu->parent == MENU_NONE){
    exitMenu();
  } else {
    enterMenu(currentMenu->parent);
  }
}

void renderCurrentMenu(){
  if(currentMenu && currentMenu->renderFn){
    currentMenu->renderFn(*currentMenu->selection);
  }
}

void openMenu(){
  hapticTestActive = false;
  profileSelectMenu = true;
  profilePlusStarted = false;
  profileMinusStarted = false;
  menuButtonHandled[6] = true; //ignore current press that opened the menu
  menuButtonHandled[7] = true;
  enterMenu(MENU_ROOT);
  // Makes core 0 pick the menu up and build the Clicky model for it. The wheel
  // position itself does not need seeding, menuWheel() centres the detents on
  // wherever the wheel happens to be sitting.
  wheelModeChanged = true;
}

void exitMenu(){
  hapticTestActive = false;
  profileSelectMenu = false;
  menuPage = MENU_NONE;
  currentMenu = nullptr;
  if(colorDirty){
    storeLastState();
    colorDirty = false;
  }
  colorEditActive = false;
  colorEditChannel = 0;
  colorEditPtr = nullptr;
  profileMinusStarted = false;
  profilePlusStarted = false;
  menuReentryGuard = true; //wait for release before allowing menu again
  wheelModeChanged = true;
}

void applyProfileSelection(uint8_t selection){
  if(totalProfiles == 0){
    exitMenu();
    return;
  }

  if(selection >= totalProfiles){
    selection = totalProfiles - 1;
  }

  activeProfile = selection;
  loadProfile("/config.xml", activeProfile);
  storeLastProfile();
  Serial.print("Stored last profile = ");
  Serial.println(activeProfile);
  loadButtonIcons();
  exitMenu();
}

void applyLedSelection(uint8_t selection){
  if(ledModeMenuCount == 0){
    return;
  }

  if(selection >= ledModeMenuCount){
    selection = 0;
  }

  rgbMenuSelection = selection;
  uint8_t selectedMode = ledModeMenu[selection];
  applyLedMode(selectedMode);
  storeLastState();
  exitMenu();
}

void toggleUsbStorageMode(){
  if(usbStorageMode){
    exitUsbStorageMode();
  } else {
    enterUsbStorageMode();
  }
}

void handleMenuButtons(){
  // Button 6 = confirm/enter
  if(lastButtonState[6] && !menuButtonHandled[6]){
    menuButtonHandled[6] = true;
    menuHandleConfirm();
  }

  // Button 7 = back/exit
  if(lastButtonState[7] && !menuButtonHandled[7]){
    menuButtonHandled[7] = true;
    menuHandleBack();
  }
}

uint16_t convertConsumerKeycode(int input){
  switch(input){
    case 173:
      return HID_USAGE_CONSUMER_MUTE;
    case 174:
      return HID_USAGE_CONSUMER_VOLUME_DECREMENT;
    case 175:
      return HID_USAGE_CONSUMER_VOLUME_INCREMENT;
  }
  return 0;
}

void sendConsumerKey(uint16_t usage){
  uint16_t report = usage;
  usb_keyboard.sendReport(REPORT_ID_CONSUMER, &report, sizeof(report));
  delay(5);
  report = 0;
  usb_keyboard.sendReport(REPORT_ID_CONSUMER, &report, sizeof(report));
}

void macroOutput(int button){
  uint8_t keycode[6] = { 0 };
  int modifier = 0;
        
  for(int i = 0; i < 3; i++){
    delay(macroDelay[button][i]);
    if(macroAction[button][i] != 0){
      uint16_t consumer = convertConsumerKeycode(macroAction[button][i]);
      if(consumer != 0){
        sendConsumerKey(consumer);
        continue;
      }

      keycode[0] = convertKeycode(macroAction[button][i]);
      if(checkModifiers(macroAction[button][i]) != 0){
        modifier = checkModifiers(macroAction[button][i]);
        Serial.println("Modifier Detected");
      } else {
        usb_keyboard.keyboardReport(REPORT_ID_KEYBOARD, modifier, keycode);
      }
    }
  }
}

void loop1() {
  buttonRead();

  if(usbStorageMode){
    TinyUSBDevice.task();
    u8g2.firstPage();
    do {
      drawUsbStorageScreen();
    } while ( u8g2.nextPage() );
    delay(20);
    return;
  }

  u8g2.firstPage();
  if(ledTimer + ledSpeed < millis()){
    if(ledMode == 0){
      haloLED();
    } else if(ledMode == 1){
      breathLED();
    } else if(ledMode == 2){
      ledBand();
    } else if(ledMode == 3){
      rainbowLED();
    } else if(ledMode == 4){
      solidLED();
    } else {
      offLED();
    }
    ledTimer = millis();
  }
  
  do {
    if(sdDetected){
      if(!profileSelectMenu){
        drawGrid();
        drawActiveProfile();
      } else {
        renderCurrentMenu();
      }

      // Momentum is the one mode whose scrolling happens on core 1. Keep the
      // reading in a local: the shared encoderAngle global belongs to the core 0
      // wheel modes and clobbering it from here races against them.
      if(!profileSelectMenu && !hapticTestActive && wheelMode == WHEEL_MOMENTUM){
        float momentumAngle = encoder.getAngle();

        if(abs(lastEncoderAngle - momentumAngle) > 0.1){
          wheelScrollOutput((int)((lastEncoderAngle - momentumAngle) * WHEEL_TICKS_PER_RADIAN));
          lastEncoderAngle = momentumAngle;
        } else {
          cancelWheelAction();
        }
      }
    } else {
      u8g2.drawXBMP(59, 10, 10, 13, SD_Card);
      u8g2.setFont(u8g2_font_5x7_tf);
      u8g2.drawStr(17, 36, "No SD Card Detected!");
      u8g2.drawStr(53, 60, "Retry");
    }

  } while ( u8g2.nextPage() );
}

bool storeLastProfile(){
  SD.remove("/lastProfile");

  File file = SD.open("/lastProfile", FILE_WRITE);
  if (file) {
    file.print(activeProfile);
    file.close();
    return true;
  } else {
    return false;
  }
}

int readLastProfile(){
  File file = SD.open("/lastProfile");

  if (!file){
    return 0;
  }

  return (uint8_t)file.parseInt();
}

bool storeLastState(){
  SD.remove("/lastState");

  File file = SD.open("/lastState", FILE_WRITE);
  if(file){
    file.print(wheelMode); file.print(",");
    file.print(ledMode); file.print(",");
    file.print(primaryColour[0]); file.print(",");
    file.print(primaryColour[1]); file.print(",");
    file.print(primaryColour[2]); file.print(",");
    file.print(secondaryColour[0]); file.print(",");
    file.print(secondaryColour[1]); file.print(",");
    file.print(secondaryColour[2]);
    file.close();
    return true;
  }
  return false;
}

bool readLastState(){
  File file = SD.open("/lastState");

  if(!file){
    return false;
  }

  wheelMode = (uint8_t)file.parseInt();
  ledMode = (uint8_t)file.parseInt();
  primaryColour[0] = (uint8_t)file.parseInt();
  primaryColour[1] = (uint8_t)file.parseInt();
  primaryColour[2] = (uint8_t)file.parseInt();
  secondaryColour[0] = (uint8_t)file.parseInt();
  secondaryColour[1] = (uint8_t)file.parseInt();
  secondaryColour[2] = (uint8_t)file.parseInt();

  file.close();

  if(wheelMode < 0 || wheelMode >= WHEEL_MODE_COUNT){
    wheelMode = WHEEL_CLICKY;
  }
  if(ledMode >= LED_MODE_COUNT){
    ledMode = LED_MODE_COUNT - 1;
  }

  wheelModeChanged = true; //core 0 started before this ran, make it re-read the mode

  return true;
}
