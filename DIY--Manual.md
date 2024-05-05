 # P.I.G.S DIY Manual Version 1.00 #
 ## Intro 
 
P.I.G.S is a open source Pico lightgun system .
Feel free to mod, fork, create better, complain, reports errors etc.

## FYI

- P.I.G.S is calibrated & configured in "Pause Mode".
- Pi Pico & Kee Boar 2040 are offically supported.

## Table of Contents:
 - [Intial Setup](#setup)
 - [IR Emitter Setup](#ir-led-setup)
 - [Calibration](#calibration)
 - [Pause Mode Buttons](#pause-mode-buttons)
 - [Pin Outs](#pin-outs)
 - [Button Layouts](#default-buttons)

## SETUP
- Download the latest release for your board from [releases](https://github.com/Fusion-Lightguns/P.I.G.S--Pico-Gun-System/releases)
- Grab your Pico or Kee Boar and cord.
- Press & hold bootloader on Pico . Then plug it in.
- Drag N Drop whichever Player you want to use on that board.
- Unplug board .
- Build your lightgun. See more here [Fusion Website DIY](https://www.fusionlightguns.com/diy)
- Secure LED emitters to screen/tv/monitor/whatever.
- Plug in assembled gun .
- When gun first plugged in it will auto go into calibrate mode. Calibrate for your screen with [calibrate instructions](#calibration)

## IR LED Setup
- Coming soon

## Calibration

Press & hold Calibrate to enter pause mode.
When in pause mode below [Pause Mode Buttons](#pause-mode-buttons) become active.
1. Press & hold **Calibration Button** for 3 seconds, while no IR points are in sight to enter pause mode.
2. Press a dpad button to select a profile unless you want to calibrate the current profile.
3. Pull the **Trigger** (or select "Calibrate current profile" in simple pause) to begin calibration.
4. Shoot the pointer at center of the screen and press the trigger while keeping a steady aim.
5. The mouse should lock to the vertical axis. Use the **A**/**B** buttons (can be held down) to adjust the mouse vertical range. **A** will increase and **B** will decrease. Track the pointer at the top and bottom edges of the screen while adjusting.
6. Pull the **Trigger** for horizontal calibration. The mouse should lock to the horizontal axis. Use the **A**/**B** buttons (can be held down) to adjust the mouse horizontal range. **A** will increase and **B** will decrease. Track the pointer at the left and right edges of the screen while adjusting.
7. Pull the **Trigger** to finish and return to run mode. Values will apply to the currently selected profile in memory.
8. After confirming the calibration is good, enter pause mode and press Start and Select to save the calibration to non-volatile memory. These will be saved, as well as the active profile, to be restored on replug/reboot.
 
Calibration can be cancelled during any step by pressing **Start** or **Select**. The gun will return to pause mode without saving if you cancel the calibration.
Pause Mode can be exited at anytime by pressing **Reload Button**. 

## Pause Modes Buttons

- Trigger --- Begin Calibration 

- A+Left ---- Offscreen Button Toggle
- A+Right --- Auto Fire Mode Toggle
- A+Up ------ Rumble Toggle Switch
- A+Down ---- Solenoid Toggle Switch  

- B+Up ------ Camera Sensitivity Up
- B+Down ---- Camera Sensitivity Down
- B+Start --- Normal Mode
- B+Select -- Average Mode


- Up -------- Select "TV Fisheye Lens" Profile
- Down ------ Select "TV Wide-angle Lens" Profile
- Left ------ Select "TV" Profile 
- Right ----- Select "Monitor" Profile

- Start or Select -- Cancel Calibration

- Reload - Exit pause mode

## Pin Outs

 **Pi Pico**
  - COMING SOON

 **Kee Boar 2040**
  - COMING SOON 
 
## Button Layout

 **Player 1**

- A ------- Enter ------- Gamepad A 
- B ------- Escape ------ Gamepad B 
- C ------- Mouse5 ------ Gamepad Left Bumper
- Start --- #1 ---------- Gamepad Start 
- Select -- #5 ---------- Gamepad Select 
- Up ------ Up ---------- Gamepad Down 
- Left ---- Left -------- Gamepad Left 
- Right --- Right ------- Gamepad Right 
- Pump ---- Mouse3 ------ Gamepad X
- Reload -- Mouse2 ------ Gamepad Y

   **Player 2**

- A ------- Left Shift ------- Gamepad A 
- B ------- Backspace ------ Gamepad B 
- C ------- Mouse5 ------ Gamepad Left Bumper
- Start --- #1 ---------- Gamepad Start 
- Select -- #5 ---------- Gamepad Select 
- Up ------ Up ---------- Gamepad Down 
- Left ---- Left -------- Gamepad Left 
- Right --- Right ------- Gamepad Right 
- Pump ---- Mouse3 ------ Gamepad X
- Reload -- Mouse2 ------ Gamepad Y

 **Player 3**

- A ------- Left Control ------- Gamepad A 
- B ------- Page UP ------ Gamepad B 
- C ------- Mouse5 ------ Gamepad Left Bumper
- Start --- #1 ---------- Gamepad Start 
- Select -- #5 ---------- Gamepad Select 
- Up ------ Up ---------- Gamepad Down 
- Left ---- Left -------- Gamepad Left 
- Right --- Right ------- Gamepad Right 
- Pump ---- Mouse3 ------ Gamepad X
- Reload -- Mouse2 ------ Gamepad Y

 **Player 4**

- A ------- Left Alt ------- Gamepad A 
- B ------- Page Down ------ Gamepad B 
- C ------- Mouse5 ------ Gamepad Left Bumper
- Start --- #1 ---------- Gamepad Start 
- Select -- #5 ---------- Gamepad Select 
- Up ------ Up ---------- Gamepad Down 
- Left ---- Left -------- Gamepad Left 
- Right --- Right ------- Gamepad Right 
- Pump ---- Mouse3 ------ Gamepad X
- Reload -- Mouse2 ------ Gamepad Y
