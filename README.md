# CNCDan - Haptic Pad
![Alt text](title.png "Haptic Pad")

A 6 button macropad with a display for button labels and a mouse knob with haptic feedback!

> **Fork with a SmartKnob-style haptic engine.** Based on [CNCDan's Haptic Pad](https://github.com/dmcke5/Hapticpad), this fork drives the wheel with a torque-mode software model of virtual detents and endstops (ported from [scottbez1/smartknob](https://github.com/scottbez1/smartknob)) — see [Changes vs. upstream](#changes-vs-upstream).

[Project Video Link](https://youtu.be/bNUKRJQjuvQ)

#### Features

- 6 Programmable  Macro Buttons
- 128x64 OLED display for button labels and Icons
- Support for up to 256 profiles for a total of 1536 Macros!
- Easy XML configuration, no special drivers required!
- Macro button combinations can be configured with up to 3 simultaneous buttons or 3 seperate button presses with configurable delays between them.
- Micro SD Storage for button labels and config files.
- Haptic feedback mouse wheel with eight different modes. Clicky, Twist, Momentum, Free, Endstop, Friction, Snap and Magnetic. The detent, endstop and snap behaviour is modelled on the [SmartKnob](https://github.com/scottbez1/smartknob) project and is configurable in strength and feel.
- A "Haptic Test" page in the menu lets you feel every mode without changing profile.
- RGB ring with configurable colours and 5 different display modes. Halo, Bands, Breath, Rainbow, Solid and Off.
- Easy profile switching with up down profile buttons or profile list display.
- The Last profile is stored to the SD card so the macro pad will start on whichever profile was last active.

#### Bill of Materials

1x 128x64 OLED Display - https://www.waveshare.com/2.42inch-oled-module.htm

1x Magnetic Encoder Board - https://www.aliexpress.com/item/1005007469177411.html

1x 2804 100kv Brushless Gimbal Motor - https://www.aliexpress.com/item/1005006008489660.html

6x Kailh Low Profile Switches - https://www.aliexpress.com/item/1005005066585322.html

1x RP2040-Plus Board - https://www.waveshare.com/rp2040-plus.htm

1x TMC6300 Motor Driver Board - https://www.sparkfun.com/sparkfun-brushless-motor-driver-3-phase-tmc6300.html

1x Micro SD Module - https://www.aliexpress.com/item/1005010587984346.html

4x 6x1mm Magnets - https://www.aliexpress.com/item/1005009894772141.html

2x Tactile Buttons - https://www.digikey.com/en/products/detail/panasonic-electronic-components/EVQ-Q2B03W/762882 (These are the ones I used, but shipping is expensive)

2x Alternative Tactile Buttons - https://www.aliexpress.com/item/1005008326208629.html (These seem like a suitable replacement for the EVQ-Q2B03W switches but I haven't tried them)

The following parts are optional if you want to include the LEDS:

20x WS2811 LED's - https://www.aliexpress.com/item/32776731877.html

20x 0.1uF (100nF) 0603 Capacitors - https://www.aliexpress.com/item/32966526545.html

#### Hardware

9x M3x5x5 Threaded Inserts (only needed for printed housing)

3x M3x6 SHCS

2x M3x10 SHCS

10x M2.5x4 SHCS

4x M3x6 CSK

### Printing Instructions

#### - Printed version
Print all files in the 3D Files/STL's folder. You will need 6 of the keycap file, two of the Menu button file and two of the PCB spacer file.


#### - Machined Version
Print all files in the 3D Files/STL's folder except: "Custom Keycap.STL", "Macro Pad - Printed Version.STL" and "Menu Button Printed.STL"
Get all of the .STEP files in the 3D Files/STEP folder machined. Don't include the Macropad Assembly .STEP file from the main directory as it is a complete model of Macro Pad rather than an individual part. If you supply the MacroPad Housing.PDF file with the housing, you can have the mounting holes tapped for you.

### Motor Assembly

I made this quick picture to show how the wheel motor gets assembled:

![Alt text](MotorStack.png "Motor Assembly")

The "Encoder Mount.STL" part has two small holes in it that are supposed to get tapped to M2 for the encoder. If you don't have an M2 tap, you can probably just glue the encoder to the mount or even use some small self tapping screws to hold it in place.

The magnet holder should be a snug fit inside the hollow motor shaft. If it isn't, I'd recomend a drop of super glue to ensure it can't work free over time.

### PCB's

You will need to have both PCB's made to complete this project. They are very simple PCB's with only two layers, so they should be cheap!
Just upload the Zip files found in "PCB's/MacroPad" and "PCB's/MacroPad Controller Board" to your PCB manufacturer of choice and they shouldn't need any further info.

#### - Controller Board

The controller board has the main components labelled so you should easily be able to tell where everything goes. I've also labelled power and ground connections on the motor controller and SD reader so just double check you are installing them up the correct way! The SD reader should be flat against the PCB as shown in the video, whilst the motor controller gets installed on its pin headers with the "bottom" of the board face up (the side with the pin labels should be showing). The Pico should also be soldered directly to the board with its USB port facing outwards.
C1 on the Controller board is supposed to be a 10000pF 50v Capacitor. I never ended up fitting mine and I've had no issues but Sparkfun recommended it so I've left the pads there in case you decide to fit it.

#### - Main Board

If you're planning to include the LED's, do them first! There should be one corner of the LED with an arrow shaped rebate. It should point away from the pad labelled with a "1". All of the C components on this board are 0.1uF 0603 size capacitors. They are also needed to make the LED's work.

The switches you need for two profile switching buttons are: EVQ-Q2B03W

They share the same footprint as the EVQ-Q2B02W's but are a bit taller so be careful to ensure you get the right ones!

#### - Connecting the boards

All of the pads on both of the PCB's are actually tiny little through holes, I find these are much more robust for beginners which is why I chose them. The hole is too small to put the wire through so just treat them as a surface mount pad!

The controller board has a set of pads labelled "Display". Use the cable that was included with your screen and solder the wires for the display directly to these pads.
I've matched the labels on my PCB to the display so it should be easy to work out where everything goes.

The two boards get connected together with wires. Just match the labels on both boards and everything should be in the correct spot! I know this is messy compared to a ribbon cable, but I was trying to keep the project as beginner friendly as possible!

The three motor wires get connected to the pins labelled U V and W on the controller board. The software should detect the motor direction when it first starts up so any order for these wires is fine but if you have problems, try swapping two of them!

The Encoder connections are labelled too so just connect the four pins I've included on the board and ignore the remainder of the pins that are on the encoder, we don't need them!

### Software

> [!WARNING] 
> The software is still a work in progress. Everything seen in the video is functional currently, but I'm sure I will have missed some bugs as I've only had time to do some limited testing so far. I will work on clearing as many bugs as I can in the coming weeks but let me know if you run into any! If you're going to submit an issue with a bug, please try and recount your steps to re-create the bug as it will make fixing it much easier!

> [!NOTE]
> The `Software/MacroPad_V1.0.uf2` checked into this repo is the **stock upstream build** — it does *not* include the haptic engine described here. Please compile the firmware from source instead (see [Compiling](#compiling)).

You will also need to have your SD card set up correctly in order to use the macro pad.
Copy the entire contents of the "Example SD Card" folder onto your SD card to begin with — it ships with one profile per wheel mode, including the icon BMPs — then edit `config.yaml`/`config.xml` to your liking.

### XML Config

The configuration is authored as YAML in `config.yaml` (repo root) and converted to `config.xml`, which is the file the firmware reads from the SD card root. `Example SD Card/config.xml` is the deployable copy.

In the `<Settings>` tag of the XML file you will find all of the settings for the LED's, along with the P and I tuning values for the various wheel modes.

There are 6 acceptable inpts for the `<LED_Mode>` tag. If you spell the words incorrectly the commands won't work, so it would be a good idea to copy and paste from here:

Breath, Bands, Halo, Rainbow, Solid, Off

`<LED_Primary>` and `<LED_Secondary>` are the two colours used in the effects and they are formatted in the order of Red, Green and Blue with values between 0-255. Rainbow and Off obviously don't make use of these colour options and solid just sets the colour to `<LED_Primary>` and ignores `<LED_Secondary>`.

The motor tuning for the printed version of the wheel should be pretty good, so try with my default P and I values first before you do any tuning. I've left out D from the tuning options as it doesn't seem to be needed for this type of feedback and just makes tuning more complex.

`<Momentum_P>` and `<Momentum_I>` still tune Momentum mode. `<Clicky_P>`, `<Clicky_I>`, `<Twist_P>` and `<Twist_I>` are now unused because Clicky and Twist run on the haptic model described below, but they are still read so older config files keep loading unchanged.

#### Haptic settings

The wheel is driven in torque mode by a software model of virtual detents and endstops, in the same style as the [SmartKnob](https://github.com/scottbez1/smartknob) project. Every tag below is optional; leave it out and the default is used, so an old config file will still work. If you do add them, keep them in the order shown here (the parser reads the file from top to bottom).

| Tag | Default | What it does |
| --- | --- | --- |
| `<Haptic_VoltageLimit>` | 3.0 | Motor voltage ceiling, i.e. the overall strength of the feedback. The safe range is 0.5 - 5.0. Raise it for firmer endstops, but the motor will run warmer. |
| `<Haptic_DetentStrength>` | 2.5 | Default detent strength. Also sets the Clicky strength unless `<Clicky_Strength>` follows it. |
| `<Haptic_EndstopStrength>` | 3.0 | How hard the wall is at the ends of a bounded range (Endstop and Magnetic modes). |
| `<Haptic_SnapPoint>` | 1.1 | How far, in detent widths, the wheel has to travel before it clicks over to the next position. Above 1.0 gives a firm over centre click, near 0.55 makes it snap to the nearest position. Values below 0.55 are clamped. |
| `<Haptic_Range>` | 24 | Number of positions between the two endstops in Endstop mode. |
| `<Haptic_MagneticPositions>` | 0,6,12,18 | Comma separated list (up to 16) of the positions that get a detent in Magnetic mode. Everything in between is smooth. Positions outside `<Magnetic_Detents>` are ignored. |
| `<Clicky_Detents>` | 40 | Detents per revolution in Clicky mode. Also sets the position pitch of Endstop mode. |
| `<Clicky_Strength>` | 2.5 | Detent strength in Clicky mode. |
| `<Twist_Strength>` | 2.0 | Strength of the spring that returns the wheel to centre in Twist mode. |
| `<Twist_Range>` | 60 | Width in degrees of the Twist deflection range. |
| `<Friction_Strength>` | 1.0 | Amount of constant drag in Friction mode. |
| `<Snap_Strength>` | 3.0 | Detent strength in Snap mode. |
| `<Snap_Detents>` | 20 | Positions per revolution in Snap mode. |
| `<Snap_Point>` | 0.55 | Snap point for Snap mode only. Keep it near 0.55 so the wheel is always driven to the nearest position. |
| `<Magnetic_Strength>` | 2.5 | Detent strength at the magnetic detent positions. |
| `<Magnetic_Detents>` | 24 | Total number of positions in the Magnetic range. |

In the `<Profiles>` tag is where each profile is stored.

Each profile starts off with a name value assigned like this: `<Profile name="Solidworks">`
Then, there is a `<WheelMode>` and `<WheelKey>` tag. `<WheelKey>` can be any key value from this website https://keycode-visualizer.netlify.app/ and will be held down when the wheel is moving. `<WheelMode>` can be one of eight things. Again, these have to be exact so copy and paste from here to ensure they work:

- **Clicky** - virtual detents at the standard pitch, unbounded, one scroll tick per detent.
- **Twist** - spring loaded, returns to centre, scroll speed rises with how far you deflect it.
- **Momentum** - free spinning with a momentum boost, continuous scrolling.
- **Free** - motor off, continuous scrolling.
- **Endstop** - free motion inside a bounded range with a hard stop at both ends, one scroll tick per position.
- **Friction** - constant resistance while turning, continuous scrolling.
- **Snap** - detents that pull the wheel into the nearest position, one scroll tick per position.
- **Magnetic** - detents only at the positions listed in `<Haptic_MagneticPositions>`, smooth in between, one scroll tick per position.

### Menu

Hold either of the menu buttons to open the menu. The wheel uses **Clicky** haptics for menu navigation: one detent scrolls exactly one menu entry, so the selection always matches what you feel. The root menu lists the profiles and the "Haptic Test" page.

### Haptic Test page

Pick "Haptic Test" from the menu. That page lists every wheel mode; confirming one runs its motor behaviour so you can feel it while you tune the settings above. Scrolling and the wheel key are suppressed while a test is running, so nothing is sent to the PC. Press BACK to return to the list, and BACK again to leave the page.

Next is a `<MacroButtons>` tag that holds all of our Macro buttons for the profile.

Each macro button looks like this: 
```
<MacroButton>
    <Action>0,68</Action>
    <Action>0,0</Action>
    <Action>0,0</Action>
    <Label>Dimension</Label>
</MacroButton>
```
There will be 6 of these sections per profile. Each action has two values, the first is the delay (in milliseconds) to perform before the action, followed by the keycode you wish to press (using the same website I linked earlier). Setting both of these values to 0 for any of the three actions will mean nothing happens for that action.
Label is simply the name that will appear on screen for that button.

And that's it! just replicate that first example profile as many times as you like (up to 256 times, anyway) and each one will create a new profile that you can store your macros in.

### Compiling

The sketch lives in `Software/MacroPad`. Build it with [arduino-cli](https://arduino.github.io/arduino-cli/) (RP2040 core 5.5.0, TinyUSB):

```
arduino-cli compile --fqbn rp2040:rp2040:waveshare_rp2040_plus --board-options "flash=16777216_0,usbstack=tinyusb" Software/MacroPad
```

Required libraries: U8g2, Simple FOC, Adafruit TinyUSB Library, SdFat, FastLED.

To flash: hold **BOOTSEL** while plugging in the Pico, copy the generated `.uf2` onto the `RPI-RP2` drive and the board reboots itself. To update the config, put the pad into **USB storage mode** from its menu and replace `config.xml` on the SD card.

### Changes vs. upstream

- **SmartKnob-style haptic engine** (`haptics.ino`): all eight modes are driven by a torque-mode software model of virtual detents, endstops, friction, snap points and magnetic positions — configurable via the settings table above.
- **Clicky/Twist P and I values** are still read for backwards compatibility, but no longer used (Clicky and Twist run on the haptic model).
- **Menu navigation uses Clicky haptics** — one detent, one menu entry.
- **Haptic Test page** to feel every mode without changing profile.
- **`config.yaml`** is the editable source; `config.xml` is generated from it.
- **Example SD Card** ships with one profile per wheel mode and generated 15x15 icon BMPs.
