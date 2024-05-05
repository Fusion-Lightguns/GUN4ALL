P.I.G.S (Pico Gun System)

Orginally I started with Prow7's enhancments to Samco, added solenoid & rumble support & misc stuff. [Fusion Prow7 Edits](https://github.com/Fusion-Lightguns/Fusion-Light-Gun)

Then I found & forked [SeongGino's Gun4ALL](https://github.com/SeongGino/ir-light-gun-plus) which is based on the [Prow Enhanced fork](https://github.com/Prow7/ir-light-gun), which in itself is based on the 4IR Beta "Big Code Update" [SAMCO's project](https://github.com/samuelballantyne/IR-Light-Gun)

I liked Gun4ALL but wanted things a little different. Thus P.I.G.S was born.

## Fusion Enhancements (Uploading soon)
- Uf2 file for each player #.
- Better support for Batocera , Retrobat , Retroarch etc.
- Burn code in GUI(working on it)

## SeongGinos Enhancements.
- **Solenoid Support**
- **Rumble Motor Support**
- **Mame Hooker Support!**
- **Gamepad Output Support!**
- **RGB Support!**
- **Offscreen Button Support!**
- **Gamepad Output Support!**
- **Temperature Sensor Support!**

### Original Prow's Fork Enhancements
- Increased precision for maths and mouse pointer position
- Glitch-free DFRobot positioning (`DFRobotIRPositionEx` library)
- IR camera sensitivity adjustment (`DFRobotIRPositionEx` library)
- Faster IIC clock option for IR camera (`DFRobotIRPositionEx` library)
- Optional averaging modes can be enabled to slightly reduce mouse position jitter
- Enhanced button debouncing and handling (`LightgunButtons` library)
- Modified AbsMouse to be a 5 button device (`AbsMouse5` library, now part of `TinyUSB_Devices`)
- Multiple calibration profiles
- Save settings and calibration profiles to flash memory (SAMD) or EEPROM (RP2040)
- Built in Processing mode for use with the SAMCO Processing sketch (now part of GUN4ALL-GUI)


## Thanks:
* SeongGino for a great base to work from.
* Samuel Ballantyne, for his SAMCO project.
* Prow7, for improving SAMCOs project.
  
