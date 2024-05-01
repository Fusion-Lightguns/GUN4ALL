P.I.G.S (Pico Gun System)

Orginally this is a fork of [SeongGino Gun4ALL](https://github.com/SeongGino/ir-light-gun-plus) which is based on the [Prow Enhanced fork](https://github.com/Prow7/ir-light-gun), which in itself is based on the 4IR Beta "Big Code Update" [SAMCO project](https://github.com/samuelballantyne/IR-Light-Gun)

I liked Gun4ALL but wanted things a little different. Thus P.I.G.S was born.


## Features:

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

## Fusion Enhancements
- Uf2 file for each player#.
- Better support for Batocera , Retrobat , Retroarch etc.
- Burn code in GUI(working on it)

## Required Parts
- An Arduino-compatible microcontroller based on an **RP2040**.
  * [Raspberry Pi Pico](https://www.raspberrypi.com/products/raspberry-pi-pico/) *(cheapest, most pins available)
  * 
- **DFRobot IR Positioning Camera SEN0158:**
   * [Mouser (US Distributor)](https://www.mouser.com/ProductDetail/DFRobot/SEN0158?qs=lqAf%2FiVYw9hCccCG%2BpzjbQ%3D%3D) | [DF-Robot (International)](https://www.dfrobot.com/product-1088.html) | [Mirrors list](https://octopart.com/sen0158-dfrobot-81833633)
- **4 IR LED emitters:**
   * Links sooon
- **Some Buttons**
   * Links sooon

  
## **Optional Parts:**
- **Any 12/24V solenoid,** w/ associated relay board.
     * *Requires a DC power extension cable &/or DC pigtail, and a separate adjustable 12-24V power supply.*
- **Any 5V gamepad rumble motor,** w/ associated relay board. 
- **Any 2-way SPDT switches,** to adjust state of rumble/solenoid/rapid fire in hardware *(can be adjusted in software from pause mode if not available!)*

## Known Issues (want to fix sooner rather than later):
- Calibrating while serial activity is ongoing has a chance of causing the gun to lock up (exact cause still being investigated).
- Camera failing initialization will cause the board to lock itself in a "Device not available" loop.
  * Add extra feedback in the initial docking message for the GUI to alert the user of the camera not working?

## Thanks:
* SeongGino for a great base to work from.
* Samuel Ballantyne, for his SAMCO project.
* Prow7, for improving SAMCOs project.
  
