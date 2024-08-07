 # P.I.G.S DIY Manual Version 1.01 #
 ## Intro 
 
P.I.G.S is a open source Pico Infared lightgun system .
Feel free to mod, fork, create better, complain, reports errors etc.

## FYI

- P.I.G.S is calibrated & configured in "Pause Mode".
- Pi Pico & Kee Boar 2040 are offically supported.
- For compiling the code yourself please see [Compiling-Tips](https://github.com/Fusion-Lightguns/P.I.G.S--Pico-Infared-Gun-System/blob/plus/DIY/Compiling-Tips.md)

## Table of Contents:
 - [Intial Setup](#setup)
 - [IR LED Setup](#ir-led-setup)
 - [Calibration](#calibration)
 - [Pause Mode Buttons](#pause-mode-buttons)
 - [Parts](#parts)
 - [Pin Outs](#pin-outs)
 - [Building](#building)
 - [Button Layouts](#button-layout)

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

## Pin Outs
![pico diy](https://github.com/user-attachments/assets/cf655e40-eaf2-4fca-ba93-14efd3138d28)

## IR LED Setup
![LED IR SETUP](https://github.com/user-attachments/assets/eda321df-f2b3-4665-8428-fc44db491444)

- [LED Cases Install Video](https://www.youtube.com/watch?v=8xgEFVKnlrg)

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
 
Calibration can be cancelled during any step by pressing **Calibrate**. The gun will return to pause mode without saving if you cancel the calibration.
Pause Mode can be exited at anytime by pressing **Pump Button**. 

## Pause Mode Buttons

Enter Pause Mode:
- Hold Calibrate for 3 seconds

Exit Pause Mode:
- Pump.

Cancel Calibration:
- Calibrate Button.

Skip the center calibration:
- Up Button

Save preferences to non-volatile memory:
- Press Start & Select at the same time

Increase IR sensitivity:
- Press B & Up at the same time.

Decrease IR sensitivity:
- Press B & Down at the same time.

Select a run mode:
Run Mode Normal
- Press B & Start at the same time.
Average Mode
- Press B & Select at the same time.

Toggle offscreen button mode in software:
- Press A & Left at the same time
  
Toggle offscreen button mode in software:
- Press A & Right at the same time.
  
Toggle rumble in software:
- Press A & Up at the same time.
  
Toggle solenoid in software:
- Press A & Down at the same time.
- Up -------- Select "TV Fisheye Lens" Profile
- Down ------ Select "TV Wide-angle Lens" Profile
- Left ------ Select "TV" Profile 
- Right ----- Select "Monitor" Profile

## Parts 

# Required Parts
- **Raspberry Pi Pico**
   * [Raspberry Pi Website](https://www.raspberrypi.com/products/raspberry-pi-pico/), [DFRobot USA](https://www.dfrobot.com/product-2187.html), [Mouser USA](https://www.mouser.com/ProductDetail/Raspberry-Pi/SC0915?qs=T%252BzbugeAwjgnLi4azxXVFA%3D%3D)
- **DFRobot IR Positioning Camera SEN0158:**
   * [Mouser (US Distributor)](https://www.mouser.com/ProductDetail/DFRobot/SEN0158?qs=lqAf%2FiVYw9hCccCG%2BpzjbQ%3D%3D) | [DF-Robot (International)](https://www.dfrobot.com/product-1088.html) | [Mirrors list](https://octopart.com/sen0158-dfrobot-81833633)
- **4 IR LED emitters:**
   * [Aliexpress ](https://www.aliexpress.us/item/3256804336848130.html?spm=a2g0o.productlist.main.11.17df6132X8zyCR&algo_pvid=6bb353e7-9ee1-4f8f-9d25-45b36b67ade3&algo_exp_id=6bb353e7-9ee1-4f8f-9d25-45b36b67ade3-5&pdp_npi=4%40dis%21USD%2117.00%2114.45%21%21%2117.00%2114.45%21%402103273e17145682557955218ecd82%2112000033430236398%21sea%21US%210%21AB&curPageLogUid=c8xBIGBe2J2x&utparam-url=scene%3Asearch%7Cquery_from%3A)
- **Some Buttons**
   * 16mm buttons
      - [Aliexpress](https://www.aliexpress.us/item/3256805541789163.html?spm=a2g0o.productlist.main.13.d05de70bjVWGUa&algo_pvid=d620f45d-a52a-4fef-bbeb-770a9c32f32a&algo_exp_id=d620f45d-a52a-4fef-bbeb-770a9c32f32a-6&pdp_npi=4%40dis%21USD%214.94%213.46%21%21%214.94%213.46%21%402101c5b117145683874874352ebac6%2112000034138241653%21sea%21US%210%21AB&curPageLogUid=kv6dCreXAmG0&utparam-url=scene%3Asearch%7Cquery_from%3A), [Amazon](https://www.amazon.com/s?k=16mm+buttons&crid=PBDDQRP52JGU&sprefix=16mm+buttons%2Caps%2C997&ref=nb_sb_noss_1)
    
    * 12mm buttons
       - [Aliexpress](https://www.aliexpress.us/item/3256805196612875.html?spm=a2g0o.productlist.main.27.75986334FpiucF&algo_pvid=bc4c4a9b-758b-41ff-9d4d-df101b8b289d&algo_exp_id=bc4c4a9b-758b-41ff-9d4d-df101b8b289d-13&pdp_npi=4%40dis%21USD%211.95%211.36%21%21%211.95%211.36%21%402101e58317145686059391667ebb46%2112000032829722519%21sea%21US%210%21AB&curPageLogUid=XPOrEVU52A4j&utparam-url=scene%3Asearch%7Cquery_from%3A), [Amazon](https://www.amazon.com/Momentary-Waterproof-Pushbutton-Pre-Wired-Automotive/dp/B0C8HLLGYG/ref=sr_1_1_sspa?crid=NGK3532MCKNO&dib=eyJ2IjoiMSJ9.1896D3ZcvP33azrftwkZu1kuU_R8nx7ITkHl0I0FJ7CkXozNjcAWom-Zh7Y7ARjvnLq1zQ2DiMaDUigHuk4byF0dBCPKU9fOgHGe65qAz71TX1mTVT2nbBAUAIa5BY9EN5snoQ0TqtNpMF237nd5rqzAOmZaUMW2qKw_klWz_RZ_Z_c4YBx-WD-D7Z5S1uO27PAvywWKsXehS9wcp279iYFasb_SctLCkvaa7ulK6iwgRigAdcYAiNadMgiVjDVmpFTYtaYJrPpg5y056CMRqCehb2iEbA84spwv1GIMSfk.JH94g8x2zwXtugNPAySfekqCfIULmNB-8dmzRKW8FNg&dib_tag=se&keywords=12mm+buttons&qid=1714568545&sprefix=mm+buttons%2Caps%2C888&sr=8-1-spons&sp_csd=d2lkZ2V0TmFtZT1zcF9hdGY&psc=1)
     
 - Something to put everything in.
  
# **Optional Parts:**
- **Any 12/24V solenoid,** w/ associated relay board.
     * *Requires a DC power extension cable &/or DC pigtail, and a separate adjustable 12-24V power supply.*
     * 12V Solenoid
         - [Aliexpress](https://www.aliexpress.us/item/2251832779711142.html?spm=a2g0o.productlist.main.3.70ef3FMl3FMl6X&algo_pvid=12c97581-5921-495b-8fe7-6b9cc9f726dc&algo_exp_id=12c97581-5921-495b-8fe7-6b9cc9f726dc-1&pdp_npi=4%40dis%21USD%211.68%211.01%21%21%211.68%211.01%21%40210313e917145692100275061ebe98%2112000024163706729%21sea%21US%210%21AB&curPageLogUid=9KGDNQaR9Wv3&utparam-url=scene%3Asearch%7Cquery_from%3A), 
- **Any 5V gamepad rumble motor,** w/ associated relay board.
    - [Aliexpress](https://www.aliexpress.us/item/3256805737542736.html?spm=a2g0o.productlist.main.15.5e7a240amQeJmL&algo_pvid=58c80eb6-1ec9-4f6d-9a43-38282733876c&aem_p4p_detail=202405010609048976331639092330010014192&algo_exp_id=58c80eb6-1ec9-4f6d-9a43-38282733876c-7&pdp_npi=4%40dis%21USD%212.30%211.98%21%21%212.30%211.98%21%402101f08717145689444005369eaf7d%2112000034871325153%21sea%21US%210%21AB&curPageLogUid=Pxd9JXtf4dTy&utparam-url=scene%3Asearch%7Cquery_from%3A&search_p4p_id=202405010609048976331639092330010014192_2), 
- **Any 2-way SPDT switches,** to adjust state of rumble/solenoid/rapid fire in hardware *(can be adjusted in software from pause mode if not available!)*
- **5 way directional switch.**
     - [Aliexpress](https://www.aliexpress.us/item/2255800363581884.html?spm=a2g0o.productlist.main.31.5fbb5705uVfOmJ&algo_pvid=5e3e44d8-eee3-4830-a8fd-5cae76b92e33&aem_p4p_detail=2024050106063318488343800487860009917641&algo_exp_id=5e3e44d8-eee3-4830-a8fd-5cae76b92e33-15&pdp_npi=4%40dis%21USD%210.66%210.66%21%21%210.66%210.66%21%402101ead817145687938005308e7c8a%2110000002851650036%21sea%21US%210%21AB&curPageLogUid=KwYvka6unOI9&utparam-url=scene%3Asearch%7Cquery_from%3A&search_p4p_id=2024050106063318488343800487860009917641_4), [Amazon](https://www.amazon.com/Tactile-Switch-Breakout-Module-Converter/dp/B07YY6VV9Q/ref=sr_1_8?crid=1EM5K5ONZ9NOU&dib=eyJ2IjoiMSJ9.dfU_8mDSc3W-08jjfBsxSNLmiDnGX631haWJq1OH3kj8v6TdSza-vIgSkKGujizkkDOxuXyx8x8WWXUXoNa12zzTVRz7yPEjguQXcZpLNc2IiiTjZ9BltcrTc8xTEeVu6OKLeies_IovWf8CN-s5VuvV1r68pmaBan_IYTGE_yn6aP12nlcyGf1vN_CSJVWvpUAIA2fxDCh9xx1uDLcL-gStQIp-bXQFlgaQj5t3RfY.fUKn-KRqyY08xvITLOAdpzvyeFlfMp6cEl1ls3Jtq8s&dib_tag=se&keywords=5way+tactile+switch&qid=1714568878&sprefix=5+way+tactile+switch%2Caps%2C177&sr=8-8)

## Building 

- This is ALOT for for this space lol. So for building instructions see: [Fusion Website DIY](https://www.fusionlightguns.com/diy)
 
## Button Layout

**Universal**

Exit Retropie F4 Key:
- Press Calibrate & A at the same time

Open MAME Menu Tab Key:
- Press Calibrate & B at the same time.

- Trigger - Mouse 1 ----- Gamepad Right Trigger
- Up ------ Up ---------- Gamepad Down 
- Left ---- Left -------- Gamepad Left 
- Right --- Right ------- Gamepad Right 
- Pedal --- Mouse3 ------ Gamepad X
- Pump ---- Mouse2 ------ Gamepad Y

 **Player 1**

- A ------- Enter ------- Gamepad A 
- B ------- Escape ------ Gamepad B 
- C ------- Mouse5 ------ Gamepad Left Bumper
- Start --- #1 ---------- Gamepad Start 
- Select -- #5 ---------- Gamepad Select 

   **Player 2**

- A ------- W ------- Gamepad A 
- B ------- Backspace ------ Gamepad B 
- C ------- Mouse5 ------ Gamepad Left Bumper
- Start --- #1 ---------- Gamepad Start 
- Select -- #5 ---------- Gamepad Select 

 **Player 3**

- A ------- A ------- Gamepad A 
- B ------- Page UP ------ Gamepad B 
- C ------- Mouse5 ------ Gamepad Left Bumper
- Start --- #1 ---------- Gamepad Start 
- Select -- #5 ---------- Gamepad Select 


 **Player 4**

- A ------- S ------- Gamepad A 
- B ------- Page Down ------ Gamepad B 
- C ------- Mouse5 ------ Gamepad Left Bumper
- Start --- #1 ---------- Gamepad Start 
- Select -- #5 ---------- Gamepad Select 

