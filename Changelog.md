## Previous Author Enchancments ##

**SeongGinos Enhancements.**
- **Solenoid Support**
- **Rumble Motor Support**
- **Mame Hooker Support!**
- **Gamepad Output Support!**
- **RGB Support!**
- **Offscreen Button Support!**
- **Gamepad Output Support!**
- **Temperature Sensor Support!**

**Original Prow's Fork Enhancements**
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

## Fusion Lightguns Changelog ##

**Version 1.00**
- Added **playerABtn** & **playerBBtn** to player variables.
- Replaced **EscapeKeyBtn** with **ExitKeyBtn(F4)** . Now you can exit Retropie to command line .
- Added **MenuKeyBtn** and all the stuff. This is so you can open MAME menu if you want to.
- Uploading each player seperately, with pre set pid, vid & manufacturers.
- Reworked a Gamepad mappings.
- IMO made pause mode easier to understand and control.
- Made calibrate hold to activate always. 

