/*!
 * @file Player2.ino
 * @brief P.I.G.S Pico Infared Gun System
 *
 * @copyright Samco, https://github.com/samuelballantyne, June 2020
 * @copyright Mike Lynch, July 2021
 * @copyright That One Seong, https://github.com/SeongGino, October 2023
 * @copyright GNU Lesser General Public License
 *
 * @author [Sam Ballantyne](samuelballantyne@hotmail.com)
 * @author Mike Lynch
 * @author [That One Seong](SeongsSeongs@gmail.com)
 * @author [Gonezo Fusion Lightguns](fusionlightguns.com)
 * @date 2025
 */
#define PIGS_VERSION 1.75

#ifdef ARDUINO_ADAFRUIT_ITSYBITSY_RP2040
#define PIGS_BOARD "adafruitItsyRP2040"
#elifdef ARDUINO_ADAFRUIT_KB2040_RP2040
#define PIGS_BOARD "adafruitKB2040"
#elifdef ARDUINO_NANO_RP2040_CONNECT
#define PIGS_BOARD "arduinoNanoRP2040"
#elifdef ARDUINO_RASPBERRY_PI_PICO
#define PIGS_BOARD "rpipico"
#else
#define PIGS_BOARD "LG2040"
#endif // board


#include <Arduino.h>
#include <SamcoBoard.h>

// include TinyUSB or HID depending on USB stack option
#if defined(USE_TINYUSB)
#include <Adafruit_TinyUSB.h>
#elif defined(CFG_TUSB_MCU)
#error Incompatible USB stack. Use Adafruit TinyUSB.
#else
// Arduino USB stack (currently not supported, will not build)
#include <HID.h>
#endif

#include <TinyUSB_Devices.h>
#include <Wire.h>
#ifdef DOTSTAR_ENABLE
    #define LED_ENABLE
    #include <Adafruit_DotStar.h>
#endif // DOTSTAR_ENABLE
#ifdef NEOPIXEL_PIN
    #define LED_ENABLE
    #include <Adafruit_NeoPixel.h>
#endif
#ifdef ARDUINO_NANO_RP2040_CONNECT
    #define LED_ENABLE
    // Apparently an LED is included, but has to be communicated with through the WiFi chip (or else it throws compiler errors)
    // That said, LEDs are attached to Pins 25(G), 26(B), 27(R).
    #include <WiFiNINA.h>
#endif
#ifdef SAMCO_FLASH_ENABLE
    #include <Adafruit_SPIFlashBase.h>
#elif SAMCO_EEPROM_ENABLE
    #include <EEPROM.h>
#endif // SAMCO_FLASH_ENABLE/EEPROM_ENABLE


#include <DFRobotIRPositionEx.h>
#include <LightgunButtons.h>
#include <SamcoPositionEnhanced.h>
#include <SamcoConst.h>
#include "SamcoColours.h"
#include "SamcoPreferences.h"

#ifdef ARDUINO_ARCH_RP2040
  #include <hardware/pwm.h>
  #include <hardware/irq.h>
  // declare PWM ISR
  void rp2040pwmIrq(void);
#endif


#define DUAL_CORE

//Players & VID/PID
#define MANUFACTURER_NAME "Fusion"
#define DEVICE_NAME "Piggie 2"
#define DEVICE_VID 0x0322
#define DEVICE_PID 0x0421
#define PLAYER_NUMBER 2

// Features
#define MAMEHOOKER
#define USES_SWITCHES
#define USES_RUMBLE
#define USES_SOLENOID
#ifdef USES_SOLENOID
    // Uncomment if your build uses a TMP36 temperature sensor for a solenoid, or comment out if your solenoid doesn't need babysitting.
    #define USES_TEMP
#endif // USES_SOLENOID
#define USES_ANALOG
#define FOURPIN_LED
#define CUSTOM_NEOPIXEL

  // Which software extras should be activated? Set here if your build doesn't use toggle switches.
bool autofireActive = true;                          // Is solenoid firing in autofire (rapid) mode? false = default single shot, true = autofire
bool offscreenButton = false;                         // Does shooting offscreen also send a button input (for buggy games that don't recognize off-screen shots)? Default to off.
bool burstFireActive = false;                         // Is the solenoid firing in burst fire mode? false = default, true = 3-shot burst firing mode

#ifdef ARDUINO_ADAFRUIT_ITSYBITSY_RP2040 // For the Adafruit ItsyBitsy RP2040
#ifdef USES_SWITCHES
    int8_t autofireSwitch = 18;                   // What's the pin number of the autofire switch? Digital.
    #ifdef USES_RUMBLE
        int8_t rumbleSwitch = 19;                 // What's the pin number of the rumble switch? Digital.
    #endif // USES_RUMBLE
    #ifdef USES_SOLENOID
        int8_t solenoidSwitch = 20;               // What's the pin number of the solenoid switch? Digital.
    #endif // USES_SOLENOID
#endif // USES_SWITCHES

#ifdef USES_RUMBLE
    // If you'd rather not use a solenoid for force-feedback effects, this will change all on-screen force feedback events to use the motor instead.
    // TODO: actually finish this.
    //#define RUMBLE_FF
    #if defined(RUMBLE_FF) && defined(USES_SOLENOID)
        #error Rumble Force-feedback is incompatible with Solenoids! Use either one or the other.
    #endif // RUMBLE_FF && USES_SOLENOID
    bool rumbleActive = true;                        // Are we allowed to do rumble? Default to off.
#endif // USES_RUMBLE

#ifdef USES_SOLENOID
    bool solenoidActive = true;                      // Are we allowed to use a solenoid? Default to off.
    #ifdef USES_TEMP    
        int8_t tempPin = A2;                         // What's the pin number of the temp sensor? Needs to be analog.
    #endif // USES_TEMP
#endif // USES_SOLENOID

  // Remember: ANALOG PINS ONLY!
#ifdef USES_ANALOG
    int8_t analogPinX = -1;
    int8_t analogPinY = -1;
#endif // USES_ANALOG

  // Remember: PWM PINS ONLY!
#ifdef FOURPIN_LED
    #define LED_ENABLE
    int8_t PinR = -1;
    int8_t PinG = -1;
    int8_t PinB = -1;
    // Set if your LED is Common Anode (+, connects to 5V) rather than Common Cathode (-, connects to GND)
    bool commonAnode = true;
#endif // FOURPIN_LED

  // Any digital pin is fine for NeoPixels. Currently we only use the first "pixel".
#ifdef CUSTOM_NEOPIXEL
    #define LED_ENABLE
    #include <Adafruit_NeoPixel.h>
    int8_t customLEDpin = -1;                      // Pin number for the custom NeoPixel (strip) being used.
#endif // CUSTOM_NEOPIXEL

  // Pins setup - where do things be plugged into like? Uses GPIO codes ONLY! See also: https://learn.adafruit.com/adafruit-itsybitsy-rp2040/pinouts
int8_t rumblePin = 24;                            // What's the pin number of the rumble output? Needs to be digital.
int8_t solenoidPin = 25;                          // What's the pin number of the solenoid output? Needs to be digital.
int8_t btnTrigger = 6;                            // Programmer's note: made this just to simplify the trigger pull detection, guh.
int8_t btnGunA = 7;                               // <-- GCon 1-spec
int8_t btnGunB = 8;                               // <-- GCon 1-spec
int8_t btnCalibrate = 9;                               // Everything below are for GCon 2-spec only 
int8_t btnStart = 10;
int8_t btnSelect = 11;
int8_t btnGunUp = 1;
int8_t btnGunDown = 0;
int8_t btnGunLeft = 4;
int8_t btnGunRight = 5;
int8_t btnPedal = 12;
int8_t btnPump = -1;
int8_t btnHome = -1;

#elifdef ARDUINO_ADAFRUIT_KB2040_RP2040           // For the Adafruit KB2040 - GUN4IR-compatible defaults
#ifdef USES_SWITCHES
    int8_t autofireSwitch = -1;                   // What's the pin number of the autofire switch? Digital.
    #ifdef USES_RUMBLE
        int8_t rumbleSwitch = -1;                 // What's the pin number of the rumble switch? Digital.
    #endif // USES_RUMBLE
    #ifdef USES_SOLENOID
        int8_t solenoidSwitch = -1;               // What's the pin number of the solenoid switch? Digital.
    #endif // USES_SOLENOID
#endif // USES_SWITCHES

#ifdef USES_RUMBLE
    // If you'd rather not use a solenoid for force-feedback effects, this will change all on-screen force feedback events to use the motor instead.
    // TODO: actually finish this.
    //#define RUMBLE_FF
    #if defined(RUMBLE_FF) && defined(USES_SOLENOID)
        #error Rumble Force-feedback is incompatible with Solenoids! Use either one or the other.
    #endif // RUMBLE_FF && USES_SOLENOID
    bool rumbleActive = true;                     // Are we allowed to do rumble? Default to off.
#endif // USES_RUMBLE

#ifdef USES_SOLENOID
    bool solenoidActive = true;                   // Are we allowed to use a solenoid? Default to off.
    #ifdef USES_TEMP    
        int8_t tempPin = A0;                      // What's the pin number of the temp sensor? Needs to be analog.
    #endif // USES_TEMP
#endif // USES_SOLENOID

  // Remember: ANALOG PINS ONLY!
#ifdef USES_ANALOG
    int8_t analogPinX = -1;
    int8_t analogPinY = -1;
#endif // USES_ANALOG

  // Remember: PWM PINS ONLY!
#ifdef FOURPIN_LED
    #define LED_ENABLE
    int8_t PinR = -1;
    int8_t PinG = -1;
    int8_t PinB = -1;
    // Set if your LED is Common Anode (+, connects to 5V) rather than Common Cathode (-, connects to GND)
    bool commonAnode = true;
#endif // FOURPIN_LED

  // Any digital pin is fine for NeoPixels. Currently we only use the first "pixel".
#ifdef CUSTOM_NEOPIXEL
    #define LED_ENABLE
    #include <Adafruit_NeoPixel.h>
    int8_t customLEDpin = -1;                      // Pin number for the custom NeoPixel (strip) being used.
#endif // CUSTOM_NEOPIXEL

  // Pins setup - where do things be plugged into like? Uses GPIO codes ONLY! See also: https://learn.adafruit.com/adafruit-itsybitsy-rp2040/pinouts
int8_t rumblePin = 0;                             // What's the pin number of the rumble output? Needs to be digital.
int8_t solenoidPin = 1;                           // What's the pin number of the solenoid output? Needs to be digital.
int8_t btnTrigger = A1;                           // Programmer's note: made this just to simplify the trigger pull detection, guh.
int8_t btnGunA = 5;                              // <-- GCon 1-spec
int8_t btnGunB = 6;                                 // <-- GCon 1-spec
int8_t btnCalibrate = 7;                               // Everything below are for GCon 2-spec only 
int8_t btnStart = 8;
int8_t btnSelect = 9;
int8_t btnGunUp = 18;
int8_t btnGunDown = 20;
int8_t btnGunLeft = 19;
int8_t btnGunRight = 10;
int8_t btnPedal = A3;
int8_t btnPump = A2;
int8_t btnHome = -1;

#elifdef ARDUINO_NANO_RP2040_CONNECT
// todo: finalize arduino nano layout soon - these are just test values for now.
#ifdef USES_SWITCHES
    int8_t autofireSwitch = -1;                   // What's the pin number of the autofire switch? Digital.
    #ifdef USES_RUMBLE
        int8_t rumbleSwitch = -1;                 // What's the pin number of the rumble switch? Digital.
    #endif // USES_RUMBLE
    #ifdef USES_SOLENOID
        int8_t solenoidSwitch = -1;               // What's the pin number of the solenoid switch? Digital.
    #endif // USES_SOLENOID
#endif // USES_SWITCHES

#ifdef USES_RUMBLE
    // If you'd rather not use a solenoid for force-feedback effects, this will change all on-screen force feedback events to use the motor instead.
    // TODO: actually finish this.
    //#define RUMBLE_FF
    #if defined(RUMBLE_FF) && defined(USES_SOLENOID)
        #error Rumble Force-feedback is incompatible with Solenoids! Use either one or the other.
    #endif // RUMBLE_FF && USES_SOLENOID
    bool rumbleActive = true;                        // Are we allowed to do rumble? Default to off.
#endif // USES_RUMBLE

#ifdef USES_SOLENOID
    bool solenoidActive = true;                      // Are we allowed to use a solenoid? Default to off.
    #ifdef USES_TEMP    
        int8_t tempPin = A2;                         // What's the pin number of the temp sensor? Needs to be analog.
    #endif // USES_TEMP
#endif // USES_SOLENOID

  // Remember: ANALOG PINS ONLY!
#ifdef USES_ANALOG
    int8_t analogPinX = -1;
    int8_t analogPinY = -1;
#endif // USES_ANALOG

  // Remember: PWM PINS ONLY!
#ifdef FOURPIN_LED
    #define LED_ENABLE
    int8_t PinR = -1;
    int8_t PinG = -1;
    int8_t PinB = -1;
    // Set if your LED is Common Anode (+, connects to 5V) rather than Common Cathode (-, connects to GND)
    bool commonAnode = true;
#endif // FOURPIN_LED

  // Any digital pin is fine for NeoPixels. Currently we only use the first "pixel".
#ifdef CUSTOM_NEOPIXEL
    #define LED_ENABLE
    #include <Adafruit_NeoPixel.h>
    int8_t customLEDpin = -1;                      // Pin number for the custom NeoPixel (strip) being used.
#endif // CUSTOM_NEOPIXEL

  // Pins setup - where do things be plugged into like? Uses GPIO codes ONLY! See also: https://learn.adafruit.com/adafruit-itsybitsy-rp2040/pinouts
int8_t rumblePin = 17;
int8_t solenoidPin = 16;
int8_t btnTrigger = 15;
int8_t btnGunA = 0;
int8_t btnGunB = 1;
int8_t btnCalibrate = 18;
int8_t btnStart = 19;
int8_t btnSelect = 20;
int8_t btnGunUp = -1;
int8_t btnGunDown = -1;
int8_t btnGunLeft = -1;
int8_t btnGunRight = -1;
int8_t btnPedal = -1;
int8_t btnPump = -1;
int8_t btnHome = -1;

#elifdef ARDUINO_RASPBERRY_PI_PICO // For the Raspberry Pi Pico

#ifdef USES_SWITCHES
    int8_t autofireSwitch = -1;                   // What's the pin number of the autofire switch? Digital.
    #ifdef USES_SOLENOID
        int8_t solenoidSwitch = -1;               // What's the pin number of the solenoid switch? Digital.
    #endif // USES_SOLENOID
    #ifdef USES_RUMBLE
        int8_t rumbleSwitch = -1;                 // What's the pin number of the rumble switch? Digital.
    #endif // USES_RUMBLE
#endif // USES_SWITCHES

#ifdef USES_RUMBLE
    // If you'd rather not use a solenoid for force-feedback effects, this will change all on-screen force feedback events to use the motor instead.
    // TODO: actually finish this.
    //#define RUMBLE_FF
    #if defined(RUMBLE_FF) && defined(USES_SOLENOID)
        #error Rumble Force-feedback is incompatible with Solenoids! Use either one or the other.
    #endif // RUMBLE_FF && USES_SOLENOID
    bool rumbleActive = true;                        // Are we allowed to do rumble? Default to off.
#endif // USES_RUMBLE

#ifdef USES_SOLENOID
    bool solenoidActive = true;                      // Are we allowed to use a solenoid? Default to off.
    #ifdef USES_TEMP    
        int8_t tempPin = A2;                         // What's the pin number of the temp sensor? Needs to be analog.
    #endif // USES_TEMP
#endif // USES_SOLENOID

  // Remember: ANALOG PINS ONLY!
#ifdef USES_ANALOG
    int8_t analogPinX = -1;
    int8_t analogPinY = -1;
#endif // USES_ANALOG

  // Remember: PWM PINS ONLY!
#ifdef FOURPIN_LED
    #define LED_ENABLE
    int8_t PinR = 10;
    int8_t PinG = 11;
    int8_t PinB = 12;
    // Set if your LED is Common Anode (+, connects to 5V) rather than Common Cathode (-, connects to GND)
    bool commonAnode = true;
#endif // FOURPIN_LED

  // Any digital pin is fine for NeoPixels. Currently we only use the first "pixel".
#ifdef CUSTOM_NEOPIXEL
    #define LED_ENABLE
    #include <Adafruit_NeoPixel.h>
    int8_t customLEDpin = -1;                      // Pin number for the custom NeoPixel (strip) being used.
#endif // CUSTOM_NEOPIXEL

int8_t rumblePin = 17;                            // What's the pin number of the rumble output? Needs to be digital.
int8_t solenoidPin = 16;                          // What's the pin number of the solenoid output? Needs to be digital.
int8_t btnTrigger = 15;                           // Programmer's note: made this just to simplify the trigger pull detection, guh.
int8_t btnGunA = 0;                               // <-- GCon 1-spec
int8_t btnGunB = 1;                               // <-- GCon 1-spec
int8_t btnCalibrate = 2;                               // Everything below are for GCon 2-spec only 
int8_t btnStart = 3;
int8_t btnSelect = 4;
int8_t btnGunUp = 6;
int8_t btnGunDown = 7;
int8_t btnGunLeft = 8;
int8_t btnGunRight = 9;
int8_t btnPedal = 14;
int8_t btnPump = 13;
int8_t btnHome = 5;

#else // for LG2040 

#ifdef USES_SWITCHES
    int8_t autofireSwitch = -1;                   // What's the pin number of the autofire switch? Digital.
    #ifdef USES_SOLENOID
        int8_t solenoidSwitch = -1;               // What's the pin number of the solenoid switch? Digital.
    #endif // USES_SOLENOID
    #ifdef USES_RUMBLE
        int8_t rumbleSwitch = -1;                 // What's the pin number of the rumble switch? Digital.
    #endif // USES_RUMBLE
#endif // USES_SWITCHES

#ifdef USES_RUMBLE
    // If you'd rather not use a solenoid for force-feedback effects, this will change all on-screen force feedback events to use the motor instead.
    // TODO: actually finish this.
    //#define RUMBLE_FF
    #if defined(RUMBLE_FF) && defined(USES_SOLENOID)
        #error Rumble Force-feedback is incompatible with Solenoids! Use either one or the other.
    #endif // RUMBLE_FF && USES_SOLENOID
    bool rumbleActive = true;                        // Are we allowed to do rumble? Default to off.
#endif // USES_RUMBLE

#ifdef USES_SOLENOID
    bool solenoidActive = true;                      // Are we allowed to use a solenoid? Default to off.
    #ifdef USES_TEMP    
        int8_t tempPin = A2;                         // What's the pin number of the temp sensor? Needs to be analog.
    #endif // USES_TEMP
#endif // USES_SOLENOID

  // Remember: ANALOG PINS ONLY!
#ifdef USES_ANALOG
    int8_t analogPinX = -1;
    int8_t analogPinY = -1;
#endif // USES_ANALOG

  // Remember: PWM PINS ONLY!
#ifdef FOURPIN_LED
    #define LED_ENABLE
    int8_t PinR = 10;
    int8_t PinG = 11;
    int8_t PinB = 12;
    // Set if your LED is Common Anode (+, connects to 5V) rather than Common Cathode (-, connects to GND)
    bool commonAnode = true;
#endif // FOURPIN_LED

  // Any digital pin is fine for NeoPixels. Currently we only use the first "pixel".
#ifdef CUSTOM_NEOPIXEL
    #define LED_ENABLE
    #include <Adafruit_NeoPixel.h>
    int8_t customLEDpin = -1;                      // Pin number for the custom NeoPixel (strip) being used.
#endif // CUSTOM_NEOPIXEL

int8_t rumblePin = 17;                            // What's the pin number of the rumble output? Needs to be digital.
int8_t solenoidPin = 16;                          // What's the pin number of the solenoid output? Needs to be digital.
int8_t btnTrigger = 15;                           // Programmer's note: made this just to simplify the trigger pull detection, guh.
int8_t btnGunA = 0;                               // <-- GCon 1-spec
int8_t btnGunB = 1;                               // <-- GCon 1-spec
int8_t btnCalibrate = 2;                               // Everything below are for GCon 2-spec only 
int8_t btnStart = 3;
int8_t btnSelect = 4;
int8_t btnGunUp = 6;
int8_t btnGunDown = 7;
int8_t btnGunLeft = 8;
int8_t btnGunRight = 9;
int8_t btnPedal = 14;
int8_t btnPump = 13;
int8_t btnHome = 5;

#endif // ARDUINO_ADAFRUIT_ITSYBITSY_RP2040

bool lowButtonMode = true;                           // Flag that determines if buttons A/B will be Start/Select when pointing offscreen; for Sega Stunner and GunCon 1-spec guns mainly.
uint16_t autofireWaitFactor = 3;                      // This is the default time to wait between rapid fire pulses (from 2-4)

// Menu options:
bool simpleMenu = false;                              // Flag that determines if Pause Mode will be a simple scrolling menu; else, relies on hotkeys.
bool holdToPause = false;                             // Flag that determines if Pause Mode is invoked by a button combo hold (EnterPauseModeHoldBtnMask) when pointing offscreen; else, relies on EnterPauseModeBtnMask hotkey.
uint16_t pauseHoldLength = 4000;                      // How long the combo should be held for, in ms.

//--------------------------------------------------------------------------------------------------------------------------------------
// Sanity checks and assignments for player number -> common keyboard assignments
#if PLAYER_NUMBER == 1
    char playerStartBtn = '1';
    char playerSelectBtn = '5';
    char playerABtn = KEY_RETURN;
    char playerBBtn = KEY_BACKSPACE;
#elif PLAYER_NUMBER == 2
    char playerStartBtn = '2';
    char playerSelectBtn = '6';
    char playerABtn = 'A';
    char playerBBtn = 'B';
#elif PLAYER_NUMBER == 3
    char playerStartBtn = '3';
    char playerSelectBtn = '7';
    char playerABtn = 'C';
    char playerBBtn = 'D';
#elif PLAYER_NUMBER == 4
    char playerStartBtn = '4';
    char playerSelectBtn = '8';
    char playerABtn = 'E';
    char playerBBtn = 'F';
#else
    #error Undefined or out-of-range player number! Please set PLAYER_NUMBER to 1, 2, 3, or 4.
#endif // PLAYER_NUMBER

// enable extra serial debug during run mode
//#define PRINT_VERBOSE 1
//#define DEBUG_SERIAL 1
//#define DEBUG_SERIAL 2

// extra position glitch filtering, 
// not required after discoverving the DFRobotIRPositionEx atomic read technique
//#define EXTRA_POS_GLITCH_FILTER

// numbered index of buttons, must match ButtonDesc[] order
enum ButtonIndex_e {
    BtnIdx_Trigger = 0,
    BtnIdx_A,
    BtnIdx_B,
    BtnIdx_Start,
    BtnIdx_Select,
    BtnIdx_Up,
    BtnIdx_Down,
    BtnIdx_Left,
    BtnIdx_Right,
    BtnIdx_Calibrate,
    BtnIdx_Pedal,
    BtnIdx_Pump,
    BtnIdx_Home
};

// bit mask for each button, must match ButtonDesc[] order to match the proper button events
enum ButtonMask_e {
    BtnMask_Trigger = 1 << BtnIdx_Trigger,
    BtnMask_A = 1 << BtnIdx_A,
    BtnMask_B = 1 << BtnIdx_B,
    BtnMask_Start = 1 << BtnIdx_Start,
    BtnMask_Select = 1 << BtnIdx_Select,
    BtnMask_Up = 1 << BtnIdx_Up,
    BtnMask_Down = 1 << BtnIdx_Down,
    BtnMask_Left = 1 << BtnIdx_Left,
    BtnMask_Right = 1 << BtnIdx_Right,
    BtnMask_Calibrate = 1 << BtnIdx_Calibrate,
    BtnMask_Pedal = 1 << BtnIdx_Pedal,
    BtnMask_Pump = 1 << BtnIdx_Pump,
    BtnMask_Home = 1 << BtnIdx_Home
};

//// TODO: Try pointers for the button functions rather than direct declaration?

// Button descriptor
// The order of the buttons is the order of the button bitmask
// must match ButtonIndex_e order, and the named bitmask values for each button
// see LightgunButtons::Desc_t, format is: 
// {pin, report type, report code (ignored for internal), offscreen report type, offscreen report code, gamepad output report type, gamepad output report code, debounce time, debounce mask, label}
LightgunButtons::Desc_t LightgunButtons::ButtonDesc[] = {
    {btnTrigger, LightgunButtons::ReportType_Internal, MOUSE_LEFT, LightgunButtons::ReportType_Internal, MOUSE_LEFT, LightgunButtons::ReportType_Internal, PAD_RT, 15, BTN_AG_MASK}, // Barry says: "I'll handle this."
    {btnGunA, LightgunButtons::ReportType_Keyboard, playerABtn, LightgunButtons::ReportType_Keyboard, playerABtn, LightgunButtons::ReportType_Gamepad, PAD_A, 15, BTN_AG_MASK2},
    {btnGunB, LightgunButtons::ReportType_Keyboard, playerBBtn, LightgunButtons::ReportType_Keyboard, playerBBtn, LightgunButtons::ReportType_Gamepad, PAD_B, 15, BTN_AG_MASK2},
    {btnStart, LightgunButtons::ReportType_Keyboard, playerStartBtn, LightgunButtons::ReportType_Mouse, MOUSE_RIGHT, LightgunButtons::ReportType_Gamepad, PAD_START, 20, BTN_AG_MASK2},
    {btnSelect, LightgunButtons::ReportType_Keyboard, playerSelectBtn, LightgunButtons::ReportType_Mouse, MOUSE_MIDDLE, LightgunButtons::ReportType_Gamepad, PAD_SELECT, 20, BTN_AG_MASK2},
    {btnGunUp, LightgunButtons::ReportType_Keyboard, KEY_UP_ARROW, LightgunButtons::ReportType_Keyboard, KEY_UP_ARROW, LightgunButtons::ReportType_Gamepad, PAD_UP, 20, BTN_AG_MASK2},
    {btnGunDown, LightgunButtons::ReportType_Keyboard, KEY_DOWN_ARROW, LightgunButtons::ReportType_Keyboard, KEY_DOWN_ARROW, LightgunButtons::ReportType_Gamepad, PAD_DOWN, 20, BTN_AG_MASK2},
    {btnGunLeft, LightgunButtons::ReportType_Keyboard, KEY_LEFT_ARROW, LightgunButtons::ReportType_Keyboard, KEY_LEFT_ARROW, LightgunButtons::ReportType_Gamepad, PAD_LEFT, 20, BTN_AG_MASK2},
    {btnGunRight, LightgunButtons::ReportType_Keyboard, KEY_RIGHT_ARROW, LightgunButtons::ReportType_Keyboard, KEY_RIGHT_ARROW, LightgunButtons::ReportType_Gamepad, PAD_RIGHT, 20, BTN_AG_MASK2},
    {btnCalibrate, LightgunButtons::ReportType_Mouse, MOUSE_BUTTON5, LightgunButtons::ReportType_Mouse, MOUSE_BUTTON5, LightgunButtons::ReportType_Gamepad, PAD_LB, 15, BTN_AG_MASK2},
    {btnPedal, LightgunButtons::ReportType_Mouse, MOUSE_MIDDLE, LightgunButtons::ReportType_Mouse, MOUSE_MIDDLE, LightgunButtons::ReportType_Gamepad, PAD_X, 15, BTN_AG_MASK2},
    {btnPump, LightgunButtons::ReportType_Mouse, MOUSE_RIGHT, LightgunButtons::ReportType_Mouse, MOUSE_RIGHT, LightgunButtons::ReportType_Gamepad, PAD_Y, 15, BTN_AG_MASK2},
    {btnHome, LightgunButtons::ReportType_Internal, 0, LightgunButtons::ReportType_Internal, 0, LightgunButtons::ReportType_Internal, 0, 15, BTN_AG_MASK2}
};

// button count constant
constexpr unsigned int ButtonCount = sizeof(LightgunButtons::ButtonDesc) / sizeof(LightgunButtons::ButtonDesc[0]);

// button runtime data arrays
LightgunButtonsStatic<ButtonCount> lgbData;

// button object instance
LightgunButtons buttons(lgbData, ButtonCount);


// button combo to enter pause mode
uint32_t EnterPauseModeBtnMask = BtnMask_Calibrate;

// button combo to enter pause mode (holding ver)
uint32_t EnterPauseModeHoldBtnMask = BtnMask_Calibrate;

// press any button to exit hotkey pause mode back to run mode (this is not a button combo)
uint32_t ExitPauseModeBtnMask = BtnMask_Calibrate;

// press and hold any button to exit simple pause menu (this is not a button combo)
uint32_t ExitPauseModeHoldBtnMask = BtnMask_Calibrate;

// press any button to cancel the calibration (this is not a button combo)
uint32_t CancelCalBtnMask = BtnMask_Pedal;

// button to save preferences to non-volatile memory
uint32_t SaveBtnMask = BtnMask_Start;

// button combo to increase IR sensitivity
uint32_t IRSensitivityUpBtnMask = BtnMask_Up;

// button combo to decrease IR sensitivity
uint32_t IRSensitivityDownBtnMask = BtnMask_Down;

//RUN MODES
// button combinations to select a run mode
uint32_t RunModeNormalBtnMask = BtnMask_Left;
uint32_t RunModeAverageBtnMask = BtnMask_Right;

//TOGGLES
// button combination to toggle autofire mode in software:
uint32_t AutofireSpeedToggleBtnMask = BtnMask_Select;

// button combination to toggle rumble in software:
uint32_t RumbleToggleBtnMask = BtnMask_A;

// button combination to toggle solenoid in software:
uint32_t SolenoidToggleBtnMask = BtnMask_B;

// EXTRAS
// button combo to exit game (escape key) 
uint32_t ExitGameBtnMask = BtnMask_Pump | BtnMask_B;

// button combo to open menu(escape tab) 
uint32_t MenuBtnMask = BtnMask_Pump | BtnMask_A;


// LEDS
// colour when no IR points are seen
uint32_t IRSeen0Color = WikiColor::Red;

// colour when calibrating
uint32_t CalModeColor = WikiColor::Blue;

// number of profiles
constexpr byte ProfileCount = 4;

// run modes
// note that this is a 5 bit value when stored in the profiles
enum RunMode_e {
    RunMode_Normal = 0,         ///< Normal gun mode, no averaging
    RunMode_Average = 1,        ///< 2 frame moving average
    RunMode_Average2 = 2,       ///< weighted average with 3 frames
    RunMode_ProfileMax = 2,     ///< maximum mode allowed for profiles
    RunMode_Processing = 3,     ///< Processing test mode
    RunMode_Count
};

// profiles ----------------------------------------------------------------------------------------------
// defaults can be populated here, but any values in EEPROM/Flash will override these.
// if you have original Samco calibration values, multiply by 4 for the center position and
// scale is multiplied by 1000 and stored as an unsigned integer, see SamcoPreferences::Calibration_t
SamcoPreferences::ProfileData_t profileData[ProfileCount] = {
    {1500, 1000, 0, 0, DFRobotIRPositionEx::Sensitivity_Default, RunMode_Average, 0, 0},
    {1500, 1000, 0, 0, DFRobotIRPositionEx::Sensitivity_Default, RunMode_Average, 0, 0},
    {1500, 1000, 0, 0, DFRobotIRPositionEx::Sensitivity_Default, RunMode_Average, 0, 0},
    {1500, 1000, 0, 0, DFRobotIRPositionEx::Sensitivity_Default, RunMode_Average, 0, 0}
};
//  ------------------------------------------------------------------------------------------------------
// profile descriptor
typedef struct ProfileDesc_s {
    // button(s) to select the profile
    uint32_t buttonMask;
    
    // LED colour
    uint32_t color;

    // button label
    const char* buttonLabel;
    
    // optional profile label
    const char* profileLabel;
} ProfileDesc_t;

// profile descriptor
static const ProfileDesc_t profileDesc[ProfileCount] = {
};

// overall calibration defaults, no need to change if data saved to NV memory or populate the profile table
// see profileData[] array below for specific profile defaults
int xCenter = MouseMaxX / 2;
int yCenter = MouseMaxY / 2;
float xScale = 1.64;
float yScale = 0.95;

// step size for adjusting the scale
constexpr float ScaleStep = 0.001;

int finalX = 0;         // Values after tilt correction
int finalY = 0;

int moveXAxis = 0;      // Unconstrained mouse postion
int moveYAxis = 0;               
int moveXAxisArr[3] = {0, 0, 0};
int moveYAxisArr[3] = {0, 0, 0};
int moveIndex = 0;

int conMoveXAxis = 0;   // Constrained mouse postion
int conMoveYAxis = 0;

  // ADDITIONS HERE: the boring inits related to the things I added.
// Offscreen bits:
//bool offScreen = false;                        // To tell if we're off-screen, if offXAxis & offYAxis are true. Part of LightgunButtons now.
bool offXAxis = false;                           // Supercedes the inliner every trigger pull, we just check each axis individually.
bool offYAxis = false;

// Boring values for the solenoid timing stuff:
#ifdef USES_SOLENOID
    unsigned long previousMillisSol = 0;         // our timer (holds the last time since a successful interval pass)
    bool solenoidFirstShot = false;              // default to off, but actually set this the first time we shoot.
    uint16_t solenoidNormalInterval = 45;   // Interval for solenoid activation, in ms.
    uint16_t solenoidFastInterval = 30;     // Interval for faster solenoid activation, in ms.
    uint16_t solenoidLongInterval = 500;    // for single shot, how long to wait until we start spamming the solenoid? In ms.
    #ifdef USES_TEMP
        uint16_t tempNormal = 50;                   // Solenoid: Anything below this value is "normal" operating temperature for the solenoid, in Celsius.
        uint16_t tempWarning = 60;                  // Solenoid: Above normal temps, this is the value up to where we throttle solenoid activation, in Celsius.
        const unsigned int solenoidWarningInterval = solenoidFastInterval * 5; // for if solenoid is getting toasty.
    #endif // USES_TEMP
#endif // USES_SOLENOID

// For burst firing stuff:
byte burstFireCount = 0;                         // What shot are we on?
byte burstFireCountLast = 0;                     // What shot have we last processed?
bool burstFiring = false;                        // Are we in a burst fire command?

// For offscreen button stuff:
bool offscreenBShot = false;                     // For offscreenButton functionality, to track if we shot off the screen.
bool buttonPressed = false;                      // Sanity check.

// For autofire:
bool triggerHeld = false;                        // Trigger SHOULDN'T be being pulled by default, right?

// For rumble:
#ifdef USES_RUMBLE
    uint16_t rumbleIntensity = 255;              // The strength of the rumble motor, 0=off to 255=maxPower.
    uint16_t rumbleInterval = 125;               // How long to wait for the whole rumble command, in ms.
    unsigned long previousMillisRumble = 0;      // our time since the rumble motor event started
    bool rumbleHappening = false;                // To keep track on if this is a rumble command or not.
    bool rumbleHappened = false;                 // If we're holding, this marks we sent a rumble command already.
    // We need the rumbleHappening because of the variable nature of the PWM controlling the motor.
#endif // USES_RUMBLE

#ifdef USES_ANALOG
    bool analogIsValid;                          // Flag set true if analog stick is mapped to valid nums
    bool analogStickPolled = false;              // Flag set on if the stick was polled recently, to prevent overloading with aStick updates.
    unsigned long previousStickPoll = 0;         // timestamp of last stick poll
    byte analogPollInterval = 2;                 // amount of time to wait after irPosUpdateTick to update analog position, in ms
#endif // USES_ANALOG

#ifdef FOURPIN_LED
    bool ledIsValid;                             // Flag set true if RGB pins are mapped to valid numbers
#endif // FOURPIN_LED

#ifdef MAMEHOOKER
// For serial mode:
    bool serialMode = false;                         // Set if we're prioritizing force feedback over serial commands or not.
    bool offscreenButtonSerial = false;              // Serial-only version of offscreenButton toggle.
    byte serialQueue = 0b00000000;                   // Bitmask of events we've queued from the serial receipt.
    // from least to most significant bit: solenoid digital, solenoid pulse, rumble digital, rumble pulse, R/G/B direct, RGB (any) pulse.
    #ifdef LED_ENABLE
    unsigned long serialLEDPulsesLastUpdate = 0;     // The timestamp of the last serial-invoked LED pulse update we iterated.
    unsigned int serialLEDPulsesLength = 2;          // How long each stage of a serial-invoked pulse rumble is, in ms.
    bool serialLEDChange = false;                    // Set on if we set an LED command this cycle.
    bool serialLEDPulseRising = true;                // In LED pulse events, is it rising now? True to indicate rising, false to indicate falling; default to on for very first pulse.
    byte serialLEDPulses = 0;                        // How many LED pulses are we being told to do?
    byte serialLEDPulsesLast = 0;                    // What LED pulse we've processed last.
    byte serialLEDR = 0;                             // For the LED, how strong should it be?
    byte serialLEDG = 0;                             // Each channel is defined as three brightness values
    byte serialLEDB = 0;                             // So yeah.
    byte serialLEDPulseColorMap = 0b00000000;        // The map of what LEDs should be pulsing (we use the rightmost three of this bitmask for R, G, or B).
    #endif // LED_ENABLE
    #ifdef USES_RUMBLE
    unsigned long serialRumbPulsesLastUpdate = 0;    // The timestamp of the last serial-invoked pulse rumble we updated.
    unsigned int serialRumbPulsesLength = 60;        // How long each stage of a serial-invoked pulse rumble is, in ms.
    byte serialRumbPulseStage = 0;                   // 0 = start/rising, 1 = peak, 2 = falling, 3 = reset to start
    byte serialRumbPulses = 0;                       // If rumble is commanded to do pulse responses, how many?
    byte serialRumbPulsesLast = 0;                   // Counter of how many pulse rumbles we did so far.
    #endif // USES_RUMBLE
    #ifdef USES_SOLENOID
    unsigned long serialSolPulsesLastUpdate = 0;     // The timestamp of the last serial-invoked pulse solenoid event we updated.
    unsigned int serialSolPulsesLength = 80;         // How long to wait between each solenoid event in a pulse, in ms.
    bool serialSolPulseOn = false;                   // Because we can't just read it normally, is the solenoid getting PWM high output now?
    int serialSolPulses = 0;                         // How many solenoid pulses are we being told to do?
    int serialSolPulsesLast = 0;                     // What solenoid pulse we've processed last.
    #endif // USES_SOLENOID
#endif // MAMEHOOKER

#ifdef USE_TINYUSB
char deviceName[16] = "PIGS-Con";
unsigned int devicePID;
#endif // USE_TINYUSB

bool customPinsInUse = false;                        // For if custom pins defined in the EEPROM are overriding sketch defaults.

unsigned int lastSeen = 0;

bool justBooted = true;                              // For ops we need to do on initial boot (custom pins, joystick centering)
bool dockedSaving = false;                           // To block sending test output in docked mode.
bool dockedCalibrating = false;                      // If set, calibration will send back to docked mode.

unsigned long testLastStamp;                         // Timestamp of last print in test mode.
const byte testPrintInterval = 50;                   // Minimum time allowed between test mode printouts.

#ifdef EXTRA_POS_GLITCH_FILTER00
int badFinalTick = 0;
int badMoveTick = 0;
int badFinalCount = 0;
int badMoveCount = 0;

// number of consecutive bad move values to filter
constexpr unsigned int BadMoveCountThreshold = 3;

// Used to filter out large jumps/glitches
constexpr int BadMoveThreshold = 49 * CamToMouseMult;
#endif // EXTRA_POS_GLITCH_FILTER

// profile in use
unsigned int selectedProfile = 0;

// IR positioning camera
#if defined(ARDUINO_ADAFRUIT_ITSYBITSY_RP2040) || defined(ARDUINO_ADAFRUIT_KB2040_RP2040)
// ItsyBitsy RP2040 only exposes I2C1 pins on the board, & KB2040 is for GUN4IR board compatibility
DFRobotIRPositionEx dfrIRPos(Wire1);
#else // Pico et al does not have this limitation
//DFRobotIRPosition myDFRobotIRPosition;
DFRobotIRPositionEx dfrIRPos(Wire);
#endif

// Samco positioning
SamcoPositionEnhanced mySamco;

// operating modes
enum GunMode_e {
    GunMode_Init = -1,
    GunMode_Run = 0,
    GunMode_CalHoriz = 1,
    GunMode_CalVert = 2,
    GunMode_CalCenter = 3,
    GunMode_Pause = 4,
    GunMode_Docked = 5
};
GunMode_e gunMode = GunMode_Init;   // initial mode

enum PauseModeSelection_e {
    PauseMode_Calibrate = 0,
    PauseMode_ProfileSelect,
    PauseMode_Save,
    #ifdef USES_RUMBLE
    PauseMode_RumbleToggle,
    #endif // USES_RUMBLE
    #ifdef USES_SOLENOID
    PauseMode_SolenoidToggle,
    //PauseMode_BurstFireToggle,
    #endif // USES_SOLENOID
    PauseMode_EscapeSignal
};
// Selector for which option in the simple pause menu you're scrolled on.
byte pauseModeSelection = 0;
// Selector for which profile in the profile selector of the simple pause menu you're picking.
byte profileModeSelection;
// Flag to tell if we're in the profile selector submenu of the simple pause menu.
bool pauseModeSelectingProfile = false;

// Timestamp of when we started holding a buttons combo.
unsigned long pauseHoldStartstamp;
bool pauseHoldStarted = false;
bool pauseExitHoldStarted = false;

// run mode
RunMode_e runMode = RunMode_Normal;

// IR camera sensitivity
DFRobotIRPositionEx::Sensitivity_e irSensitivity = DFRobotIRPositionEx::Sensitivity_Default;

static const char* RunModeLabels[RunMode_Count] = {
    "Normal",
    "Averaging",
    "Averaging2",
    "Processing"
};

// preferences saved in non-volatile memory, populated with defaults 
SamcoPreferences::Preferences_t SamcoPreferences::preferences = {
    profileData, ProfileCount, // profiles
    0, // default profile
};

enum StateFlag_e {
    // print selected profile once per pause state when the COM port is open
    StateFlag_PrintSelectedProfile = (1 << 0),
    
    // report preferences once per pause state when the COM port is open
    StateFlag_PrintPreferences = (1 << 1),
    
    // enable save (allow save once per pause state)
    StateFlag_SavePreferencesEn = (1 << 2),
    
    // print preferences storage
    StateFlag_PrintPreferencesStorage = (1 << 3)
};

// when serial connection resets, these flags are set
constexpr uint32_t StateFlagsDtrReset = StateFlag_PrintSelectedProfile | StateFlag_PrintPreferences | StateFlag_PrintPreferencesStorage;

// state flags, see StateFlag_e
uint32_t stateFlags = StateFlagsDtrReset;

// internal addressable LEDs inits
#ifdef DOTSTAR_ENABLE
// note if the colours don't match then change the colour format from BGR
// apparently different lots of DotStars may have different colour ordering ¯\_(ツ)_/¯
Adafruit_DotStar dotstar(1, DOTSTAR_DATAPIN, DOTSTAR_CLOCKPIN, DOTSTAR_BGR);
#endif // DOTSTAR_ENABLE

#ifdef NEOPIXEL_PIN
Adafruit_NeoPixel neopixel(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
#endif // CUSTOM_NEOPIXEL
#ifdef CUSTOM_NEOPIXEL
Adafruit_NeoPixel* externPixel;
// Amount of pixels in a strand.
uint16_t customLEDcount = 1;
#endif // CUSTOM_NEOPIXEL

// flash transport instance
#if defined(EXTERNAL_FLASH_USE_QSPI)
    Adafruit_FlashTransport_QSPI flashTransport;
#elif defined(EXTERNAL_FLASH_USE_SPI)
    Adafruit_FlashTransport_SPI flashTransport(EXTERNAL_FLASH_USE_CS, EXTERNAL_FLASH_USE_SPI);
#endif

#ifdef SAMCO_FLASH_ENABLE
// Adafruit_SPIFlashBase non-volatile storage
// flash instance
Adafruit_SPIFlashBase flash(&flashTransport);

static const char* NVRAMlabel = "Flash";

// flag to indicate if non-volatile storage is available
// this will enable in setup()
bool nvAvailable = false;
#endif // SAMCO_FLASH_ENABLE

#ifdef SAMCO_EEPROM_ENABLE
// EEPROM non-volatile storage
static const char* NVRAMlabel = "EEPROM";

// flag to indicate if non-volatile storage is available
// unconditional for EEPROM
bool nvAvailable = true;
#endif

// non-volatile preferences error code
int nvPrefsError = SamcoPreferences::Error_NoStorage;

// preferences instance
SamcoPreferences samcoPreferences;

// number of times the IR camera will update per second
constexpr unsigned int IRCamUpdateRate = 209;

#ifdef SAMCO_NO_HW_TIMER
// use the millis() or micros() counter instead
unsigned long irPosUpdateTime = 0;
// will set this to 1 when the IR position can update
unsigned int irPosUpdateTick = 0;

#define SAMCO_NO_HW_TIMER_UPDATE() NoHardwareTimerCamTickMillis()
//define SAMCO_NO_HW_TIMER_UPDATE() NoHardwareTimerCamTickMicros()

#else
// timer will set this to 1 when the IR position can update
volatile unsigned int irPosUpdateTick = 0;
#endif // SAMCO_NO_HW_TIMER

#ifdef DEBUG_SERIAL
static unsigned long serialDbMs = 0;
static unsigned long frameCount = 0;
static unsigned long irPosCount = 0;
#endif

// used for periodic serial prints
unsigned long lastPrintMillis = 0;

#ifdef USE_TINYUSB
// AbsMouse5 instance (this is hardcoded as the second definition, so weh)
AbsMouse5_ AbsMouse5(2);
#else
// AbsMouse5 instance
AbsMouse5_ AbsMouse5(1);
#endif

//-----------------------------------------------------------------------------------------------------
// The main show!
void setup() {
    Serial.begin(9600);   // 9600 = 1ms data transfer rates, default for MAMEHOOKER COM devices.
    Serial.setTimeout(0);
 
#ifdef SAMCO_FLASH_ENABLE
    // init flash and load saved preferences
    nvAvailable = flash.begin();
#else
#if defined(SAMCO_EEPROM_ENABLE) && defined(ARDUINO_ARCH_RP2040)
    // initialize EEPROM device. Arduino AVR has a 1k flash, so use that.
    EEPROM.begin(1024); 
#endif // SAMCO_EEPROM_ENABLE/RP2040
#endif // SAMCO_FLASH_ENABLE
    
    if(nvAvailable) {
        LoadPreferences();
        if(nvPrefsError == SamcoPreferences::Error_NoData) {
            Serial.println("No data detected, setting defaults!");
            SamcoPreferences::ResetPreferences();
        } else if(nvPrefsError == SamcoPreferences::Error_Success) {
            Serial.println("Data detected, pulling settings from EEPROM!");
            // use values from preferences
            ApplyInitialPrefs();
            ExtPreferences(true);
        }
    }
 
    // We're setting our custom USB identifiers, as defined in the configuration area!
    #ifdef USE_TINYUSB
        TinyUSBInit();
    #endif // USE_TINYUSB

#if defined(ARDUINO_ADAFRUIT_ITSYBITSY_RP2040) || defined(ARDUINO_ADAFRUIT_KB2040_RP2040)
    // ensure Wire1 SDA and SCL are correct for the ItsyBitsy RP2040 and KB2040
    Wire1.setSDA(2);
    Wire1.setSCL(3);
#endif
#ifdef ARDUINO_NANO_RP2040_CONNECT
    Wire.setSDA(12);
    Wire.setSCL(13);
#else // assumes ARDUINO_RASPBERRY_PI_PICO
    Wire.setSDA(20);
    Wire.setSCL(21);
#endif // board

    // initialize buttons & feedback devices
    buttons.Begin();
    FeedbackSet();
    #ifdef LED_ENABLE
        LedInit();
    #endif // LED_ENABLE

    // Start IR Camera with basic data format
    dfrIRPos.begin(DFROBOT_IR_IIC_CLOCK, DFRobotIRPositionEx::DataFormat_Basic, irSensitivity);
    
#ifdef USE_TINYUSB
    // Initializing the USB devices chunk.
    TinyUSBDevices.begin(1);
#endif
    
    AbsMouse5.init(MouseMaxX, MouseMaxY, true);
    
    // fetch the calibration data, other values already handled in ApplyInitialPrefs() 
    SelectCalPrefs(selectedProfile);

#ifdef USE_TINYUSB
    // wait until device mounted
    while(!USBDevice.mounted()) { yield(); }
#else
    // was getting weird hangups... maybe nothing, or maybe related to dragons, so wait a bit
    delay(100);
#endif

    // IR camera maxes out motion detection at ~300Hz, and millis() isn't good enough
    startIrCamTimer(IRCamUpdateRate);

    // First boot sanity checks.
    // Check if loading has failde
    if(nvPrefsError != SamcoPreferences::Error_Success ||
    profileData[selectedProfile].xCenter == 0 || profileData[selectedProfile].yCenter == 0) {
        // SHIT, it's a first boot! Prompt to start calibration.
        Serial.println("Preferences data is empty!");
        SetMode(GunMode_CalCenter);
        Serial.println("Pull the trigger to start your first calibration!");
        unsigned int timerIntervalShort = 600;
        unsigned int timerInterval = 1000;
        LedOff();
        unsigned long lastT = millis();
        bool LEDisOn = false;
        while(!(buttons.pressedReleased == BtnMask_Trigger)) {
            // Check and process serial commands, in case user needs to change EEPROM settings.
            if(Serial.available()) {
                SerialProcessingDocked();
            }
            if(gunMode == GunMode_Docked) {
                ExecGunModeDocked();
                SetMode(GunMode_CalCenter);
            }
            buttons.Poll(1);
            buttons.Repeat();
            // LED update:
            if(LEDisOn) {
                unsigned long t = millis();
                if(t - lastT > timerInterval) {
                    LedOff();
                    LEDisOn = false;
                    lastT = millis();
                }
            } else {
                unsigned long t = millis();
                if(t - lastT > timerIntervalShort) {
                    LedUpdate(255,125,0);
                    LEDisOn = true;
                    lastT = millis();
                } 
            }
        }
    } else {
        // this will turn off the DotStar/RGB LED and ensure proper transition to Run
        SetMode(GunMode_Run);
    }
}

// inits and/or re-sets feedback pins using currently loaded pin values
// is run both in setup and at runtime
void FeedbackSet()
{
    #ifdef USES_RUMBLE
        if(rumblePin >= 0) {
            pinMode(rumblePin, OUTPUT);
        } else {
            rumbleActive = false;
        }
    #endif // USES_RUMBLE
    #ifdef USES_SOLENOID
        if(solenoidPin >= 0) {
            pinMode(solenoidPin, OUTPUT);
        } else {
            solenoidActive = false;
        }
    #endif // USES_SOLENOID
    #ifdef USES_SWITCHES
        #ifdef USES_RUMBLE
            if(rumbleSwitch >= 0) {
                pinMode(rumbleSwitch, INPUT_PULLUP);
            }
        #endif // USES_RUMBLE
        #ifdef USES_SOLENOID
            if(solenoidSwitch >= 0) {
                pinMode(solenoidSwitch, INPUT_PULLUP);
            }  
        #endif // USES_SOLENOID
        if(autofireSwitch >= 0) {
            pinMode(autofireSwitch, INPUT_PULLUP);
        }
    #endif // USES_SWITCHES
    #ifdef USES_ANALOG
        analogReadResolution(12);
        #ifdef USES_TEMP
        if(analogPinX >= 0 && analogPinY >= 0 && analogPinX != analogPinY &&
        analogPinX != tempPin && analogPinY != tempPin) {
        #else
        if(analogPinX >= 0 && analogPinY >= 0 && analogPinX != analogPinY) {
        #endif // USES_TEMP
            //pinMode(analogPinX, INPUT);
            //pinMode(analogPinY, INPUT);
            analogIsValid = true;
        } else {
            analogIsValid = false;
        }
    #endif // USES_ANALOG
    #if defined(LED_ENABLE) && defined(FOURPIN_LED)
    if(PinR < 0 || PinG < 0 || PinB < 0) {
        Serial.println("RGB values not valid! Disabling four pin access.");
        ledIsValid = false;
    } else {
        pinMode(PinR, OUTPUT);
        pinMode(PinG, OUTPUT);
        pinMode(PinB, OUTPUT);
        ledIsValid = true;
    }
    #endif // FOURPIN_LED
    #ifdef CUSTOM_NEOPIXEL
    if(customLEDpin >= 0) {
        externPixel = new Adafruit_NeoPixel(customLEDcount, customLEDpin, NEO_GRB + NEO_KHZ800);
    }
    #endif // CUSTOM_NEOPIXEL
}

// clears currently loaded feedback pins
// is run both in setup and at runtime
void PinsReset()
{
    #ifdef USES_RUMBLE
        if(rumblePin >= 0) {
            pinMode(rumblePin, INPUT);
        }
    #endif // USES_RUMBLE
    #ifdef USES_SOLENOID
        if(solenoidPin >= 0) {
            pinMode(solenoidPin, INPUT);
        }
    #endif // USES_SOLENOID
    #ifdef USES_SWITCHES
        #ifdef USES_RUMBLE
            if(rumbleSwitch >= 0) {
                pinMode(rumbleSwitch, INPUT);
            }
        #endif // USES_RUMBLE
        #ifdef USES_SOLENOID
            if(solenoidSwitch >= 0) {
                pinMode(solenoidSwitch, INPUT);
            }  
        #endif // USES_SOLENOID
        if(autofireSwitch >= 0) {
            pinMode(autofireSwitch, INPUT);
        }
    #endif // USES_SWITCHES
    #ifdef LED_ENABLE
        LedOff();
        #ifdef FOURPIN_LED
            if(ledIsValid) {
                pinMode(PinR, INPUT);
                pinMode(PinG, INPUT);
                pinMode(PinB, INPUT);
            }
        #endif // FOURPIN_LED
        #ifdef CUSTOM_NEOPIXEL
            if(externPixel != nullptr) {
                delete externPixel;
            }
            if(customLEDpin >= 0) {
                externPixel = new Adafruit_NeoPixel(customLEDcount, customLEDpin, NEO_GRB + NEO_KHZ800);
                externPixel->begin();
            }
        #endif // CUSTOM_NEOPIXEL
    #endif // LED_ENABLE
}

#ifdef USE_TINYUSB
// Initializes TinyUSB identifier
// Values are pulled from EEPROM values that were loaded earlier in setup()
void TinyUSBInit()
{
    TinyUSBDevice.setManufacturerDescriptor(MANUFACTURER_NAME);
    for(byte i = 0; i < sizeof(deviceName); i++) {
        deviceName[i] = '\0';
    }
    for(byte i = 0; i < 16; i++) {
        deviceName[i] = EEPROM.read(EEPROM.length() - 18 + i);
    }
    EEPROM.get(EEPROM.length() - 22, devicePID);
    if(devicePID) {
        TinyUSBDevice.setID(DEVICE_VID, devicePID);
        if(deviceName[0] == '\0') {
            TinyUSBDevice.setProductDescriptor(DEVICE_NAME);
        } else {
            TinyUSBDevice.setProductDescriptor(deviceName);
        }
    } else {
        TinyUSBDevice.setProductDescriptor(DEVICE_NAME);
        TinyUSBDevice.setID(DEVICE_VID, DEVICE_PID);
    }
}
#endif // USE_TINYUSB

void startIrCamTimer(int frequencyHz)
{
#if defined(SAMCO_SAMD21)
    startTimerEx(&TC4->COUNT16, GCLK_CLKCTRL_ID_TC4_TC5, TC4_IRQn, frequencyHz);
#elif defined(SAMCO_SAMD51)
    startTimerEx(&TC3->COUNT16, TC3_GCLK_ID, TC3_IRQn, frequencyHz);
#elif defined(SAMCO_ATMEGA32U4)
    startTimer3(frequencyHz);
#elif defined(SAMCO_RP2040)
    rp2040EnablePWMTimer(0, frequencyHz);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, rp2040pwmIrq);
    irq_set_enabled(PWM_IRQ_WRAP, true);
#endif
}

#if defined(SAMCO_SAMD21)
void startTimerEx(TcCount16* ptc, uint16_t gclkCtrlId, IRQn_Type irqn, int frequencyHz)
{
    // use Generic clock generator 0
    GCLK->CLKCTRL.reg = (uint16_t)(GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | gclkCtrlId);
    while(GCLK->STATUS.bit.SYNCBUSY == 1); // wait for sync
    
    ptc->CTRLA.bit.ENABLE = 0;
    while(ptc->STATUS.bit.SYNCBUSY == 1); // wait for sync
    
    // Use the 16-bit timer
    ptc->CTRLA.reg |= TC_CTRLA_MODE_COUNT16;
    while(ptc->STATUS.bit.SYNCBUSY == 1); // wait for sync
    
    // Use match mode so that the timer counter resets when the count matches the compare register
    ptc->CTRLA.reg |= TC_CTRLA_WAVEGEN_MFRQ;
    while(ptc->STATUS.bit.SYNCBUSY == 1); // wait for sync
    
    // Set prescaler
    ptc->CTRLA.reg |= TIMER_TC_CTRLA_PRESCALER_DIV;
    while(ptc->STATUS.bit.SYNCBUSY == 1); // wait for sync
    
    setTimerFrequency(ptc, frequencyHz);
    
    // Enable the compare interrupt
    ptc->INTENSET.reg = 0;
    ptc->INTENSET.bit.MC0 = 1;
    
    NVIC_EnableIRQ(irqn);
    
    ptc->CTRLA.bit.ENABLE = 1;
    while(ptc->STATUS.bit.SYNCBUSY == 1); // wait for sync
}

void TC4_Handler()
{
    // If this interrupt is due to the compare register matching the timer count
    if(TC4->COUNT16.INTFLAG.bit.MC0 == 1) {
        // clear interrupt
        TC4->COUNT16.INTFLAG.bit.MC0 = 1;

        irPosUpdateTick = 1;
    }
}
#endif // SAMCO_SAMD21

#if defined(SAMCO_SAMD51)
void startTimerEx(TcCount16* ptc, uint16_t gclkCtrlId, IRQn_Type irqn, int frequencyHz)
{
    // use Generic clock generator 0
    GCLK->PCHCTRL[gclkCtrlId].reg = GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN;
    while(GCLK->SYNCBUSY.reg); // wait for sync
    
    ptc->CTRLA.bit.ENABLE = 0;
    while(ptc->SYNCBUSY.bit.STATUS == 1); // wait for sync
    
    // Use the 16-bit timer
    ptc->CTRLA.reg |= TC_CTRLA_MODE_COUNT16;
    while(ptc->SYNCBUSY.bit.STATUS == 1); // wait for sync
    
    // Use match mode so that the timer counter resets when the count matches the compare register
    ptc->WAVE.bit.WAVEGEN = TC_WAVE_WAVEGEN_MFRQ;
    while(ptc->SYNCBUSY.bit.STATUS == 1); // wait for sync
    
    // Set prescaler
    ptc->CTRLA.reg |= TIMER_TC_CTRLA_PRESCALER_DIV;
    while(ptc->SYNCBUSY.bit.STATUS == 1); // wait for sync
    
    setTimerFrequency(ptc, frequencyHz);
    
    // Enable the compare interrupt
    ptc->INTENSET.reg = 0;
    ptc->INTENSET.bit.MC0 = 1;
    
    NVIC_EnableIRQ(irqn);
    
    ptc->CTRLA.bit.ENABLE = 1;
    while(ptc->SYNCBUSY.bit.STATUS == 1); // wait for sync
}

void TC3_Handler()
{
    // If this interrupt is due to the compare register matching the timer count
    if(TC3->COUNT16.INTFLAG.bit.MC0 == 1) {
        // clear interrupt
        TC3->COUNT16.INTFLAG.bit.MC0 = 1;

        irPosUpdateTick = 1;
    }
}
#endif // SAMCO_SAMD51

#if defined(SAMCO_SAMD21) || defined(SAMCO_SAMD51)
void setTimerFrequency(TcCount16* ptc, int frequencyHz)
{
    int compareValue = (F_CPU / (TIMER_PRESCALER_DIV * frequencyHz));

    // Make sure the count is in a proportional position to where it was
    // to prevent any jitter or disconnect when changing the compare value.
    ptc->COUNT.reg = map(ptc->COUNT.reg, 0, ptc->CC[0].reg, 0, compareValue);
    ptc->CC[0].reg = compareValue;

#if defined(SAMCO_SAMD21)
    while(ptc->STATUS.bit.SYNCBUSY == 1);
#elif defined(SAMCO_SAMD51)
    while(ptc->SYNCBUSY.bit.STATUS == 1);
#endif
}
#endif

#ifdef SAMCO_ATMEGA32U4
void startTimer3(unsigned long frequencyHz)
{
    // disable comapre output mode
    TCCR3A = 0;
    
    //set the pre-scalar to 8 and set Clear on Compare
    TCCR3B = (1 << CS31) | (1 << WGM32); 
    
    // set compare value
    OCR3A = F_CPU / (8UL * frequencyHz);
    
    // enable Timer 3 Compare A interrupt
    TIMSK3 = 1 << OCIE3A;
}

// Timer3 compare A interrupt
ISR(TIMER3_COMPA_vect)
{
    irPosUpdateTick = 1;
}
#endif // SAMCO_ATMEGA32U4

#ifdef ARDUINO_ARCH_RP2040
void rp2040EnablePWMTimer(unsigned int slice_num, unsigned int frequency)
{
    pwm_config pwmcfg = pwm_get_default_config();
    float clkdiv = (float)clock_get_hz(clk_sys) / (float)(65535 * frequency);
    if(clkdiv < 1.0f) {
        clkdiv = 1.0f;
    } else {
        // really just need to round up 1 lsb
        clkdiv += 2.0f / (float)(1u << PWM_CH1_DIV_INT_LSB);
    }
    
    // set the clock divider in the config and fetch the actual value that is used
    pwm_config_set_clkdiv(&pwmcfg, clkdiv);
    clkdiv = (float)pwmcfg.div / (float)(1u << PWM_CH1_DIV_INT_LSB);
    
    // calculate wrap value that will trigger the IRQ for the target frequency
    pwm_config_set_wrap(&pwmcfg, (float)clock_get_hz(clk_sys) / (frequency * clkdiv));
    
    // initialize and start the slice and enable IRQ
    pwm_init(slice_num, &pwmcfg, true);
    pwm_set_irq_enabled(slice_num, true);
}

void rp2040pwmIrq(void)
{
    pwm_hw->intr = 0xff;
    irPosUpdateTick = 1;
}
#endif

#ifdef SAMCO_NO_HW_TIMER
void NoHardwareTimerCamTickMicros()
{
    unsigned long us = micros();
    if(us - irPosUpdateTime >= 1000000UL / IRCamUpdateRate) {
        irPosUpdateTime = us;
        irPosUpdateTick = 1;
    }
}

void NoHardwareTimerCamTickMillis()
{
    unsigned long ms = millis();
    if(ms - irPosUpdateTime >= (1000UL + (IRCamUpdateRate / 2)) / IRCamUpdateRate) {
        irPosUpdateTime = ms;
        irPosUpdateTick = 1;
    }
}
#endif // SAMCO_NO_HW_TIMER

#if defined(ARDUINO_ARCH_RP2040) && defined(DUAL_CORE)
// Second core setup
// does... nothing, since timing is kinda important.
void setup1()
{
    // i sleep
}

// Second core main loop
// currently handles all button & serial processing when Core 0 is in ExecRunMode()
void loop1()
{
    while(gunMode == GunMode_Run) {
        // For processing the trigger specifically.
        // (buttons.debounced is a binary variable intended to be read 1 bit at a time, with the 0'th point == rightmost == decimal 1 == trigger, 3 = start, 4 = select)
        buttons.Poll(0);

        #ifdef MAMEHOOKER
            // Stalling mitigation: make both cores pause when reading serial
            if(Serial.available()) {
                SerialProcessing();
            }
            if(!serialMode) {   // Have we released a serial signal pulse? If not,
                if(bitRead(buttons.debounced, 0)) {   // Check if we pressed the Trigger this run.
                    TriggerFire();                                      // Handle button events and feedback ourselves.
                } else {   // Or we haven't pressed the trigger.
                    TriggerNotFire();                                   // Releasing button inputs and sending stop signals to feedback devices.
                }
            } else {   // This is if we've received a serial signal pulse in the last n millis.
                // For safety reasons, we're just using the second core for polling, and the main core for sending signals entirely. Too much a headache otherwise. =w='
                if(bitRead(buttons.debounced, 0)) {   // Check if we pressed the Trigger this run.
                    TriggerFireSimple();                                // Since serial is handling our devices, we're just handling button events.
                } else {   // Or if we haven't pressed the trigger,
                    TriggerNotFireSimple();                             // Release button inputs.
                }
                SerialHandling();                                       // Process the force feedback.
            }
        #else
            if(bitRead(buttons.debounced, 0)) {   // Check if we pressed the Trigger this run.
                TriggerFire();                                          // Handle button events and feedback ourselves.
            } else {   // Or we haven't pressed the trigger.
                TriggerNotFire();                                       // Releasing button inputs and sending stop signals to feedback devices.
            }
        #endif // MAMEHOOKER

        #ifdef USES_ANALOG
            if(analogIsValid) {
                // Poll the analog values 2ms after the IR sensor has updated so as not to overload the USB buffer.
                // stickPolled and previousStickPoll are reset/updated after irPosUpdateTick.
                if(!analogStickPolled) {
                    if(buttons.pressed || buttons.released) {
                        previousStickPoll = millis();
                    } else if(millis() - previousStickPoll > analogPollInterval) {
                        AnalogStickPoll();
                        analogStickPolled = true;
                    }
                }
            }
        #endif // USES_ANALOG

        if(holdToPause) {
            if((buttons.debounced == EnterPauseModeHoldBtnMask)
            && !lastSeen && !pauseHoldStarted) {
                pauseHoldStarted = true;
                pauseHoldStartstamp = millis();
                if(!serialMode) {
                    Serial.println("Started holding pause mode signal buttons!");
                }
            } else if(pauseHoldStarted && (buttons.debounced != EnterPauseModeHoldBtnMask || lastSeen)) {
                pauseHoldStarted = false;
                if(!serialMode) {
                    Serial.println("Either stopped holding pause mode buttons, aimed onscreen, or pressed other buttons");
                }
            } else if(pauseHoldStarted) {
                unsigned long t = millis();
                if(t - pauseHoldStartstamp > pauseHoldLength) {
                    // MAKE SURE EVERYTHING IS DISENGAGED:
                    #ifdef USES_SOLENOID
                        digitalWrite(solenoidPin, LOW);
                        solenoidFirstShot = false;
                    #endif // USES_SOLENOID
                    #ifdef USES_RUMBLE
                        digitalWrite(rumblePin, LOW);
                        rumbleHappening = false;
                        rumbleHappened = false;
                    #endif // USES_RUMBLE
                    offscreenBShot = false;
                    buttonPressed = false;
                    triggerHeld = false;
                    burstFiring = false;
                    burstFireCount = 0;
                    SetMode(GunMode_Pause);
                    buttons.ReportDisable();
                }
            }
        } else {
            if(buttons.pressedReleased == EnterPauseModeBtnMask) {
                // MAKE SURE EVERYTHING IS DISENGAGED:
                #ifdef USES_SOLENOID
                    digitalWrite(solenoidPin, LOW);
                    solenoidFirstShot = false;
                #endif // USES_SOLENOID
                #ifdef USES_RUMBLE
                    digitalWrite(rumblePin, LOW);
                    rumbleHappening = false;
                    rumbleHappened = false;
                #endif // USES_RUMBLE
                offscreenBShot = false;
                buttonPressed = false;
                triggerHeld = false;
                burstFiring = false;
                burstFireCount = 0;
                buttons.ReportDisable();
                SetMode(GunMode_Pause);
                // at this point, the other core should be stopping us now.
            } else if(buttons.pressedReleased == BtnMask_Home) {
                // MAKE SURE EVERYTHING IS DISENGAGED:
                #ifdef USES_SOLENOID
                    digitalWrite(solenoidPin, LOW);
                    solenoidFirstShot = false;
                #endif // USES_SOLENOID
                #ifdef USES_RUMBLE
                    digitalWrite(rumblePin, LOW);
                    rumbleHappening = false;
                    rumbleHappened = false;
                #endif // USES_RUMBLE
                offscreenBShot = false;
                buttonPressed = false;
                triggerHeld = false;
                burstFiring = false;
                burstFireCount = 0;
                buttons.ReportDisable();
                SetMode(GunMode_Pause);
                // at this point, the other core should be stopping us now.
            }
        }
    }
}
#endif // ARDUINO_ARCH_RP2040 || DUAL_CORE

// Main core events hub
// splits off into subsequent ExecModes depending on circumstances
void loop()
{
    #ifdef SAMCO_NO_HW_TIMER
        SAMCO_NO_HW_TIMER_UPDATE();
    #endif // SAMCO_NO_HW_TIMER
    
    // poll/update button states with 1ms interval so debounce mask is more effective
    buttons.Poll(1);
    buttons.Repeat();
    #ifdef MAMEHOOKER
    while(Serial.available()) {                             // So we can process serial requests while in Pause Mode.
        SerialProcessing();
    }
    #endif // MAMEHOOKER

    if(holdToPause && pauseHoldStarted) {
        #ifdef USES_RUMBLE
            analogWrite(rumblePin, rumbleIntensity);
            delay(300);
            digitalWrite(rumblePin, LOW);
        #endif // USES_RUMBLE
        while(buttons.debounced != 0) {
            // Should release the buttons to continue, pls.
            buttons.Poll(1);
        }
        pauseHoldStarted = false;
        pauseModeSelection = PauseMode_Calibrate;
        pauseModeSelectingProfile = false;
    }

    switch(gunMode) {
        case GunMode_Pause:
            if(simpleMenu) {
                if(pauseModeSelectingProfile) {
                    if(buttons.pressedReleased == BtnMask_A) {
                        SetProfileSelection(false);
                    } else if(buttons.pressedReleased == BtnMask_B) {
                        SetProfileSelection(true);
                    } else if(buttons.pressedReleased == BtnMask_Trigger) {
                        SelectCalProfile(profileModeSelection);
                        pauseModeSelectingProfile = false;
                        pauseModeSelection = PauseMode_Calibrate;
                        if(!serialMode) {
                            Serial.print("Switched to profile: ");
                            Serial.println(profileDesc[selectedProfile].profileLabel);
                            Serial.println("Going back to the main menu...");
                            Serial.println("Selecting: Calibrate current profile");
                        }
                    } else if(buttons.pressedReleased & ExitPauseModeBtnMask) {
                        if(!serialMode) {
                            Serial.println("Exiting profile selection.");
                        }
                        pauseModeSelectingProfile = false;
                        #ifdef LED_ENABLE
                            for(byte i = 0; i < 2; i++) {
                                LedUpdate(180,180,180);
                                delay(125);
                                LedOff();
                                delay(100);
                            }
                            LedUpdate(255,0,0);
                        #endif // LED_ENABLE
                        pauseModeSelection = PauseMode_Calibrate;
                    }
                } else if(buttons.pressedReleased == BtnMask_A) {
                    SetPauseModeSelection(false);
                } else if(buttons.pressedReleased == BtnMask_B) {
                    SetPauseModeSelection(true);
                } else if(buttons.pressedReleased == BtnMask_Trigger) {
                    switch(pauseModeSelection) {
                        case PauseMode_Calibrate:
                          SetMode(GunMode_CalCenter);
                          if(!serialMode) {
                              Serial.print("Calibrating for current profile: ");
                              Serial.println(profileDesc[selectedProfile].profileLabel);
                          }
                          break;
                        case PauseMode_ProfileSelect:
                          if(!serialMode) {
                              Serial.println("Pick a profile!");
                              Serial.print("Current profile in use: ");
                              Serial.println(profileDesc[selectedProfile].profileLabel);
                          }
                          pauseModeSelectingProfile = true;
                          profileModeSelection = selectedProfile;
                          #ifdef LED_ENABLE
                              SetLedPackedColor(profileDesc[profileModeSelection].color);
                          #endif // LED_ENABLE
                          break;
                        case PauseMode_Save:
                          if(!serialMode) {
                              Serial.println("Saving...");
                          }
                          SavePreferences();
                          break;
                        #ifdef USES_RUMBLE
                        case PauseMode_RumbleToggle:
                          if(!serialMode) {
                              Serial.println("Toggling rumble!");
                          }
                          RumbleToggle();
                          break;
                        #endif // USES_RUMBLE
                        #ifdef USES_SOLENOID
                        case PauseMode_SolenoidToggle:
                          if(!serialMode) {
                              Serial.println("Toggling solenoid!");
                          }
                          SolenoidToggle();
                          break;
                        #endif // USES_SOLENOID
                        /*
                        #ifdef USES_SOLENOID
                        case PauseMode_BurstFireToggle:
                          Serial.println("Toggling solenoid burst firing!");
                          BurstFireToggle();
                          break;
                        #endif // USES_SOLENOID
                        */
                        case PauseMode_EscapeSignal:
                          SendExitKey();
                          #ifdef LED_ENABLE
                              for(byte i = 0; i < 3; i++) {
                                  LedUpdate(150,0,150);
                                  delay(55);
                                  LedOff();
                                  delay(40);
                              }
                          #endif // LED_ENABLE
                          break;
                        /*case PauseMode_Exit:
                          Serial.println("Exiting pause mode...");
                          if(runMode == RunMode_Processing) {
                              switch(profileData[selectedProfile].runMode) {
                                  case RunMode_Normal:
                                    SetRunMode(RunMode_Normal);
                                    break;
                                  case RunMode_Average:
                                    SetRunMode(RunMode_Average);
                                    break;
                                  case RunMode_Average2:
                                    SetRunMode(RunMode_Average2);
                                    break;
                                  default:
                                    break;
                              }
                          }
                          SetMode(GunMode_Run);
                          break;
                        */
                        default:
                          Serial.println("Oops, somethnig went wrong.");
                          break;
                    }
                } else if(buttons.pressedReleased & ExitPauseModeBtnMask) {
                    if(!serialMode) {
                        Serial.println("Exiting pause mode...");
                    }
                    SetMode(GunMode_Run);
                }
                if(pauseExitHoldStarted &&
                (buttons.debounced & ExitPauseModeHoldBtnMask)) {
                    unsigned long t = millis();
                    if(t - pauseHoldStartstamp > (pauseHoldLength / 2)) {
                        if(!serialMode) {
                            Serial.println("Exiting pause mode via hold...");
                        }
                        if(runMode == RunMode_Processing) {
                            switch(profileData[selectedProfile].runMode) {
                                case RunMode_Normal:
                                  SetRunMode(RunMode_Normal);
                                  break;
                                case RunMode_Average:
                                  SetRunMode(RunMode_Average);
                                  break;
                                case RunMode_Average2:
                                  SetRunMode(RunMode_Average2);
                                  break;
                                default:
                                  break;
                            }
                        }
                        #ifdef USES_RUMBLE
                            for(byte i = 0; i < 3; i++) {
                                analogWrite(rumblePin, rumbleIntensity);
                                delay(80);
                                digitalWrite(rumblePin, LOW);
                                delay(50);
                            }
                        #endif // USES_RUMBLE
                        while(buttons.debounced != 0) {
                            //lol
                            buttons.Poll(1);
                        }
                        SetMode(GunMode_Run);
                        pauseExitHoldStarted = false;
                    }
                } else if(buttons.debounced & ExitPauseModeHoldBtnMask) {
                    pauseExitHoldStarted = true;
                    pauseHoldStartstamp = millis();
                } else if(buttons.pressedReleased & ExitPauseModeHoldBtnMask) {
                    pauseExitHoldStarted = false;
                }
            } else if(buttons.pressedReleased & ExitPauseModeBtnMask) {
                SetMode(GunMode_Run);
            } else if(buttons.pressedReleased == BtnMask_Trigger) {
                SetMode(GunMode_CalCenter);
            } else if(buttons.pressedReleased == RunModeNormalBtnMask) {
                SetRunMode(RunMode_Normal);
            } else if(buttons.pressedReleased == RunModeAverageBtnMask) {
                SetRunMode(runMode == RunMode_Average ? RunMode_Average2 : RunMode_Average);
            } else if(buttons.pressedReleased == IRSensitivityUpBtnMask) {
                IncreaseIrSensitivity();
            } else if(buttons.pressedReleased == IRSensitivityDownBtnMask) {
                DecreaseIrSensitivity();
            } else if(buttons.pressedReleased == SaveBtnMask) {
                SavePreferences();
            } else if(buttons.pressedReleased == AutofireSpeedToggleBtnMask) {
                AutofireSpeedToggle(0);
            #ifdef USES_RUMBLE
                } else if(buttons.pressedReleased == RumbleToggleBtnMask) {
                    RumbleToggle();
            #endif // USES_RUMBLE
            #ifdef USES_SOLENOID
                } else if(buttons.pressedReleased == SolenoidToggleBtnMask) {
                    SolenoidToggle();
            #endif // USES_SOLENOID
            } else if(buttons.pressedReleased == ExitGameBtnMask) {
                SendExitKey();
            } else if(buttons.pressedReleased == MenuBtnMask) {
                SendMenuKey();
            } else {
                SelectCalProfileFromBtnMask(buttons.pressedReleased);
            }

            if(!serialMode && !dockedCalibrating) {
                PrintResults();
            }
            
            break;
        case GunMode_Docked:
            ExecGunModeDocked();
            break;
        case GunMode_CalCenter:
            AbsMouse5.move(MouseMaxX / 2, MouseMaxY / 2);
            if(buttons.pressedReleased & CancelCalBtnMask && !justBooted) {
                CancelCalibration();
            } else if(buttons.pressed & BtnMask_Trigger) {
                // trigger pressed, begin center cal 
                CalCenter();
                // extra delay to wait for trigger to release (though not required)
                SetModeWaitNoButtons(GunMode_CalVert, 500);
            }
            break;
        case GunMode_CalVert:
            if(buttons.pressedReleased & CancelCalBtnMask && !justBooted) {
                CancelCalibration();
            } else {
                if(buttons.pressed & BtnMask_Trigger) {
                    SetMode(GunMode_CalHoriz);
                } else {
                    CalVert();
                }
            }
            break;
        case GunMode_CalHoriz:
            if(buttons.pressedReleased & CancelCalBtnMask && !justBooted) {
                CancelCalibration();
            } else {
                if(buttons.pressed & BtnMask_Trigger) {
                    ApplyCalToProfile();
                    if(justBooted) {
                        // If this is an initial calibration, save it immediately!
                        SetMode(GunMode_Pause);
                        SavePreferences();
                        SetMode(GunMode_Run);
                    } else if(dockedCalibrating) {
                        Serial.print("UpdatedProf: ");
                        Serial.println(selectedProfile);
                        Serial.println(profileData[selectedProfile].xScale);
                        Serial.println(profileData[selectedProfile].yScale);
                        Serial.println(profileData[selectedProfile].xCenter);
                        Serial.println(profileData[selectedProfile].yCenter);
                        SetMode(GunMode_Docked);
                    } else {
                        SetMode(GunMode_Run);
                    }
                } else {
                    CalHoriz();
                }
            }
            break;
        default:
            /* ---------------------- LET'S GO --------------------------- */
            switch(runMode) {
            case RunMode_Processing:
                //ExecRunModeProcessing();
                //break;
            case RunMode_Average:
            case RunMode_Average2:
            case RunMode_Normal:
            default:
                ExecRunMode();
                break;
            }
            break;
    }

#ifdef DEBUG_SERIAL
    PrintDebugSerial();
#endif // DEBUG_SERIAL

}

/*        -----------------------------------------------        */
/* --------------------------- METHODS ------------------------- */
/*        -----------------------------------------------        */

// Main core loop
void ExecRunMode()
{
#ifdef DEBUG_SERIAL
    Serial.print("exec run mode ");
    Serial.println(RunModeLabels[runMode]);
#endif
    moveIndex = 0;
    buttons.ReportEnable();
    if(justBooted) {
        // center the joystick so RetroArch doesn't throw a hissy fit about uncentered joysticks
        delay(100);  // Exact time needed to wait seems to vary, so make a safe assumption here.
        Gamepad16.releaseAll();
        justBooted = false;
    }
    for(;;) {
        // Setting the state of our toggles, if used.
        // Only sets these values if the switches are mapped to valid pins.
        #ifdef USES_SWITCHES
            #ifdef USES_RUMBLE
                if(rumbleSwitch >= 0) {
                    rumbleActive = !digitalRead(rumbleSwitch);
                }
            #endif // USES_RUMBLE
            #ifdef USES_SOLENOID
                if(solenoidSwitch >= 0) {
                    solenoidActive = !digitalRead(solenoidSwitch);
                }
            #endif // USES_SOLENOID
            if(autofireSwitch >= 0) {
                autofireActive = !digitalRead(autofireSwitch);
            }
        #endif // USES_SWITCHES

        // If we're on RP2040, we offload the button polling to the second core.
        #if !defined(ARDUINO_ARCH_RP2040) || !defined(DUAL_CORE)
        buttons.Poll(0);

        // The main gunMode loop: here it splits off to different paths,
        // depending on if we're in serial handoff (MAMEHOOK) or normal mode.
        #ifdef MAMEHOOKER
            if(Serial.available()) {                             // Have we received serial input? (This is cleared after we've read from it in full.)
                SerialProcessing();                                 // Run through the serial processing method (repeatedly, if there's leftover bits)
            }
            if(!serialMode) {  // Normal (gun-handled) mode
                // For processing the trigger specifically.
                // (buttons.debounced is a binary variable intended to be read 1 bit at a time,
                // with the 0'th point == rightmost == decimal 1 == trigger, 3 = start, 4 = select)
                if(bitRead(buttons.debounced, 0)) {   // Check if we pressed the Trigger this run.
                    TriggerFire();                                  // Handle button events and feedback ourselves.
                } else {   // Or we haven't pressed the trigger.
                    TriggerNotFire();                               // Releasing button inputs and sending stop signals to feedback devices.
                }
            } else {  // Serial handoff mode
                if(bitRead(buttons.debounced, 0)) {   // Check if we pressed the Trigger this run.
                    TriggerFireSimple();                            // Since serial is handling our devices, we're just handling button events.
                } else {   // Or if we haven't pressed the trigger,
                    TriggerNotFireSimple();                         // Release button inputs.
                }
                SerialHandling();                                   // Process the force feedback from the current queue.
            }
        #else
            // For processing the trigger specifically.
            // (buttons.debounced is a binary variable intended to be read 1 bit at a time,
            // with the 0'th point == rightmost == decimal 1 == trigger, 3 = start, 4 = select)
            if(bitRead(buttons.debounced, 0)) {   // Check if we pressed the Trigger this run.
                TriggerFire();                                      // Handle button events and feedback ourselves.
            } else {   // Or we haven't pressed the trigger.
                TriggerNotFire();                                   // Releasing button inputs and sending stop signals to feedback devices.
            }
        #endif // MAMEHOOKER
        #endif // DUAL_CORE

        #ifdef SAMCO_NO_HW_TIMER
            SAMCO_NO_HW_TIMER_UPDATE();
        #endif // SAMCO_NO_HW_TIMER

        if(irPosUpdateTick) {
            irPosUpdateTick = 0;
            GetPosition();
            
            int halfHscale = (int)(mySamco.h() * xScale + 0.5f) / 2;
            moveXAxis = map(finalX, xCenter + halfHscale, xCenter - halfHscale, 0, MouseMaxX);
            halfHscale = (int)(mySamco.h() * yScale + 0.5f) / 2;
            moveYAxis = map(finalY, yCenter + halfHscale, yCenter - halfHscale, 0, MouseMaxY);

            switch(runMode) {
            case RunMode_Average:
                // 2 position moving average
                moveIndex ^= 1;
                moveXAxisArr[moveIndex] = moveXAxis;
                moveYAxisArr[moveIndex] = moveYAxis;
                moveXAxis = (moveXAxisArr[0] + moveXAxisArr[1]) / 2;
                moveYAxis = (moveYAxisArr[0] + moveYAxisArr[1]) / 2;
                break;
            case RunMode_Average2:
                // weighted average of current position and previous 2
                if(moveIndex < 2) {
                    ++moveIndex;
                } else {
                    moveIndex = 0;
                }
                moveXAxisArr[moveIndex] = moveXAxis;
                moveYAxisArr[moveIndex] = moveYAxis;
                moveXAxis = (moveXAxis + moveXAxisArr[0] + moveXAxisArr[1] + moveXAxisArr[1] + 2) / 4;
                moveYAxis = (moveYAxis + moveYAxisArr[0] + moveYAxisArr[1] + moveYAxisArr[1] + 2) / 4;
                break;
            case RunMode_Normal:
            default:
                break;
            }

            conMoveXAxis = constrain(moveXAxis, 0, MouseMaxX);
            if(conMoveXAxis == 0 || conMoveXAxis == MouseMaxX) {
                offXAxis = true;
            } else {
                offXAxis = false;
            }
            conMoveYAxis = constrain(moveYAxis, 0, MouseMaxY);  
            if(conMoveYAxis == 0 || conMoveYAxis == MouseMaxY) {
                offYAxis = true;
            } else {
                offYAxis = false;
            }

            if(buttons.analogOutput) {
                Gamepad16.moveCam(conMoveXAxis, conMoveYAxis);
            } else {
                AbsMouse5.move(conMoveXAxis, conMoveYAxis);
            }

            if(offXAxis || offYAxis) {
                buttons.offScreen = true;
            } else {
                buttons.offScreen = false;
            }

            #ifdef USES_ANALOG
                analogStickPolled = false;
                previousStickPoll = millis();
            #endif // USES_ANALOG
        }

        // If using RP2040, we offload the button processing to the second core.
        #if !defined(ARDUINO_ARCH_RP2040) || !defined(DUAL_CORE)

        #ifdef USES_ANALOG
            if(analogIsValid) {
                // Poll the analog values 2ms after the IR sensor has updated so as not to overload the USB buffer.
                // stickPolled and previousStickPoll are reset/updated after irPosUpdateTick.
                if(!analogStickPolled) {
                    if(buttons.pressed || buttons.released) {
                        previousStickPoll = millis();
                    } else if(millis() - previousStickPoll > analogPollInterval) {
                        AnalogStickPoll();
                        analogStickPolled = true;
                    }
                }
            }
        #endif // USES_ANALOG

        if(holdToPause) {
            if((buttons.debounced == EnterPauseModeHoldBtnMask)
            && !lastSeen && !pauseHoldStarted) {
                pauseHoldStarted = true;
                pauseHoldStartstamp = millis();
                if(!serialMode) {
                    Serial.println("Started holding pause mode signal buttons!");
                }
            } else if(pauseHoldStarted && (buttons.debounced != EnterPauseModeHoldBtnMask || lastSeen)) {
                pauseHoldStarted = false;
                if(!serialMode) {
                    Serial.println("Either stopped holding pause mode buttons, aimed onscreen, or pressed other buttons");
                }
            } else if(pauseHoldStarted) {
                unsigned long t = millis();
                if(t - pauseHoldStartstamp > pauseHoldLength) {
                    // MAKE SURE EVERYTHING IS DISENGAGED:
                    #ifdef USES_SOLENOID
                        digitalWrite(solenoidPin, LOW);
                        solenoidFirstShot = false;
                    #endif // USES_SOLENOID
                    #ifdef USES_RUMBLE
                        digitalWrite(rumblePin, LOW);
                        rumbleHappening = false;
                        rumbleHappened = false;
                    #endif // USES_RUMBLE
                    Keyboard.releaseAll();
                    delay(1);
                    AbsMouse5.release(MOUSE_LEFT);
                    AbsMouse5.release(MOUSE_RIGHT);
                    offscreenBShot = false;
                    buttonPressed = false;
                    triggerHeld = false;
                    burstFiring = false;
                    burstFireCount = 0;
                    SetMode(GunMode_Pause);
                    buttons.ReportDisable();
                    return;
                }
            }
        } else {
            if(buttons.pressedReleased == EnterPauseModeBtnMask) {
                // MAKE SURE EVERYTHING IS DISENGAGED:
                #ifdef USES_SOLENOID
                    digitalWrite(solenoidPin, LOW);
                    solenoidFirstShot = false;
                #endif // USES_SOLENOID
                #ifdef USES_RUMBLE
                    digitalWrite(rumblePin, LOW);
                    rumbleHappening = false;
                    rumbleHappened = false;
                #endif // USES_RUMBLE
                Keyboard.releaseAll();
                delay(1);
                AbsMouse5.release(MOUSE_LEFT);
                AbsMouse5.release(MOUSE_RIGHT);
                offscreenBShot = false;
                buttonPressed = false;
                triggerHeld = false;
                burstFiring = false;
                burstFireCount = 0;
                SetMode(GunMode_Pause);
                buttons.ReportDisable();
                return;
            } else if(buttons.pressedReleased == BtnMask_Home) {
                // MAKE SURE EVERYTHING IS DISENGAGED:
                #ifdef USES_SOLENOID
                    digitalWrite(solenoidPin, LOW);
                    solenoidFirstShot = false;
                #endif // USES_SOLENOID
                #ifdef USES_RUMBLE
                    digitalWrite(rumblePin, LOW);
                    rumbleHappening = false;
                    rumbleHappened = false;
                #endif // USES_RUMBLE
                Keyboard.releaseAll();
                delay(1);
                AbsMouse5.release(MOUSE_LEFT);
                AbsMouse5.release(MOUSE_RIGHT);
                offscreenBShot = false;
                buttonPressed = false;
                triggerHeld = false;
                burstFiring = false;
                burstFireCount = 0;
                SetMode(GunMode_Pause);
                buttons.ReportDisable();
                return;
            }
        }
        #else                                                       // if we're using dual cores,
        if(gunMode != GunMode_Run) {                                // We just check if the gunmode has been changed by the other thread.
            Keyboard.releaseAll();
            delay(1);
            AbsMouse5.release(MOUSE_LEFT);
            AbsMouse5.release(MOUSE_RIGHT);
            return;
        }
        #endif // ARDUINO_ARCH_RP2040 || DUAL_CORE

#ifdef DEBUG_SERIAL
        ++frameCount;
        PrintDebugSerial();
#endif // DEBUG_SERIAL
    }
}

// from Samco_4IR_Test_BETA sketch
// for use with the Samco_4IR_Processing_Sketch_BETA Processing sketch
void ExecRunModeProcessing()
{
    // constant offset added to output values
    const int processingOffset = 100;

    buttons.ReportDisable();
    for(;;) {
        buttons.Poll(1);
        if(Serial.available()) {
            SerialProcessingDocked();
        }
        if(runMode != RunMode_Processing) {
            return;
        }

        #ifdef SAMCO_NO_HW_TIMER
            SAMCO_NO_HW_TIMER_UPDATE();
        #endif // SAMCO_NO_HW_TIMER
        if(irPosUpdateTick) {
            irPosUpdateTick = 0;
        
            int error = dfrIRPos.basicAtomic(DFRobotIRPositionEx::Retry_2);
            if(error == DFRobotIRPositionEx::Error_Success) {
                mySamco.begin(dfrIRPos.xPositions(), dfrIRPos.yPositions(), dfrIRPos.seen(), MouseMaxX / 2, MouseMaxY / 2);
                UpdateLastSeen();
                if(millis() - testLastStamp > testPrintInterval) {
                    testLastStamp = millis();
                    for(int i = 0; i < 4; i++) {
                        Serial.print(map(mySamco.testX(i), 0, MouseMaxX, CamMaxX, 0) + processingOffset);
                        Serial.print(",");
                        Serial.print(map(mySamco.testY(i), 0, MouseMaxY, CamMaxY, 0) + processingOffset);
                        Serial.print(",");
                    }
                    Serial.print(map(mySamco.x(), 0, MouseMaxX, CamMaxX, 0) + processingOffset);
                    Serial.print(",");
                    Serial.print(map(mySamco.y(), 0, MouseMaxY, CamMaxY, 0) + processingOffset);
                    Serial.print(",");
                    Serial.print(map(mySamco.testMedianX(), 0, MouseMaxX, CamMaxX, 0) + processingOffset);
                    Serial.print(",");
                    Serial.println(map(mySamco.testMedianY(), 0, MouseMaxY, CamMaxY, 0) + processingOffset);
                }
            } else if(error == DFRobotIRPositionEx::Error_IICerror) {
                Serial.println("Device not available!");
            }
        }
    }
}

// For use with PIGS-GUI when app connects to this board.
void ExecGunModeDocked()
{
    buttons.ReportDisable();
    if(justBooted) {
        // center the joystick so RetroArch/Windows doesn't throw a hissy fit about uncentered joysticks
        delay(250);  // Exact time needed to wait seems to vary, so make a safe assumption here.
        Gamepad16.releaseAll();
    }
    #ifdef LED_ENABLE
        LedUpdate(127, 127, 255);
    #endif // LED_ENABLE
    unsigned long tempChecked = millis();
    for(;;) {
        buttons.Poll(1);

        if(Serial.available()) {
            SerialProcessingDocked();
        }

        if(dockedCalibrating) {
            #ifdef SAMCO_NO_HW_TIMER
                SAMCO_NO_HW_TIMER_UPDATE();
            #endif // SAMCO_NO_HW_TIMER

            if(irPosUpdateTick) {
                irPosUpdateTick = 0;
                GetPosition();
                
                int halfHscale = (int)(mySamco.h() * xScale + 0.5f) / 2;
                moveXAxis = map(finalX, xCenter + halfHscale, xCenter - halfHscale, 0, MouseMaxX);
                halfHscale = (int)(mySamco.h() * yScale + 0.5f) / 2;
                moveYAxis = map(finalY, yCenter + halfHscale, yCenter - halfHscale, 0, MouseMaxY);

                switch(runMode) {
                case RunMode_Average:
                    // 2 position moving average
                    moveIndex ^= 1;
                    moveXAxisArr[moveIndex] = moveXAxis;
                    moveYAxisArr[moveIndex] = moveYAxis;
                    moveXAxis = (moveXAxisArr[0] + moveXAxisArr[1]) / 2;
                    moveYAxis = (moveYAxisArr[0] + moveYAxisArr[1]) / 2;
                    break;
                case RunMode_Average2:
                    // weighted average of current position and previous 2
                    if(moveIndex < 2) {
                        ++moveIndex;
                    } else {
                        moveIndex = 0;
                    }
                    moveXAxisArr[moveIndex] = moveXAxis;
                    moveYAxisArr[moveIndex] = moveYAxis;
                    moveXAxis = (moveXAxis + moveXAxisArr[0] + moveXAxisArr[1] + moveXAxisArr[1] + 2) / 4;
                    moveYAxis = (moveYAxis + moveYAxisArr[0] + moveYAxisArr[1] + moveYAxisArr[1] + 2) / 4;
                    break;
                case RunMode_Normal:
                default:
                    break;
                }

                conMoveXAxis = constrain(moveXAxis, 0, MouseMaxX);
                conMoveYAxis = constrain(moveYAxis, 0, MouseMaxY);  

                AbsMouse5.move(conMoveXAxis, conMoveYAxis);
            }
            if(buttons.pressed == BtnMask_Trigger) {
                dockedCalibrating = false;
            }
        } else if(!dockedSaving) {
            switch(buttons.pressed) {
                case BtnMask_Trigger:
                  Serial.println("Pressed: 1");
                  break;
                case BtnMask_A:
                  Serial.println("Pressed: 2");
                  break;
                case BtnMask_B:
                  Serial.println("Pressed: 3");
                  break;
                case BtnMask_Calibrate:
                  Serial.println("Pressed: 4");
                  break;
                case BtnMask_Start:
                  Serial.println("Pressed: 5");
                  break;
                case BtnMask_Select:
                  Serial.println("Pressed: 6");
                  break;
                case BtnMask_Up:
                  Serial.println("Pressed: 7");
                  break;
                case BtnMask_Down:
                  Serial.println("Pressed: 8");
                  break;
                case BtnMask_Left:
                  Serial.println("Pressed: 9");
                  break;
                case BtnMask_Right:
                  Serial.println("Pressed: 10");
                  break;
                case BtnMask_Pedal:
                  Serial.println("Pressed: 11");
                  break;
                case BtnMask_Home:
                  Serial.println("Pressed: 12");
                  break;
                case BtnMask_Pump:
                  Serial.println("Pressed: 13");
                  break;
            }

            switch(buttons.released) {
                case BtnMask_Trigger:
                  Serial.println("Released: 1");
                  break;
                case BtnMask_A:
                  Serial.println("Released: 2");
                  break;
                case BtnMask_B:
                  Serial.println("Released: 3");
                  break;
                case BtnMask_Calibrate:
                  Serial.println("Released: 4");
                  break;
                case BtnMask_Start:
                  Serial.println("Released: 5");
                  break;
                case BtnMask_Select:
                  Serial.println("Released: 6");
                  break;
                case BtnMask_Up:
                  Serial.println("Released: 7");
                  break;
                case BtnMask_Down:
                  Serial.println("Released: 8");
                  break;
                case BtnMask_Left:
                  Serial.println("Released: 9");
                  break;
                case BtnMask_Right:
                  Serial.println("Released: 10");
                  break;
                case BtnMask_Pedal:
                  Serial.println("Released: 11");
                  break;
                case BtnMask_Home:
                  Serial.println("Released: 12");
                  break;
                case BtnMask_Pump:
                  Serial.println("Released: 13");
                  break;
            }
            unsigned long currentMillis = millis();
            if(currentMillis - tempChecked >= 1000) {
                if(tempPin >= 0) {
                    int tempSensor = analogRead(tempPin);
                    tempSensor = (((tempSensor * 3.3) / 4096) - 0.5) * 100;
                    Serial.print("Temperature: ");
                    Serial.println(tempSensor);
                }
                tempChecked = currentMillis;
            }
        }

        if(gunMode != GunMode_Docked) {
            return;
        }
        if(runMode == RunMode_Processing) {
            ExecRunModeProcessing();
        }
    }
}

// center calibration with a bit of averaging
void CalCenter()
{
    unsigned int xAcc = 0;
    unsigned int yAcc = 0;
    unsigned int count = 0;
    unsigned long ms = millis();
    
    // accumulate center position over a bit of time for some averaging
    while(millis() - ms < 333) {
        // center pointer
        AbsMouse5.move(MouseMaxX / 2, MouseMaxY / 2);
        
        // get position
        if(GetPositionIfReady()) {
            xAcc += finalX;
            yAcc += finalY;
            count++;
            
            xCenter = finalX;
            yCenter = finalY;
            PrintCalInterval();
        }

        // poll buttons
        buttons.Poll(1);
        
        // if trigger not pressed then break out of loop early
        if(!(buttons.debounced & BtnMask_Trigger)) {
            break;
        }
    }

    // unexpected, but make sure x and y positions are accumulated
    if(count) {
        xCenter = xAcc / count;
        yCenter = yAcc / count;
    } else {
        Serial.print("Unexpected Center calibration failure, no center position was acquired!");
        // just continue anyway
    }

    PrintCalInterval();
}

// vertical calibration 
void CalVert()
{
    if(GetPositionIfReady()) {
        int halfH = (int)(mySamco.h() * yScale + 0.5f) / 2;
        moveYAxis = map(finalY, yCenter + halfH, yCenter - halfH, 0, MouseMaxY);
        conMoveXAxis = MouseMaxX / 2;
        conMoveYAxis = constrain(moveYAxis, 0, MouseMaxY);
        AbsMouse5.move(conMoveXAxis, conMoveYAxis);
        if(conMoveYAxis == 0 || conMoveYAxis == MouseMaxY) {
            buttons.offScreen = true;
        } else {
            buttons.offScreen = false;
        }
    }
    
    if((buttons.repeat & BtnMask_B) || (buttons.offScreen && (buttons.repeat & BtnMask_A))) {
        yScale = yScale + ScaleStep;
    } else if(buttons.repeat & BtnMask_A) {
        if(yScale > 0.005f) {
            yScale = yScale - ScaleStep;
        }
    }

    if(buttons.pressedReleased == BtnMask_Up) {
        yCenter--;
    } else if(buttons.pressedReleased == BtnMask_Down) {
        yCenter++;
    }
    
    PrintCalInterval();
}

// horizontal calibration 
void CalHoriz()
{
    if(GetPositionIfReady()) {    
        int halfH = (int)(mySamco.h() * xScale + 0.5f) / 2;
        moveXAxis = map(finalX, xCenter + halfH, xCenter - halfH, 0, MouseMaxX);
        conMoveXAxis = constrain(moveXAxis, 0, MouseMaxX);
        conMoveYAxis = MouseMaxY / 2;
        AbsMouse5.move(conMoveXAxis, conMoveYAxis);
        if(conMoveXAxis == 0 || conMoveXAxis == MouseMaxX) {
            buttons.offScreen = true;
        } else {
            buttons.offScreen = false;
        }
    }

    if((buttons.repeat & BtnMask_B) || (buttons.offScreen && (buttons.repeat & BtnMask_A))) {
        xScale = xScale + ScaleStep;
    } else if(buttons.repeat & BtnMask_A) {
        if(xScale > 0.005f) {
            xScale = xScale - ScaleStep;
        }
    }
    
    if(buttons.pressedReleased == BtnMask_Left) {
        xCenter--;
    } else if(buttons.pressedReleased == BtnMask_Right) {
        xCenter++;
    }

    PrintCalInterval();
}

// Helper to get position if the update tick is set
bool GetPositionIfReady()
{
    if(irPosUpdateTick) {
        irPosUpdateTick = 0;
        GetPosition();
        return true;
    }
    return false;
}

// Get tilt adjusted position from IR postioning camera
// Updates finalX and finalY values
void GetPosition()
{
    int error = dfrIRPos.basicAtomic(DFRobotIRPositionEx::Retry_2);
    if(error == DFRobotIRPositionEx::Error_Success) {
        mySamco.begin(dfrIRPos.xPositions(), dfrIRPos.yPositions(), dfrIRPos.seen(), xCenter, yCenter);
#ifdef EXTRA_POS_GLITCH_FILTER
        if((abs(mySamco.X() - finalX) > BadMoveThreshold || abs(mySamco.Y() - finalY) > BadMoveThreshold) && badFinalTick < BadMoveCountThreshold) {
            ++badFinalTick;
        } else {
            if(badFinalTick) {
                badFinalCount++;
                badFinalTick = 0;
            }
            finalX = mySamco.X();
            finalY = mySamco.Y();
        }
#else
        finalX = mySamco.x();
        finalY = mySamco.y();
#endif // EXTRA_POS_GLITCH_FILTER

        UpdateLastSeen();
     
#if DEBUG_SERIAL == 2
        Serial.print(finalX);
        Serial.print(' ');
        Serial.print(finalY);
        Serial.print("   ");
        Serial.println(mySamco.h());
#endif
    } else if(error != DFRobotIRPositionEx::Error_DataMismatch) {
        Serial.println("Device not available!");
    }
}

// wait up to given amount of time for no buttons to be pressed before setting the mode
void SetModeWaitNoButtons(GunMode_e newMode, unsigned long maxWait)
{
    unsigned long ms = millis();
    while(buttons.debounced && (millis() - ms < maxWait)) {
        buttons.Poll(1);
    }
    SetMode(newMode);
}

// update the last seen value
// only to be called during run mode since this will modify the LED colour
void UpdateLastSeen()
{
    if(lastSeen != mySamco.seen()) {
        #ifdef MAMEHOOKER
        if(!serialMode) {
        #endif // MAMEHOOKER
            #ifdef LED_ENABLE
            if(!lastSeen && mySamco.seen()) {
                LedOff();
            } else if(lastSeen && !mySamco.seen()) {
                SetLedPackedColor(IRSeen0Color);
            }
            #endif // LED_ENABLE
        #ifdef MAMEHOOKER
        }
        #endif // MAMEHOOKER
        lastSeen = mySamco.seen();
    }
}

// Handles events when trigger is pulled/held
void TriggerFire()
{
    if(!buttons.offScreen &&                                     // Check if the X or Y axis is in the screen's boundaries, i.e. "off screen".
    !offscreenBShot) {                                           // And only as long as we haven't fired an off-screen shot,
        if(!buttonPressed) {
            if(buttons.analogOutput) {
                Gamepad16.press(LightgunButtons::ButtonDesc[BtnIdx_Trigger].reportCode3); // No reason to handle this ourselves here, but eh.
            } else {
                AbsMouse5.press(MOUSE_LEFT);                     // We're handling the trigger button press ourselves for a reason.
            }
            buttonPressed = true;                                // Set this so we won't spam a repeat press event again.
        }
        if(!bitRead(buttons.debounced, 3) &&                     // Is the trigger being pulled WITHOUT pressing Start & Select?
        !bitRead(buttons.debounced, 4)) {
            #ifdef USES_SOLENOID
                if(solenoidActive) {                             // (Only activate when the solenoid switch is on!)
                    if(!triggerHeld) {  // If this is the first time we're firing,
                        if(burstFireActive && !burstFiring) {  // Are we in burst firing mode?
                            solenoidFirstShot = true;               // Set this so we use the instant solenoid fire path,
                            SolenoidActivation(0);                  // Engage it,
                            solenoidFirstShot = false;              // And disable the flag to mitigate confusion.
                            burstFiring = true;                     // Set that we're in a burst fire event.
                            burstFireCount = 1;                     // Set this as the first shot in a burst fire sequence,
                            burstFireCountLast = 1;                 // And reset the stored counter,
                        } else if(!burstFireActive) {  // Or, if we're in normal or rapid fire mode,
                            solenoidFirstShot = true;               // Set the First Shot flag on.
                            SolenoidActivation(0);                  // Just activate the Solenoid already!
                            if(autofireActive) {          // If we are in auto mode,
                                solenoidFirstShot = false;          // Immediately set this bit off!
                            }
                        }
                    // Else, these below are all if we've been holding the trigger.
                    } else if(burstFiring) {  // If we're in a burst firing sequence,
                        BurstFire();                                // Process it.
                    } else if(autofireActive &&  // Else, if we've been holding the trigger, is the autofire switch active?
                    !burstFireActive) {          // (WITHOUT burst firing enabled)
                        if(digitalRead(solenoidPin)) {              // Is the solenoid engaged?
                            SolenoidActivation(solenoidFastInterval); // If so, immediately pass the autofire faster interval to solenoid method
                        } else {                                    // Or if it's not,
                            SolenoidActivation(solenoidFastInterval * autofireWaitFactor); // We're holding it for longer.
                        }
                    } else if(solenoidFirstShot) {                  // If we aren't in autofire mode, are we waiting for the initial shot timer still?
                        if(digitalRead(solenoidPin)) {              // If so, are we still engaged? We need to let it go normally, but maintain the single shot flag.
                            unsigned long currentMillis = millis(); // Initialize timer to check if we've passed it.
                            if(currentMillis - previousMillisSol >= solenoidNormalInterval) { // If we finally surpassed the wait threshold...
                                digitalWrite(solenoidPin, LOW);     // Let it go.
                            }
                        } else {                                    // We're waiting on the extended wait before repeating in single shot mode.
                            unsigned long currentMillis = millis(); // Initialize timer.
                            if(currentMillis - previousMillisSol >= solenoidLongInterval) { // If we finally surpassed the LONGER wait threshold...
                                solenoidFirstShot = false;          // We're gonna turn this off so we don't have to pass through this check anymore.
                                SolenoidActivation(solenoidNormalInterval); // Process it now.
                            }
                        }
                    } else if(!burstFireActive) {                   // if we don't have the single shot wait flag on (holding the trigger w/out autofire)
                        if(digitalRead(solenoidPin)) {              // Are we engaged right now?
                            SolenoidActivation(solenoidNormalInterval); // Turn it off with this timer.
                        } else {                                    // Or we're not engaged.
                            SolenoidActivation(solenoidNormalInterval * 2); // So hold it that way for twice the normal timer.
                        }
                    }
                } // We ain't using the solenoid, so just ignore all that.
                #elif RUMBLE_FF
                    if(rumbleActive) {
                        // TODO: actually make stuff here.
                    } // We ain't using the motor, so just ignore all that.
                #endif // USES_SOLENOID/RUMBLE_FF
            }
            #ifdef USES_RUMBLE
                #ifndef RUMBLE_FF
                    if(rumbleActive &&                     // Is rumble activated,
                        rumbleHappening && triggerHeld) {  // AND we're in a rumbling command WHILE the trigger's held?
                        RumbleActivation();                    // Continue processing the rumble command, to prevent infinite rumble while going from on-screen to off mid-command.
                    }
                #endif // RUMBLE_FF
            #endif // USES_RUMBLE
        } else {  // We're shooting outside of the screen boundaries!
            #ifdef PRINT_VERBOSE
                Serial.println("Shooting outside of the screen! RELOAD!");
            #endif
            if(!buttonPressed) {  // If we haven't pressed a trigger key yet,
                if(!triggerHeld && offscreenButton) {  // If we are in offscreen button mode (and aren't dragging a shot offscreen)
                    if(buttons.analogOutput) {
                        Gamepad16.press(LightgunButtons::ButtonDesc[BtnIdx_A].reportCode3);
                    } else {
                        AbsMouse5.press(MOUSE_RIGHT);
                    }
                    offscreenBShot = true;                     // Mark we pressed the right button via offscreen shot mode,
                    buttonPressed = true;                      // Mark so we're not spamming these press events.
                } else {  // Or if we're not in offscreen button mode,
                    if(buttons.analogOutput) {
                        Gamepad16.press(LightgunButtons::ButtonDesc[BtnIdx_Trigger].reportCode3);
                    } else {
                        AbsMouse5.press(MOUSE_LEFT);
                    }
                    buttonPressed = true;                      // Mark so we're not spamming these press events.
                }
            }
            #ifdef USES_RUMBLE
                #ifndef RUMBLE_FF
                    if(rumbleActive) {  // Only activate if the rumble switch is enabled!
                        if(!rumbleHappened && !triggerHeld) {  // Is this the first time we're rumbling AND only started pulling the trigger (to prevent starting a rumble w/ trigger hold)?
                            RumbleActivation();                        // Start a rumble command.
                        } else if(rumbleHappening) {  // We are currently processing a rumble command.
                            RumbleActivation();                        // Keep processing that command then.
                        }  // Else, we rumbled already, so don't do anything to prevent infinite rumbling.
                    }
                #endif // RUMBLE_FF
            #endif // USES_RUMBLE
            if(burstFiring) {                                  // If we're in a burst firing sequence,
                BurstFire();
            #ifdef USES_SOLENOID
                } else if(digitalRead(solenoidPin) &&
                !burstFireActive) {                               // If the solenoid is engaged, since we're not shooting the screen, shut off the solenoid a'la an idle cycle
                    unsigned long currentMillis = millis();         // Calibrate current time
                    if(currentMillis - previousMillisSol >= solenoidFastInterval) { // I guess if we're not firing, may as well use the fastest shutoff.
                        previousMillisSol = currentMillis;          // Timer calibration, yawn.
                        digitalWrite(solenoidPin, LOW);             // Turn it off already, dangit.
                    }
            #elif RUMBLE_FF
                } else if(rumbleHappening && !burstFireActive) {
                    // TODO: actually implement
            #endif // USES_SOLENOID
        }
    }
    triggerHeld = true;                                     // Signal that we've started pulling the trigger this poll cycle.
}

// Handles events when trigger is released
void TriggerNotFire()
{
    triggerHeld = false;                                    // Disable the holding function
    if(buttonPressed) {
        if(offscreenBShot) {                                // If we fired off screen with the offscreenButton set,
            if(buttons.analogOutput) {
                Gamepad16.release(LightgunButtons::ButtonDesc[BtnIdx_A].reportCode3);
            } else {
                AbsMouse5.release(MOUSE_RIGHT);             // We were pressing the right mouse, so release that.
            }
            offscreenBShot = false;
            buttonPressed = false;
        } else {                                            // Or if not,
            if(buttons.analogOutput) {
                Gamepad16.release(LightgunButtons::ButtonDesc[BtnIdx_Trigger].reportCode3);
            } else {
                AbsMouse5.release(MOUSE_LEFT);              // We were pressing the left mouse, so release that instead.
            }
            buttonPressed = false;
        }
    }
    #ifdef USES_SOLENOID
        if(solenoidActive) {  // Has the solenoid remain engaged this cycle?
            if(burstFiring) {    // Are we in a burst fire command?
                BurstFire();                                    // Continue processing it.
            } else if(!burstFireActive) { // Else, we're just processing a normal/rapid fire shot.
                solenoidFirstShot = false;                      // Make sure this is unset to prevent "sticking" in single shot mode!
                unsigned long currentMillis = millis();         // Start the timer
                if(currentMillis - previousMillisSol >= solenoidFastInterval) { // I guess if we're not firing, may as well use the fastest shutoff.
                    previousMillisSol = currentMillis;          // Timer calibration, yawn.
                    digitalWrite(solenoidPin, LOW);             // Make sure to turn it off.
                }
            }
        }
    #endif // USES_SOLENOID
    #ifdef USES_RUMBLE
        if(rumbleHappening) {                                   // Are we currently in a rumble command? (Implicitly needs rumbleActive)
            RumbleActivation();                                 // Continue processing our rumble command.
            // (This is to prevent making the lack of trigger pull actually activate a rumble command instead of skipping it like we should.)
        } else if(rumbleHappened) {                             // If rumble has happened,
            rumbleHappened = false;                             // well we're clear now that we've stopped holding.
        }
    #endif // USES_RUMBLE
}

#ifdef USES_ANALOG
void AnalogStickPoll()
{
    unsigned int analogValueX = analogRead(analogPinX);
    unsigned int analogValueY = analogRead(analogPinY);
    Gamepad16.moveStick(analogValueX, analogValueY);
}
#endif // USES_ANALOG

// Limited subset of SerialProcessing specifically for "docked" mode
// contains setting submethods for use by PIGS-GUI
void SerialProcessingDocked()
{
    char serialInput = Serial.read();
    char serialInputS[3] = {0, 0, 0};

    switch(serialInput) {
        case 'X':
          serialInput = Serial.read();
          switch(serialInput) {
              // Set IR Brightness
              case 'B':
                serialInput = Serial.read();
                if(serialInput == '0' || serialInput == '1' || serialInput == '2') {
                    if(gunMode != GunMode_Pause || gunMode != GunMode_Docked) {
                        Serial.println("Can't set sensitivity in run mode! Please enter pause mode if you'd like to change IR sensitivity.");
                    } else {
                        byte brightnessLvl = serialInput - '0';
                        SetIrSensitivity(brightnessLvl);
                    }
                } else {
                    Serial.println("SERIALREAD: No valid IR sensitivity level set! (Expected 0 to 2)");
                }
                break;
              // Toggle Test/Processing Mode
              case 'T':
                if(runMode == RunMode_Processing) {
                    Serial.println("Exiting processing mode...");
                    switch(profileData[selectedProfile].runMode) {
                        case RunMode_Normal:
                          SetRunMode(RunMode_Normal);
                          break;
                        case RunMode_Average:
                          SetRunMode(RunMode_Average);
                          break;
                        case RunMode_Average2:
                          SetRunMode(RunMode_Average2);
                          break;
                    }
                } else {
                    Serial.println("Entering Test Mode...");
                    SetRunMode(RunMode_Processing);
                }
                break;
              // Enter Docked Mode
              case 'P':
                SetMode(GunMode_Docked);
                break;
              // Exit Docked Mode
              case 'E':
                if(!justBooted) {
                    SetMode(GunMode_Run);
                } else {
                    SetMode(GunMode_Init);
                }
                switch(profileData[selectedProfile].runMode) {
                    case RunMode_Normal:
                      SetRunMode(RunMode_Normal);
                      break;
                    case RunMode_Average:
                      SetRunMode(RunMode_Average);
                      break;
                    case RunMode_Average2:
                      SetRunMode(RunMode_Average2);
                      break;
                }
                break;
              // Enter Calibration mode (optional: switch to cal profile if detected)
              case 'C':
                serialInput = Serial.read();
                if(serialInput == '1' || serialInput == '2' ||
                   serialInput == '3' || serialInput == '4') {
                    // Converting char to its real respective number
                    byte profileNum = serialInput - '0';
                    SelectCalProfile(profileNum-1);
                    Serial.print("Profile: ");
                    Serial.println(profileNum-1);
                    serialInput = Serial.read();
                    if(serialInput == 'C') {
                        dockedCalibrating = true;
                        //Serial.print("Now calibrating selected profile: ");
                        //Serial.println(profileDesc[selectedProfile].profileLabel);
                        SetMode(GunMode_CalCenter);
                    }
                }
                break;
              // Save current profile
              case 'S':
                Serial.println("Saving preferences...");
                // Update bindings so LED/Pixel changes are reflected immediately
                FeedbackSet();
                // dockedSaving flag is set by Xm, since that's required anyways for this to make any sense.
                SavePreferences();
                // load everything back to commit custom pins setting to memory
                if(nvPrefsError == SamcoPreferences::Error_Success) {
                    ExtPreferences(true);
                    #ifdef LED_ENABLE
                    // Save op above resets color, so re-set it back to docked idle color
                    LedUpdate(127, 127, 255);
                    #endif // LED_ENABLE
                }
                buttons.Begin();
                UpdateBindings(lowButtonMode);
                dockedSaving = false;
                break;
              // Clear EEPROM.
              case 'c':
                //Serial.println(EEPROM.length());
                dockedSaving = true;
                SamcoPreferences::ResetPreferences();
                Serial.println("Cleared! Please reset the board.");
                dockedSaving = false;
                break;
              // Mapping new values to commit to EEPROM.
              case 'm':
              {
                if(!dockedSaving) {
                    buttons.Unset();
                    PinsReset();
                    dockedSaving = true; // mark so button presses won't interrupt this process.
                } else {
                    serialInput = Serial.read(); // nomf
                    serialInput = Serial.read();
                    // bool change
                    if(serialInput == '0') {
                        serialInput = Serial.read(); // nomf
                        serialInput = Serial.read();
                        switch(serialInput) {
                          #ifdef USES_RUMBLE
                          case '0':
                            serialInput = Serial.read(); // nomf
                            serialInput = Serial.read();
                            rumbleActive = serialInput - '0';
                            rumbleActive = constrain(rumbleActive, 0, 1);
                            Serial.println("OK: Toggled Rumble setting.");
                            break;
                          #endif
                          #ifdef USES_SOLENOID
                          case '1':
                            serialInput = Serial.read(); // nomf
                            serialInput = Serial.read();
                            solenoidActive = serialInput - '0';
                            solenoidActive = constrain(solenoidActive, 0, 1);
                            Serial.println("OK: Toggled Solenoid setting.");
                            break;
                          #endif
                          case '2':
                            serialInput = Serial.read(); // nomf
                            serialInput = Serial.read();
                            autofireActive = serialInput - '0';
                            autofireActive = constrain(autofireActive, 0, 1);
                            Serial.println("OK: Toggled Autofire setting.");
                            break;
                          case '3':
                            serialInput = Serial.read(); // nomf
                            serialInput = Serial.read();
                            simpleMenu = serialInput - '0';
                            simpleMenu = constrain(simpleMenu, 0, 1);
                            Serial.println("OK: Toggled Simple Pause Menu setting.");
                            break;
                          case '4':
                            serialInput = Serial.read(); // nomf
                            serialInput = Serial.read();
                            holdToPause = serialInput - '0';
                            holdToPause = constrain(holdToPause, 0, 1);
                            Serial.println("OK: Toggled Hold to Pause setting.");
                            break;
                          #ifdef FOURPIN_LED
                          case '5':
                            serialInput = Serial.read(); // nomf
                            serialInput = Serial.read();
                            commonAnode = serialInput - '0';
                            commonAnode = constrain(commonAnode, 0, 1);
                            Serial.println("OK: Toggled Common Anode setting.");
                            break;
                          #endif
                          case '6':
                            serialInput = Serial.read(); // nomf
                            serialInput = Serial.read();
                            lowButtonMode = serialInput - '0';
                            lowButtonMode = constrain(lowButtonMode, 0, 1);
                            Serial.println("OK: Toggled Low Button Mode setting.");
                            break;
                          default:
                            while(!Serial.available()) {
                              serialInput = Serial.read(); // nomf it all
                            }
                            Serial.println("NOENT: No matching case (feature disabled).");
                            break;
                        }
                    // Pins
                    } else if(serialInput == '1') {
                        serialInput = Serial.read(); // nomf
                        byte sCase = Serial.parseInt();
                        switch(sCase) {
                          case 0:
                            serialInput = Serial.read(); // nomf
                            serialInput = Serial.read();
                            customPinsInUse = serialInput - '0';
                            customPinsInUse = constrain(customPinsInUse, 0, 1);
                            Serial.println("OK: Toggled Custom Pin setting.");
                            break;
                          case 1:
                            serialInput = Serial.read(); // nomf
                            btnTrigger = Serial.parseInt();
                            btnTrigger = constrain(btnTrigger, -1, 40);
                            Serial.println("OK: Set trigger button pin.");
                            break;
                          case 2:
                            serialInput = Serial.read(); // nomf
                            btnGunA = Serial.parseInt();
                            btnGunA = constrain(btnGunA, -1, 40);
                            Serial.println("OK: Set A button pin.");
                            break;
                          case 3:
                            serialInput = Serial.read(); // nomf
                            btnGunB = Serial.parseInt();
                            btnGunB = constrain(btnGunB, -1, 40);
                            Serial.println("OK: Set B button pin.");
                            break;
                          case 4:
                            serialInput = Serial.read(); // nomf
                            btnCalibrate = Serial.parseInt();
                            btnCalibrate = constrain(btnCalibrate, -1, 40);
                            Serial.println("OK: Set C button pin.");
                            break;
                          case 5:
                            serialInput = Serial.read(); // nomf
                            btnStart = Serial.parseInt();
                            btnStart = constrain(btnStart, -1, 40);
                            Serial.println("OK: Set Start button pin.");
                            break;
                          case 6:
                            serialInput = Serial.read(); // nomf
                            btnSelect = Serial.parseInt();
                            btnSelect = constrain(btnSelect, -1, 40);
                            Serial.println("OK: Set Select button pin.");
                            break;
                          case 7:
                            serialInput = Serial.read(); // nomf
                            btnGunUp = Serial.parseInt();
                            btnGunUp = constrain(btnGunUp, -1, 40);
                            Serial.println("OK: Set D-Pad Up button pin.");
                            break;
                          case 8:
                            serialInput = Serial.read(); // nomf
                            btnGunDown = Serial.parseInt();
                            btnGunDown = constrain(btnGunDown, -1, 40);
                            Serial.println("OK: Set D-Pad Down button pin.");
                            break;
                          case 9:
                            serialInput = Serial.read(); // nomf
                            btnGunLeft = Serial.parseInt();
                            btnGunLeft = constrain(btnGunLeft, -1, 40);
                            Serial.println("OK: Set D-Pad Left button pin.");
                            break;
                          case 10:
                            serialInput = Serial.read(); // nomf
                            btnGunRight = Serial.parseInt();
                            btnGunRight = constrain(btnGunRight, -1, 40);
                            Serial.println("OK: Set D-Pad Right button pin.");
                            break;
                          case 11:
                            serialInput = Serial.read(); // nomf
                            btnPedal = Serial.parseInt();
                            btnPedal = constrain(btnPedal, -1, 40);
                            Serial.println("OK: Set External Pedal button pin.");
                            break;
                          case 12:
                            serialInput = Serial.read(); // nomf
                            btnHome = Serial.parseInt();
                            btnHome = constrain(btnHome, -1, 40);
                            Serial.println("OK: Set Home button pin.");
                            break;
                          case 13:
                            serialInput = Serial.read(); // nomf
                            btnPump = Serial.parseInt();
                            btnPump = constrain(btnPump, -1, 40);
                            Serial.println("OK: Set Pump Action button pin.");
                            break;
                          #ifdef USES_RUMBLE
                          case 14:
                            serialInput = Serial.read(); // nomf
                            rumblePin = Serial.parseInt();
                            rumblePin = constrain(rumblePin, -1, 40);
                            Serial.println("OK: Set Rumble signal pin.");
                            break;
                          #endif
                          #ifdef USES_SOLENOID
                          case 15:
                            serialInput = Serial.read(); // nomf
                            solenoidPin = Serial.parseInt();
                            solenoidPin = constrain(solenoidPin, -1, 40);
                            Serial.println("OK: Set Solenoid signal pin.");
                            break;
                          #ifdef USES_TEMP
                          case 16:
                            serialInput = Serial.read(); // nomf
                            tempPin = Serial.parseInt();
                            tempPin = constrain(tempPin, -1, 40);
                            Serial.println("OK: Set Temperature Sensor pin.");
                            break;
                          #endif
                          #endif
                          #ifdef USES_SWITCHES
                          #ifdef USES_RUMBLE
                          case 17:
                            serialInput = Serial.read(); // nomf
                            rumbleSwitch = Serial.parseInt();
                            rumbleSwitch = constrain(rumbleSwitch, -1, 40);
                            Serial.println("OK: Set Rumble Switch pin.");
                            break;
                          #endif
                          #ifdef USES_SOLENOID
                          case 18:
                            serialInput = Serial.read(); // nomf
                            solenoidSwitch = Serial.parseInt();
                            solenoidSwitch = constrain(solenoidSwitch, -1, 40);
                            Serial.println("OK: Set Solenoid Switch pin.");
                            break;
                          #endif
                          case 19:
                            serialInput = Serial.read(); // nomf
                            autofireSwitch = Serial.parseInt();
                            autofireSwitch = constrain(autofireSwitch, -1, 40);
                            Serial.println("OK: Set Autofire Switch pin.");
                            break;
                          #endif
                          #ifdef FOURPIN_LED
                          case 20:
                            serialInput = Serial.read(); // nomf
                            PinR = Serial.parseInt();
                            PinR = constrain(PinR, -1, 40);
                            Serial.println("OK: Set RGB LED R pin.");
                            break;
                          case 21:
                            serialInput = Serial.read(); // nomf
                            PinG = Serial.parseInt();
                            PinG = constrain(PinG, -1, 40);
                            Serial.println("OK: Set RGB LED G pin.");
                            break;
                          case 22:
                            serialInput = Serial.read(); // nomf
                            PinB = Serial.parseInt();
                            PinB = constrain(PinB, -1, 40);
                            Serial.println("OK: Set RGB LED B pin.");
                            break;
                          #endif
                          #ifdef CUSTOM_NEOPIXEL
                          case 23:
                            serialInput = Serial.read(); // nomf
                            customLEDpin = Serial.parseInt();
                            customLEDpin = constrain(customLEDpin, -1, 40);
                            Serial.println("OK: Set Custom NeoPixel pin.");
                            break;
                          #endif
                          #ifdef USES_ANALOG
                          case 24:
                            serialInput = Serial.read(); // nomf
                            analogPinX = Serial.parseInt();
                            analogPinX = constrain(analogPinX, -1, 40);
                            Serial.println("OK: Set Analog X pin.");
                            break;
                          case 25:
                            serialInput = Serial.read(); // nomf
                            analogPinY = Serial.parseInt();
                            analogPinY = constrain(analogPinY, -1, 40);
                            Serial.println("OK: Set Analog Y pin.");
                            break;
                          #endif
                          default:
                            while(!Serial.available()) {
                              serialInput = Serial.read(); // nomf it all
                            }
                            Serial.println("NOENT: No matching case (feature disabled).");
                            break;
                        }
                    // Extended Settings
                    } else if(serialInput == '2') {
                        serialInput = Serial.read(); // nomf
                        serialInput = Serial.read();
                        switch(serialInput) {
                          #ifdef USES_RUMBLE
                          case '0':
                            serialInput = Serial.read(); // nomf
                            rumbleIntensity = Serial.parseInt();
                            rumbleIntensity = constrain(rumbleIntensity, 0, 255);
                            Serial.println("OK: Set Rumble Intensity setting.");
                            break;
                          case '1':
                            serialInput = Serial.read(); // nomf
                            rumbleInterval = Serial.parseInt();
                            Serial.println("OK: Set Rumble Length setting.");
                            break;
                          #endif
                          #ifdef USES_SOLENOID
                          case '2':
                            serialInput = Serial.read(); // nomf
                            solenoidNormalInterval = Serial.parseInt();
                            Serial.println("OK: Set Solenoid Normal Interval setting.");
                            break;
                          case '3':
                            serialInput = Serial.read(); // nomf
                            solenoidFastInterval = Serial.parseInt();
                            Serial.println("OK: Set Solenoid Fast Interval setting.");
                            break;
                          case '4':
                            serialInput = Serial.read(); // nomf
                            solenoidLongInterval = Serial.parseInt();
                            Serial.println("OK: Set Solenoid Hold Length setting.");
                            break;
                          #endif
                          #ifdef CUSTOM_NEOPIXEL
                          case '5':
                            serialInput = Serial.read(); // nomf
                            customLEDcount = Serial.parseInt();
                            customLEDcount = constrain(customLEDcount, 1, 255);
                            Serial.println("OK: Set NeoPixel strand length setting.");
                            break;
                          #endif
                          case '6':
                            serialInput = Serial.read(); // nomf
                            autofireWaitFactor = Serial.parseInt();
                            autofireWaitFactor = constrain(autofireWaitFactor, 2, 4);
                            Serial.println("OK: Set Autofire Wait Factor setting.");
                            break;
                          case '7':
                            serialInput = Serial.read(); // nomf
                            pauseHoldLength = Serial.parseInt();
                            Serial.println("OK: Set Hold to Pause length setting.");
                            break;
                          default:
                            while(!Serial.available()) {
                              serialInput = Serial.read(); // nomf it all
                            }
                            Serial.println("NOENT: No matching case (feature disabled).");
                            break;
                        }
                    #ifdef USE_TINYUSB
                    // TinyUSB Identifier Settings
                    } else if(serialInput == '3') {
                        serialInput = Serial.read(); // nomf
                        serialInput = Serial.read();
                        switch(serialInput) {
                          // Device PID
                          case '0':
                            {
                              serialInput = Serial.read(); // nomf
                              devicePID = Serial.parseInt();
                              Serial.println("OK: Updated TinyUSB Device ID.");
                              EEPROM.put(EEPROM.length() - 22, devicePID);
                              #ifdef ARDUINO_ARCH_RP2040
                                  EEPROM.commit();
                              #endif // ARDUINO_ARCH_RP2040
                              break;
                            }
                          // Device name
                          case '1':
                            serialInput = Serial.read(); // nomf
                            for(byte i = 0; i < sizeof(deviceName); i++) {
                                deviceName[i] = '\0';
                            }
                            for(byte i = 0; i < 15; i++) {
                                deviceName[i] = Serial.read();
                                if(!Serial.available()) {
                                    break;
                                }
                            }
                            Serial.println("OK: Updated TinyUSB Device String.");
                            for(byte i = 0; i < 16; i++) {
                                EEPROM.update(EEPROM.length() - 18 + i, deviceName[i]);
                            }
                            #ifdef ARDUINO_ARCH_RP2040
                                EEPROM.commit();
                            #endif // ARDUINO_ARCH_RP2040
                            break;
                        }
                    #endif // USE_TINYUSB
                    // Profile settings
                    } else if(serialInput == 'P') {
                        serialInput = Serial.read(); // nomf
                        serialInput = Serial.read();
                        switch(serialInput) {
                          case 'i':
                          {
                            serialInput = Serial.read(); // nomf
                            serialInput = Serial.read();
                            uint8_t i = serialInput - '0';
                            i = constrain(i, 0, ProfileCount - 1);
                            serialInput = Serial.read(); // nomf
                            serialInput = Serial.read();
                            uint8_t v = serialInput - '0';
                            v = constrain(v, 0, 2);
                            profileData[i].irSensitivity = v;
                            if(i == selectedProfile) {
                                SetIrSensitivity(v);
                            }
                            Serial.println("OK: Set IR sensitivity");
                            break;
                          }
                          case 'r':
                          {
                            serialInput = Serial.read(); // nomf
                            serialInput = Serial.read();
                            uint8_t i = serialInput - '0';
                            i = constrain(i, 0, ProfileCount - 1);
                            serialInput = Serial.read(); // nomf
                            serialInput = Serial.read();
                            uint8_t v = serialInput - '0';
                            v = constrain(v, 0, 2);
                            profileData[i].runMode = v;
                            if(i == selectedProfile) {
                                switch(v) {
                                  case 0:
                                    SetRunMode(RunMode_Normal);
                                    break;
                                  case 1:
                                    SetRunMode(RunMode_Average);
                                    break;
                                  case 2:
                                    SetRunMode(RunMode_Average2);
                                    break;
                                }
                            }
                            Serial.println("OK: Set Run Mode");
                            break;
                          }
                        }
                    }
                }
                break;
              }
              // Print EEPROM values.
              case 'l':
              {
                //Serial.println("Printing values saved in EEPROM...");
                uint8_t tempBools = 0b00000000;
                uint8_t *dataBools = &tempBools;

                if(nvPrefsError != SamcoPreferences::Error_Success) {
                    #ifdef USES_RUMBLE
                        bitWrite(tempBools, 0, rumbleActive);
                    #endif // USES_RUMBLE
                    #ifdef USES_SOLENOID
                        bitWrite(tempBools, 1, solenoidActive);
                    #endif // USES_SOLENOID
                    bitWrite(tempBools, 2, autofireActive);
                    bitWrite(tempBools, 3, simpleMenu);
                    bitWrite(tempBools, 4, holdToPause);
                    #ifdef FOURPIN_LED
                        bitWrite(tempBools, 5, commonAnode);
                    #endif // FOURPIN_LED
                    bitWrite(tempBools, 6, lowButtonMode);
                }

                // Temp pin mappings
                int8_t tempMappings[] = {
                    customPinsInUse,            // custom pin enabled - disabled by default
                    btnTrigger,
                    btnGunA,
                    btnGunB,
                    btnCalibrate,
                    btnStart,
                    btnSelect,
                    btnGunUp,
                    btnGunDown,
                    btnGunLeft,
                    btnGunRight,
                    btnPedal,
                    btnHome,
                    btnPump,
                    #ifdef USES_RUMBLE
                    rumblePin,
                    #else
                    -1,
                    #endif // USES_RUMBLE
                    #ifdef USES_SOLENOID
                    solenoidPin,
                    #ifdef USES_TEMP
                    tempPin,
                    #else
                    -1,
                    #endif // USES_TEMP
                    #else
                    -1,
                    #endif // USES_SOLENOID
                    #ifdef USES_SWITCHES
                    #ifdef USES_RUMBLE
                    rumbleSwitch,
                    #else
                    -1,
                    #endif // USES_RUMBLE
                    #ifdef USES_SOLENOID
                    solenoidSwitch,
                    #else
                    -1,
                    #endif // USES_SOLENOID
                    autofireSwitch,
                    #else
                    -1,
                    #endif // USES_SWITCHES
                    #ifdef FOURPIN_LED
                    PinR,
                    PinG,
                    PinB,
                    #else
                    -1,
                    -1,
                    -1,
                    #endif // FOURPIN_LED
                    #ifdef CUSTOM_NEOPIXEL
                    customLEDpin,
                    #else
                    -1,
                    #endif // CUSTOM_NEOPIXEL
                    #ifdef USES_ANALOG
                    analogPinX,
                    analogPinY,
                    #else
                    -1,
                    -1,
                    #endif // USES_ANALOG
                    -127
                };
                int8_t *dataMappings = tempMappings;

                uint16_t tempSettings[] = {
                    #ifdef USES_RUMBLE
                    rumbleIntensity,
                    rumbleInterval,
                    #endif // USES_RUMBLE
                    #ifdef USES_SOLENOID
                    solenoidNormalInterval,
                    solenoidFastInterval,
                    solenoidLongInterval,
                    #endif // USES_SOLENOID
                    #ifdef CUSTOM_NEOPIXEL
                    customLEDcount,
                    #endif // CUSTOM_NEOPIXEL
                    autofireWaitFactor,
                    pauseHoldLength
                };
                uint16_t *dataSettings = tempSettings;

                // Only load if there's been no errors reported
                if(nvPrefsError == SamcoPreferences::Error_Success) {
                    SamcoPreferences::LoadExtended(dataBools, dataMappings, dataSettings);
                }

                serialInput = Serial.read();
                switch(serialInput) {
                  case 'b':
                    //Serial.println("----------BOOL SETTINGS----------");
                    //Serial.print("Rumble Active: ");
                    Serial.println(bitRead(tempBools, 0));
                    //Serial.print("Solenoid Active: ");
                    Serial.println(bitRead(tempBools, 1));
                    //Serial.print("Autofire Active: ");
                    Serial.println(bitRead(tempBools, 2));
                    //Serial.print("Simple Pause Menu Enabled: ");
                    Serial.println(bitRead(tempBools, 3));
                    //Serial.print("Hold to Pause Enabled: ");
                    Serial.println(bitRead(tempBools, 4));
                    //Serial.print("Common Anode Active: ");
                    Serial.println(bitRead(tempBools, 5));
                    //Serial.print("Low Buttons Mode Active: ");
                    Serial.println(bitRead(tempBools, 6));
                    break;
                  case 'p':
                    //Serial.println("----------PIN MAPPINGS-----------");
                    //Serial.print("Custom pins layout enabled: ");
                    Serial.println(tempMappings[0]);
                    //Serial.print("Trigger: ");
                    Serial.println(tempMappings[1]);
                    //Serial.print("Button A: ");
                    Serial.println(tempMappings[2]);
                    //Serial.print("Button B: ");
                    Serial.println(tempMappings[3]);
                    //Serial.print("Button C: ");
                    Serial.println(tempMappings[4]);
                    //Serial.print("Start: ");
                    Serial.println(tempMappings[5]);
                    //Serial.print("Select: ");
                    Serial.println(tempMappings[6]);
                    //Serial.print("D-Pad Up: ");
                    Serial.println(tempMappings[7]);
                    //Serial.print("D-Pad Down: ");
                    Serial.println(tempMappings[8]);
                    //Serial.print("D-Pad Left: ");
                    Serial.println(tempMappings[9]);
                    //Serial.print("D-Pad Right: ");
                    Serial.println(tempMappings[10]);
                    //Serial.print("External Pedal: ");
                    Serial.println(tempMappings[11]);
                    //Serial.print("Home Button: ");
                    Serial.println(tempMappings[12]);
                    //Serial.print("Pump Action: ");
                    Serial.println(tempMappings[13]);
                    //Serial.print("Rumble Signal Wire: ");
                    Serial.println(tempMappings[14]);
                    //Serial.print("Solenoid Signal Wire: ");
                    Serial.println(tempMappings[15]);
                    // for some reason, at this point, QT stops reading output.
                    // so we'll just wait for a ping to print out the rest.
                    while(!Serial.available()) {
                      // derp
                    }
                    //Serial.print("Temperature Sensor: ");
                    Serial.println(tempMappings[16]);
                    //Serial.print("Rumble Switch: ");
                    Serial.println(tempMappings[17]);
                    //Serial.print("Solenoid Switch: ");
                    Serial.println(tempMappings[18]);
                    //Serial.print("Autofire Switch: ");
                    Serial.println(tempMappings[19]);
                    //Serial.print("LED R: ");
                    Serial.println(tempMappings[20]);
                    //Serial.print("LED G: ");
                    Serial.println(tempMappings[21]);
                    //Serial.print("LED B: ");
                    Serial.println(tempMappings[22]);
                    //Serial.print("Custom NeoPixel Pin: ");
                    Serial.println(tempMappings[23]);
                    //Serial.print("Analog Joystick X: ");
                    Serial.println(tempMappings[24]);
                    //Serial.print("Analog Joystick Y: ");
                    Serial.println(tempMappings[25]);
                    //Serial.print("Padding Bit (Should be -127): ");
                    Serial.println(tempMappings[26]);
                    // clean house now from my dirty hack.
                    while(Serial.available()) {
                      serialInput = Serial.read();
                    }
                    break;
                  case 's':
                    //Serial.println("----------OTHER SETTINGS-----------");
                    //Serial.print("Rumble Intensity Value: ");
                    Serial.println(tempSettings[0]);
                    //Serial.print("Rumble Length: ");
                    Serial.println(tempSettings[1]);
                    //Serial.print("Solenoid Normal Interval: ");
                    Serial.println(tempSettings[2]);
                    //Serial.print("Solenoid Fast Interval: ");
                    Serial.println(tempSettings[3]);
                    //Serial.print("Solenoid Hold Length: ");
                    Serial.println(tempSettings[4]);
                    //Serial.print("Custom NeoPixel Strip Length: ");
                    Serial.println(tempSettings[5]);
                    //Serial.print("Autofire Wait Factor: ");
                    Serial.println(tempSettings[6]);
                    //Serial.print("Hold to Pause Length: ");
                    Serial.println(tempSettings[7]);
                    break;
                  case 'P':
                    serialInput = Serial.read();
                    if(serialInput == '0' || serialInput == '1' ||
                       serialInput == '2' || serialInput == '3') {
                        uint8_t i = serialInput - '0';
                        Serial.println(profileData[i].xScale);
                        Serial.println(profileData[i].yScale);
                        Serial.println(profileData[i].xCenter);
                        Serial.println(profileData[i].yCenter);
                        Serial.println(profileData[i].irSensitivity);
                        Serial.println(profileData[i].runMode);
                    }
                    break;
                  #ifdef USE_TINYUSB
                  case 'n':
                    for(byte i = 0; i < sizeof(deviceName); i++) {
                        deviceName[i] = '\0';
                    }
                    for(byte i = 0; i < 16; i++) {
                        deviceName[i] = EEPROM.read(EEPROM.length() - 18 + i);
                    }
                    if(deviceName[0] == '\0') {
                        Serial.println("SERIALREADERR01");
                    } else {
                        Serial.println(deviceName);
                    }
                    break;
                  case 'i':
                    devicePID = 0;
                    EEPROM.get(EEPROM.length() - 22, devicePID);
                    Serial.println(devicePID);
                    break;
                  #endif // USE_TINYUSB
                }
                break;
              }
              // Testing feedback
              case 't':
                serialInput = Serial.read();
                if(serialInput == 's') {
                  digitalWrite(solenoidPin, HIGH);
                  delay(solenoidNormalInterval);
                  digitalWrite(solenoidPin, LOW);
                } else if(serialInput == 'r') {
                  analogWrite(rumblePin, rumbleIntensity);
                  delay(rumbleInterval);
                  digitalWrite(rumblePin, LOW);
                }
                break;
          }
          break;
    }
}

#ifdef MAMEHOOKER
// Reading the input from the serial buffer.
// for normal runmode runtime use w/ e.g. Mamehook et al
void SerialProcessing()
{
    // So, APPARENTLY there is a map of what the serial commands are, at least wrt gun controllers.
    // "Sx" is a start command, the x being a number from 0-6 (0=sol, 1=motor, 2-4=led colors, 6=everything) but this is more a formal suggestion than anything.
    // "E" is an end command, which is the cue to hand control over back to the sketch's force feedback.
    // "M" sets parameters for how the gun should be handled when the game starts:
    //  - M3 is fullscreen or 4:3, which we don't do so ignore this
    //  - M5 is auto reload(?). How do we use this?
    //  - M1 is the reload type (0=none, 1=shoot lower left(?), 2=offscreen button, 3=real offscreen reload)
    //  - M8 is the type of auto fire (0=single shot, 1="auto"(is this a burst fire, or what?), 2=always/full auto)
    // "Fx" is the type of force feedback command being received (periods are for readability and not in the actual commands):
    //  - F0.x = solenoid - 0=off, 1=on, 2=pulse (which is the same as 1 for the solenoid)
    //  - F1.x.y = rumble motor - 0=off, 1=normal on, 2=pulse (where Y is the number of "pulses" it does)
    //  - F2/3/4.x.y = Red/Green/Blue (respectively) LED - 0=off, 1=on, 2=pulse (where Y is the strength of the LED)
    // These commands are all sent as one clump of data, with the letter being the distinguisher.
    // Apart from "E" (end), each setting bit (S/M) is two chars long, and each feedback bit (F) is four (the command, a padding bit of some kind, and the value).

    char serialInput = Serial.read();                              // Read the serial input one byte at a time (we'll read more later)
    char serialInputS[3] = {0, 0, 0};

    switch(serialInput) {
        // Start Signal
        case 'S':
          if(serialMode) {
              Serial.println("SERIALREAD: Detected Serial Start command while already in Serial handoff mode!");
          } else {
              serialMode = true;                                         // Set it on, then!
              #ifdef USES_RUMBLE
                  digitalWrite(rumblePin, LOW);                          // Turn off stale rumbling from normal gun mode.
                  rumbleHappened = false;
                  rumbleHappening = false;
              #endif // USES_RUMBLE
              #ifdef USES_SOLENOID
                  digitalWrite(solenoidPin, LOW);                        // Turn off stale solenoid-ing from normal gun mode.
                  solenoidFirstShot = false;
              #endif // USES_SOLENOID
              triggerHeld = false;                                       // Turn off other stale values that serial mode doesn't use.
              burstFiring = false;
              burstFireCount = 0;
              offscreenBShot = false;
              #ifdef LED_ENABLE
                  // Set the LEDs to a mid-intense white.
                  LedUpdate(127, 127, 127);
              #endif // LED_ENABLE
          }
          break;
        // Modesetting Signal
        case 'M':
          serialInput = Serial.read();                               // Read the second bit.
          switch(serialInput) {
              case '1':
                if(serialMode) {
                    offscreenButtonSerial = true;
                } else {
                    // eh, might be useful for Linux Supermodel users.
                    offscreenButton = !offscreenButton;
                    if(offscreenButton) {
                        Serial.println("Setting offscreen button mode on!");
                    } else {
                        Serial.println("Setting offscreen button mode off!");
                    }
                }
                break;
              #ifdef USES_SOLENOID
              case '8':
                serialInput = Serial.read();                           // Nomf the padding bit.
                serialInput = Serial.read();                           // Read the next.
                if(serialInput == '1') {
                    burstFireActive = true;
                    autofireActive = false;
                } else if(serialInput == '2') {
                    autofireActive = true;
                    burstFireActive = false;
                } else if(serialInput == '0') {
                    autofireActive = false;
                    burstFireActive = false;
                }
                break;
              #endif // USES_SOLENOID
              default:
                if(!serialMode) {
                    Serial.println("SERIALREAD: Serial modesetting command found, but no valid set bit found!");
                }
                break;
          }
          break;
        // End Signal
        case 'E':
          if(!serialMode) {
              Serial.println("SERIALREAD: Detected Serial End command while Serial Handoff mode is already off!");
          } else {
              serialMode = false;                                    // Turn off serial mode then.
              offscreenButtonSerial = false;                         // And clear the stale serial offscreen button mode flag.
              serialQueue = 0b00000000;
              #ifdef LED_ENABLE
                  serialLEDPulseColorMap = 0b00000000;               // Clear any stale serial LED pulses
                  serialLEDPulses = 0;
                  serialLEDPulsesLast = 0;
                  serialLEDPulseRising = true;
                  serialLEDR = 0;                                    // Clear stale serial LED values.
                  serialLEDG = 0;
                  serialLEDB = 0;
                  serialLEDChange = false;
                  LedOff();                                          // Turn it off, and let lastSeen handle it from here.
              #endif // LED_ENABLE
              #ifdef USES_RUMBLE
                  digitalWrite(rumblePin, LOW);
                  serialRumbPulseStage = 0;
                  serialRumbPulses = 0;
                  serialRumbPulsesLast = 0;
              #endif // USES_RUMBLE
              #ifdef USES_SOLENOID
                  digitalWrite(solenoidPin, LOW);
                  serialSolPulseOn = false;
                  serialSolPulses = 0;
                  serialSolPulsesLast = 0;
              #endif // USES_SOLENOID
              AbsMouse5.release(MOUSE_LEFT);
              AbsMouse5.release(MOUSE_RIGHT);
              AbsMouse5.release(MOUSE_MIDDLE);
              AbsMouse5.release(MOUSE_BUTTON4);
              AbsMouse5.release(MOUSE_BUTTON5);
              Keyboard.releaseAll();
              delay(5);
              Serial.println("Received end serial pulse, releasing FF override.");
          }
          break;
        // owo SPECIAL SETUP EH?
        case 'X':
          serialInput = Serial.read();
          switch(serialInput) {
              // Toggle Gamepad Output Mode
              case 'A':
                serialInput = Serial.read();
                switch(serialInput) {
                    case 'L':
                      if(!buttons.analogOutput) {
                          buttons.analogOutput = true;
                          AbsMouse5.release(MOUSE_LEFT);
                          AbsMouse5.release(MOUSE_RIGHT);
                          AbsMouse5.release(MOUSE_MIDDLE);
                          AbsMouse5.release(MOUSE_BUTTON4);
                          AbsMouse5.release(MOUSE_BUTTON5);
                          Keyboard.releaseAll();
                          Serial.println("Switched to Analog Output mode!");
                      }
                      Gamepad16.stickRight = true;
                      Serial.println("Setting camera to the Left Stick.");
                      break;
                    case 'R':
                      if(!buttons.analogOutput) {
                          buttons.analogOutput = true;
                          AbsMouse5.release(MOUSE_LEFT);
                          AbsMouse5.release(MOUSE_RIGHT);
                          AbsMouse5.release(MOUSE_MIDDLE);
                          AbsMouse5.release(MOUSE_BUTTON4);
                          AbsMouse5.release(MOUSE_BUTTON5);
                          Keyboard.releaseAll();
                          Serial.println("Switched to Analog Output mode!");
                      }
                      Gamepad16.stickRight = false;
                      Serial.println("Setting camera to the Right Stick.");
                      break;
                    default:
                      buttons.analogOutput = !buttons.analogOutput;
                      if(buttons.analogOutput) {
                          AbsMouse5.release(MOUSE_LEFT);
                          AbsMouse5.release(MOUSE_RIGHT);
                          AbsMouse5.release(MOUSE_MIDDLE);
                          AbsMouse5.release(MOUSE_BUTTON4);
                          AbsMouse5.release(MOUSE_BUTTON5);
                          Keyboard.releaseAll();
                          Serial.println("Switched to Analog Output mode!");
                      } else {
                          Gamepad16.releaseAll();
                          Keyboard.releaseAll();
                          Serial.println("Switched to Mouse Output mode!");
                      }
                      break;
                }
                break;
              // Set Autofire Interval Length
              case 'I':
                serialInput = Serial.read();
                if(serialInput == '2' || serialInput == '3' || serialInput == '4') {
                    byte afSetting = serialInput - '0';
                    AutofireSpeedToggle(afSetting);
                } else {
                    Serial.println("SERIALREAD: No valid interval set! (Expected 2 to 4)");
                }
                break;
              // Remap player numbers
              case 'R':
                serialInput = Serial.read();
                switch(serialInput) {
                    case '1':
                      playerStartBtn = '1';
                      playerSelectBtn = '5';
                      Serial.println("Remapping to player slot 1.");
                      break;
                    case '2':
                      playerStartBtn = '2';
                      playerSelectBtn = '6';
                      Serial.println("Remapping to player slot 2.");
                      break;
                    case '3':
                      playerStartBtn = '3';
                      playerSelectBtn = '7';
                      Serial.println("Remapping to player slot 3.");
                      break;
                    case '4':
                      playerStartBtn = '4';
                      playerSelectBtn = '8';
                      Serial.println("Remapping to player slot 4.");
                      break;
                    default:
                      Serial.println("SERIALREAD: Player remap command called, but an invalid or no slot number was declared!");
                      break;
                }
                UpdateBindings(lowButtonMode);
                break;
              // Enter Docked Mode
              case 'P':
                SetMode(GunMode_Docked);
                break;
              default:
                Serial.println("SERIALREAD: Internal setting command detected, but no valid option found!");
                Serial.println("Internally recognized commands are:");
                Serial.println("A(nalog)[L/R] / I(nterval Autofire)2/3/4 / R(emap)1/2/3/4 / P(ause)");
                break;
          }
          // End of 'X'
          break;
        // Force Feedback
        case 'F':
          serialInput = Serial.read();
          switch(serialInput) {
              #ifdef USES_SOLENOID
              // Solenoid bits
              case '0':
                serialInput = Serial.read();                           // nomf the padding since it's meaningless.
                serialInput = Serial.read();                           // Read the next number.
                if(serialInput == '1') {         // Is it a solenoid "on" command?)
                    bitSet(serialQueue, 0);                            // Queue the solenoid on bit.
                } else if(serialInput == '2' &&  // Is it a solenoid pulse command?
                !bitRead(serialQueue, 1)) {      // (and we aren't already pulsing?)
                    bitSet(serialQueue, 1);                            // Set the solenoid pulsing bit!
                    serialInput = Serial.read();                       // nomf the padding bit.
                    for(byte n = 0; n < 3; n++) {                      // For three runs,
                        serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                        if(serialInputS[n] == 'x' || serialInputS[n] == '.') {
                            break;
                        }
                    }
                    serialSolPulses = atoi(serialInputS);              // Import the amount of pulses we're being told to do.
                    serialSolPulsesLast = 0;                           // PulsesLast on zero indicates we haven't started pulsing.
                } else if(serialInput == '0') {  // Else, it's a solenoid off signal.
                    bitClear(serialQueue, 0);                          // Disable the solenoid off bit!
                }
                break;
              #endif // USES_SOLENOID
              #ifdef USES_RUMBLE
              // Rumble bits
              case '1':
                serialInput = Serial.read();                           // nomf the padding since it's meaningless.
                serialInput = Serial.read();                           // read the next number.
                if(serialInput == '1') {         // Is it an on signal?
                    bitSet(serialQueue, 2);                            // Queue the rumble on bit.
                } else if(serialInput == '2' &&  // Is it a pulsed on signal?
                !bitRead(serialQueue, 3)) {      // (and we aren't already pulsing?)
                    bitSet(serialQueue, 3);                            // Set the rumble pulsed bit.
                    serialInput = Serial.read();                       // nomf the x
                    for(byte n = 0; n < 3; n++) {                      // For three runs,
                        serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                        if(serialInputS[n] == 'x' || serialInputS[n] == '.') {
                            break;
                        }
                    }
                    serialRumbPulses = atoi(serialInputS);             // and set as the amount of rumble pulses queued.
                    serialRumbPulsesLast = 0;                          // Reset the serialPulsesLast count.
                } else if(serialInput == '0') {  // Else, it's a rumble off signal.
                    bitClear(serialQueue, 2);                          // Queue the rumble off bit... 
                    //bitClear(serialQueue, 3); // And the rumble pulsed bit.
                    // TODO: do we want to set this off if we get a rumble off bit?
                }
                break;
              #endif // USES_RUMBLE
              #ifdef LED_ENABLE
              // LED Red bits
              case '2':
                serialLEDChange = true;                                // Set that we've changed an LED here!
                serialInput = Serial.read();                           // nomf the padding since it's meaningless.
                serialInput = Serial.read();                           // Read the next number
                if(serialInput == '1') {         // is it an "on" command?
                    bitSet(serialQueue, 4);                            // set that here!
                    serialInput = Serial.read();                       // nomf the padding
                    for(byte n = 0; n < 3; n++) {                      // For three runs,
                        serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                        if(serialInputS[n] == 'x' || serialInputS[n] == '.') {
                            break;
                        }
                    }
                    serialLEDR = atoi(serialInputS);                   // And set that as the strength of the red value that's requested!
                } else if(serialInput == '2' &&  // else, is it a pulse command?
                !bitRead(serialQueue, 7)) {      // (and we haven't already sent a pulse command?)
                    bitSet(serialQueue, 7);                            // Set the pulse bit!
                    serialLEDPulseColorMap = 0b00000001;               // Set the R LED as the one pulsing only (overwrites the others).
                    serialInput = Serial.read();                       // nomf the padding
                    for(byte n = 0; n < 3; n++) {                      // For three runs,
                        serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                        if(serialInputS[n] == 'x' || serialInputS[n] == '.') {
                            break;
                        }
                    }
                    serialLEDPulses = atoi(serialInputS);              // and set that as the amount of pulses requested
                    serialLEDPulsesLast = 0;                           // reset the pulses done count.
                } else if(serialInput == '0') {  // else, it's an off command.
                    bitClear(serialQueue, 4);                          // Set the R bit off.
                    serialLEDR = 0;                                    // Clear the R value.
                }
                break;
              // LED Green bits
              case '3':
                serialLEDChange = true;                                // Set that we've changed an LED here!
                serialInput = Serial.read();                           // nomf the padding since it's meaningless.
                serialInput = Serial.read();                           // Read the next number
                if(serialInput == '1') {         // is it an "on" command?
                    bitSet(serialQueue, 5);                            // set that here!
                    serialInput = Serial.read();                       // nomf the padding
                    for(byte n = 0; n < 3; n++) {                      // For three runs,
                        serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                        if(serialInputS[n] == 'x' || serialInputS[n] == '.') {
                            break;
                        }
                    }
                    serialLEDG = atoi(serialInputS);                   // And set that here!
                } else if(serialInput == '2' &&  // else, is it a pulse command?
                !bitRead(serialQueue, 7)) {      // (and we haven't already sent a pulse command?)
                    bitSet(serialQueue, 7);                            // Set the pulse bit!
                    serialLEDPulseColorMap = 0b00000010;               // Set the G LED as the one pulsing only (overwrites the others).
                    serialInput = Serial.read();                       // nomf the padding
                    for(byte n = 0; n < 3; n++) {                      // For three runs,
                        serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                        if(serialInputS[n] == 'x' || serialInputS[n] == '.') {
                            break;
                        }
                    }
                    serialLEDPulses = atoi(serialInputS);              // and set that as the amount of pulses requested
                    serialLEDPulsesLast = 0;                           // reset the pulses done count.
                } else if(serialInput == '0') {  // else, it's an off command.
                    bitClear(serialQueue, 5);                       // Set the G bit off.
                    serialLEDG = 0;                                    // Clear the G value.
                }
                break;
              // LED Blue bits
              case '4':
                serialLEDChange = true;                                // Set that we've changed an LED here!
                serialInput = Serial.read();                           // nomf the padding since it's meaningless.
                serialInput = Serial.read();                           // Read the next number
                if(serialInput == '1') {         // is it an "on" command?
                    bitSet(serialQueue, 6);                       // set that here!
                    serialInput = Serial.read();                       // nomf the padding
                    for(byte n = 0; n < 3; n++) {                      // For three runs,
                        serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                        if(serialInputS[n] == 'x' || serialInputS[n] == '.') {
                            break;
                        }
                    }
                    serialLEDB = atoi(serialInputS);                   // And set that as the strength requested here!
                } else if(serialInput == '2' &&  // else, is it a pulse command?
                !bitRead(serialQueue, 7)) {      // (and we haven't already sent a pulse command?)
                    bitSet(serialQueue, 7);                       // Set the pulse bit!
                    serialLEDPulseColorMap = 0b00000100;               // Set the B LED as the one pulsing only (overwrites the others).
                    serialInput = Serial.read();                       // nomf the padding
                    for(byte n = 0; n < 3; n++) {                      // For three runs,
                        serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                        if(serialInputS[n] == 'x' || serialInputS[n] == '.') {
                            break;
                        }
                    }
                    serialLEDPulses = atoi(serialInputS);              // and set that as the amount of pulses requested
                    serialLEDPulsesLast = 0;                           // reset the pulses done count.
                } else if(serialInput == '0') {  // else, it's an off command.
                    bitClear(serialQueue, 6);                          // Set the B bit off.
                    serialLEDB = 0;                                    // Clear the G value.
                }
                break;
              #endif // LED_ENABLE
              #if !defined(USES_SOLENOID) && !defined(USES_RUMBLE) && !defined(LED_ENABLE)
              default:
                Serial.println("SERIALREAD: Feedback command detected, but no feedback devices are built into this firmware!");
              #endif
          }
          // End of 'F'
          break;
    }
}

// Handling the serial events received from SerialProcessing()
void SerialHandling()
{   // The Mamehook feedback system handles all the timing and safety for us.
    // So all we have to do is just read and process what it sends us at face value.
    // The only exception is rumble PULSE bits, where we actually do need to calculate that ourselves.

    #ifdef USES_SOLENOID
      if(solenoidActive) {
          if(bitRead(serialQueue, 0)) {                             // If the solenoid digital bit is on,
              digitalWrite(solenoidPin, HIGH);                           // Make it go!
          } else if(bitRead(serialQueue, 1)) {                      // if the solenoid pulse bit is on,
              if(!serialSolPulsesLast) {                            // Have we started pulsing?
                  analogWrite(solenoidPin, 178);                         // Start pulsing it on!
                  serialSolPulseOn = true;                               // Set that the pulse cycle is in on.
                  serialSolPulsesLast = 1;                               // Start the sequence.
                  serialSolPulses++;                                     // Cheating and scooting the pulses bit up.
              } else if(serialSolPulsesLast <= serialSolPulses) {   // Have we met the pulses quota?
                  unsigned long currentMillis = millis();                // Calibrate timer.
                  if(currentMillis - serialSolPulsesLastUpdate > serialSolPulsesLength) { // Have we passed the set interval length between stages?
                      if(serialSolPulseOn) {                        // If we're currently pulsing on,
                          analogWrite(solenoidPin, 122);                 // Start pulsing it off.
                          serialSolPulseOn = false;                      // Set that we're in off.
                          serialSolPulsesLast++;                         // Iterate that we've done a pulse cycle,
                          serialSolPulsesLastUpdate = millis();          // Timestamp our last pulse event.
                      } else {                                      // Or if we're pulsing off,
                          analogWrite(solenoidPin, 178);                 // Start pulsing it on.
                          serialSolPulseOn = true;                       // Set that we're in on.
                          serialSolPulsesLastUpdate = millis();          // Timestamp our last pulse event.
                      }
                  }
              } else {  // let the armature smoothly sink loose for one more pulse length before snapping it shut off.
                  unsigned long currentMillis = millis();                // Calibrate timer.
                  if(currentMillis - serialSolPulsesLastUpdate > serialSolPulsesLength) { // Have we paassed the set interval length between stages?
                      digitalWrite(solenoidPin, LOW);                    // Finally shut it off for good.
                      bitClear(serialQueue, 1);                          // Set the pulse bit as off.
                  }
              }
          } else {  // or if it's not,
              digitalWrite(solenoidPin, LOW);                            // turn it off!
          }
      } else {
          digitalWrite(solenoidPin, LOW);
      }
  #endif // USES_SOLENOID
  #ifdef USES_RUMBLE
      if(rumbleActive) {
          if(bitRead(serialQueue, 2)) {                             // Is the rumble on bit set?
              analogWrite(rumblePin, rumbleIntensity);              // turn/keep it on.
              //bitClear(serialQueue, 3);
          } else if(bitRead(serialQueue, 3)) {                      // or if the rumble pulse bit is set,
              if(!serialRumbPulsesLast) {                           // is the pulses last bit set to off?
                  analogWrite(rumblePin, 75);                            // we're starting fresh, so use the stage 0 value.
                  serialRumbPulseStage = 0;                              // Set that we're at stage 0.
                  serialRumbPulsesLast = 1;                              // Set that we've started a pulse rumble command, and start counting how many pulses we're doing.
              } else if(serialRumbPulsesLast <= serialRumbPulses) { // Have we exceeded the set amount of pulses the rumble command called for?
                  unsigned long currentMillis = millis();                // Calibrate the timer.
                  if(currentMillis - serialRumbPulsesLastUpdate > serialRumbPulsesLength) { // have we waited enough time between pulse stages?
                      switch(serialRumbPulseStage) {                     // If so, let's start processing.
                          case 0:                                        // Basically, each case
                              analogWrite(rumblePin, 255);               // bumps up the intensity, (lowest to rising)
                              serialRumbPulseStage++;                    // and increments the stage of the pulse.
                              serialRumbPulsesLastUpdate = millis();     // and timestamps when we've had updated this last.
                              break;                                     // Then quits the switch.
                          case 1:
                              analogWrite(rumblePin, 120);               // (rising to peak)
                              serialRumbPulseStage++;
                              serialRumbPulsesLastUpdate = millis();
                              break;
                          case 2:
                              analogWrite(rumblePin, 75);                // (peak to falling,)
                              serialRumbPulseStage = 0;
                              serialRumbPulsesLast++;
                              serialRumbPulsesLastUpdate = millis();
                              break;
                      }
                  }
              } else {                                              // ...or the pulses count is complete.
                  digitalWrite(rumblePin, LOW);                          // turn off the motor,
                  bitClear(serialQueue, 3);                              // and set the rumble pulses bit off, now that we've completed it.
              }
          } else {                                                  // ...or we're being told to turn it off outright.
              digitalWrite(rumblePin, LOW);                              // Do that then.
          }
      } else {
          digitalWrite(rumblePin, LOW);
      }
  #endif // USES_RUMBLE
  #ifdef LED_ENABLE
    if(serialLEDChange) {                                     // Has the LED command state changed?
        if(bitRead(serialQueue, 4) ||                         // Are either the R,
        bitRead(serialQueue, 5) ||                            // G,
        bitRead(serialQueue, 6)) {                            // OR B digital bits set to on?
            // Command the LED to change/turn on with the values serialProcessing set for us.
            LedUpdate(serialLEDR, serialLEDG, serialLEDB);
            serialLEDChange = false;                               // Set the bit to off.
        } else if(bitRead(serialQueue, 7)) {                  // Or is it an LED pulse command?
            if(!serialLEDPulsesLast) {                        // Are we just starting?
                serialLEDPulsesLast = 1;                           // Set that we have started.
                serialLEDPulseRising = true;                       // Set the LED cycle to rising.
                // Reset all the LEDs to zero, the color map will tell us which one to focus on.
                serialLEDR = 0;
                serialLEDG = 0;
                serialLEDB = 0;
            } else if(serialLEDPulsesLast <= serialLEDPulses) { // Else, have we not reached the number of pulses requested?
                unsigned long currentMillis = millis();            // Calibrate the timer.
                if(currentMillis - serialLEDPulsesLastUpdate > serialLEDPulsesLength) { // have we waited enough time between pulse stages?
                    if(serialLEDPulseRising) {                // If we're in the rising stage,
                        switch(serialLEDPulseColorMap) {           // Check the color map
                            case 0b00000001:                       // Basically for R, G, or B,
                                serialLEDR += 3;                   // Set the LED value up by three (it's easiest to do blindly like this without over/underflowing tbh)
                                if(serialLEDR == 255) {       // If we've reached the max value,
                                    serialLEDPulseRising = false;  // Set that we're in the falling state now.
                                }
                                serialLEDPulsesLastUpdate = millis(); // Timestamp this event.
                                break;                             // And get out.
                            case 0b00000010:
                                serialLEDG += 3;
                                if(serialLEDG == 255) {
                                    serialLEDPulseRising = false;
                                }
                                serialLEDPulsesLastUpdate = millis();
                                break;
                            case 0b00000100:
                                serialLEDB += 3;
                                if(serialLEDB == 255) {
                                    serialLEDPulseRising = false;
                                }
                                serialLEDPulsesLastUpdate = millis();
                                break;
                        }
                    } else {                                  // Or, we're in the falling stage.
                        switch(serialLEDPulseColorMap) {           // Check the color map.
                            case 0b00000001:                       // Then here, for the set color,
                                serialLEDR -= 3;                   // Decrement the value.
                                if(serialLEDR == 0) {         // If the LED value has reached the lowest point,
                                    serialLEDPulseRising = true;   // Set that we should be in the rising part of a new cycle.
                                    serialLEDPulsesLast++;         // This was a pulse cycle, so increment that.
                                }
                                serialLEDPulsesLastUpdate = millis(); // Timestamp this event.
                                break;                             // Get outta here.
                            case 0b00000010:
                                serialLEDG -= 3;
                                if(serialLEDG == 0) {
                                    serialLEDPulseRising = true;
                                    serialLEDPulsesLast++;
                                }
                                serialLEDPulsesLastUpdate = millis();
                                break;
                            case 0b00000100:
                                serialLEDB -= 3;
                                if(serialLEDB == 0) {
                                    serialLEDPulseRising = true;
                                    serialLEDPulsesLast++;
                                }
                                serialLEDPulsesLastUpdate = millis();
                                break;
                        }
                    }
                    // Then, commit the changed value.
                    LedUpdate(serialLEDR, serialLEDG, serialLEDB);
                }
            } else {                                       // Or, we're done with the amount of pulse commands.
                serialLEDPulseColorMap = 0b00000000;               // Clear the now-stale pulse color map,
                bitClear(serialQueue, 7);                          // And flick the pulse command bit off.
            }
        } else {                                           // Or, all the LED bits are off, so we should be setting it off entirely.
            LedOff();                                              // Turn it off.
            serialLEDChange = false;                               // We've done the change, so set it off to reduce redundant LED updates.
        }
    }
    #endif // LED_ENABLE
}

// Trigger execution path while in Serial handoff mode - pulled
void TriggerFireSimple()
{
    if(!buttonPressed &&                             // Have we not fired the last cycle,
    offscreenButtonSerial && buttons.offScreen) {    // and are pointing the gun off screen WITH the offScreen button mode set?
        AbsMouse5.press(MOUSE_RIGHT);                // Press the right mouse button
        offscreenBShot = true;                       // Mark we pressed the right button via offscreen shot mode,
        buttonPressed = true;                        // Mark so we're not spamming these press events.
    } else if(!buttonPressed) {                      // Else, have we simply not fired the last cycle?
        AbsMouse5.press(MOUSE_LEFT);                 // We're handling the trigger button press ourselves for a reason.
        buttonPressed = true;                        // Set this so we won't spam a repeat press event again.
    }
}

// Trigger execution path while in Serial handoff mode - released
void TriggerNotFireSimple()
{
    if(buttonPressed) {                              // Just to make sure we aren't spamming mouse button events.
        if(offscreenBShot) {                         // if it was marked as an offscreen button shot,
            AbsMouse5.release(MOUSE_RIGHT);          // Release the right mouse,
            offscreenBShot = false;                  // And set it off.
        } else {                                     // Else,
            AbsMouse5.release(MOUSE_LEFT);           // It was a normal shot, so just release the left mouse button.
        }
        buttonPressed = false;                       // Unset the button pressed bit.
    }
}
#endif // MAMEHOOKER


void SendExitKey()
{
    Keyboard.press(KEY_ESC);
    delay(20);  // wait a bit so it registers on the PC.
    Keyboard.release(KEY_ESC);
    Serial.println("Exit Key Pressed");
}

void SendMenuKey()
{
    Keyboard.press(KEY_TAB);
    delay(20);  // wait a bit so it registers on the PC.
    Keyboard.release(KEY_TAB);
    Serial.println("Menu Key Pressed");
}

// Macro for functions to run when gun enters new gunmode
void SetMode(GunMode_e newMode)
{
    if(gunMode == newMode) {
        return;
    }
    
    // exit current mode
    switch(gunMode) {
    case GunMode_Run:
        stateFlags |= StateFlag_PrintPreferences;
        break;
    case GunMode_CalHoriz:
        break;
    case GunMode_CalVert:
        break;
    case GunMode_CalCenter:
        break;
    case GunMode_Pause:
        break;
    case GunMode_Docked:
        if(!dockedCalibrating) {
            Serial.println("Undocking.");
        }
        break;
    }
    
    // enter new mode
    gunMode = newMode;
    switch(newMode) {
    case GunMode_Run:
        // begin run mode with all 4 points seen
        lastSeen = 0x0F;        
        break;
    case GunMode_CalHoriz:
        break;
    case GunMode_CalVert:
        break;
    case GunMode_CalCenter:
        break;
    case GunMode_Pause:
        stateFlags |= StateFlag_SavePreferencesEn | StateFlag_PrintSelectedProfile;
        break;
    case GunMode_Docked:
        stateFlags |= StateFlag_SavePreferencesEn;
        if(!dockedCalibrating) {
            Serial.println("PIGS");
            Serial.println(PIGS_VERSION, 1);
            Serial.println(PIGS_BOARD);
            Serial.println(selectedProfile);
        }
        break;
    }

    #ifdef LED_ENABLE
        SetLedColorFromMode();
    #endif // LED_ENABLE
}

// set new run mode and apply it to the selected profile
void SetRunMode(RunMode_e newMode)
{
    if(newMode >= RunMode_Count) {
        return;
    }

    // block Processing/test modes being applied to a profile
    if(newMode <= RunMode_ProfileMax && profileData[selectedProfile].runMode != newMode) {
        profileData[selectedProfile].runMode = newMode;
        stateFlags |= StateFlag_SavePreferencesEn;
    }
    
    if(runMode != newMode) {
        runMode = newMode;          
        if(!(stateFlags & StateFlag_PrintSelectedProfile)) {
            PrintRunMode();
        }
    }
}

// Simple Pause Menu scrolling function
// Bool determines if it's incrementing or decrementing the list
// LEDs update according to the setting being scrolled onto, if any.
void SetPauseModeSelection(bool isIncrement)
{
    if(isIncrement) {
        if(pauseModeSelection == PauseMode_EscapeSignal) {
            pauseModeSelection = PauseMode_Calibrate;
        } else {
            pauseModeSelection++;
            // If we use switches, and they ARE mapped to valid pins,
            // then skip over the manual toggle options.
            #ifdef USES_SWITCHES
                #ifdef USES_RUMBLE
                    if(pauseModeSelection == PauseMode_RumbleToggle &&
                    rumbleSwitch >= 0 && rumblePin >= 0) {
                        pauseModeSelection++;
                    }
                #endif // USES_RUMBLE
                #ifdef USES_SOLENOID
                    if(pauseModeSelection == PauseMode_SolenoidToggle &&
                    solenoidSwitch >= 0 && solenoidPin >= 0) {
                        pauseModeSelection++;
                    }
                #endif // USES_SOLENOID
            #else
                #ifdef USES_RUMBLE
                    if(pauseModeSelection == PauseMode_RumbleToggle &&
                    rumblePin >= 0) {
                        pauseModeSelection++;
                    }
                #endif // USES_RUMBLE
                #ifdef USES_SOLENOID
                    if(pauseModeSelection == PauseMode_SolenoidToggle &&
                    solenoidPin >= 0) {
                        pauseModeSelection++;
                    }
                #endif // USES_SOLENOID
            #endif // USES_SWITCHES
        }
    } else {
        if(pauseModeSelection == PauseMode_Calibrate) {
            pauseModeSelection = PauseMode_EscapeSignal;
        } else {
            pauseModeSelection--;
            #ifdef USES_SWITCHES
                #ifdef USES_SOLENOID
                    if(pauseModeSelection == PauseMode_SolenoidToggle &&
                    solenoidSwitch >= 0 && solenoidPin >= 0) {
                        pauseModeSelection--;
                    }
                #endif // USES_SOLENOID
                #ifdef USES_RUMBLE
                    if(pauseModeSelection == PauseMode_RumbleToggle &&
                    rumbleSwitch >= 0 && rumblePin >= 0) {
                        pauseModeSelection--;
                    }
                #endif // USES_RUMBLE
            #else
                #ifdef USES_SOLENOID
                    if(pauseModeSelection == PauseMode_SolenoidToggle &&
                    solenoidPin >= 0) {
                        pauseModeSelection--;
                    }
                #endif // USES_SOLENOID
                #ifdef USES_RUMBLE
                    if(pauseModeSelection == PauseMode_RumbleToggle &&
                    rumblePin >= 0) {
                        pauseModeSelection--;
                    }
                #endif // USES_RUMBLE
            #endif // USES_SWITCHES
        }
    }
    switch(pauseModeSelection) {
        case PauseMode_Calibrate:
          Serial.println("Selecting: Calibrate current profile");
          #ifdef LED_ENABLE
              LedUpdate(255,0,0);
          #endif // LED_ENABLE
          break;
        case PauseMode_ProfileSelect:
          Serial.println("Selecting: Switch profile");
          #ifdef LED_ENABLE
              LedUpdate(200,50,0);
          #endif // LED_ENABLE
          break;
        case PauseMode_Save:
          Serial.println("Selecting: Save Settings");
          #ifdef LED_ENABLE
              LedUpdate(155,100,0);
          #endif // LED_ENABLE
          break;
        #ifdef USES_RUMBLE
        case PauseMode_RumbleToggle:
          Serial.println("Selecting: Toggle rumble On/Off");
          #ifdef LED_ENABLE
              LedUpdate(100,155,0);
          #endif // LED_ENABLE
          break;
        #endif // USES_RUMBLE
        #ifdef USES_SOLENOID
        case PauseMode_SolenoidToggle:
          Serial.println("Selecting: Toggle solenoid On/Off");
          #ifdef LED_ENABLE
              LedUpdate(55,200,0);
          #endif // LED_ENABLE
          break;
        #endif // USES_SOLENOID
        /*#ifdef USES_SOLENOID
        case PauseMode_BurstFireToggle:
          Serial.println("Selecting: Toggle burst-firing mode");
          #ifdef LED_ENABLE
              LedUpdate(0,255,0);
          #endif // LED_ENABLE
          break;
        #endif // USES_SOLENOID
        */
        case PauseMode_EscapeSignal:
          Serial.println("Selecting: Send Escape key signal");
          #ifdef LED_ENABLE
              LedUpdate(150,0,150);
          #endif // LED_ENABLE
          break;
        /*case PauseMode_Exit:
          Serial.println("Selecting: Exit pause mode");
          break;
        */
        default:
          Serial.println("YOU'RE NOT SUPPOSED TO BE SEEING THIS");
          break;
    }
}

// Simple Pause Mode - scrolls up/down profiles list
// Bool determines if it's incrementing or decrementing the list
// LEDs update according to the profile being scrolled on, if any.
void SetProfileSelection(bool isIncrement)
{
    if(isIncrement) {
        if(profileModeSelection >= ProfileCount - 1) {
            profileModeSelection = 0;
        } else {
            profileModeSelection++;
        }
    } else {
        if(profileModeSelection <= 0) {
            profileModeSelection = ProfileCount - 1;
        } else {
            profileModeSelection--;
        }
    }
    #ifdef LED_ENABLE
        SetLedPackedColor(profileDesc[profileModeSelection].color);
    #endif // LED_ENABLE
    Serial.print("Selecting profile: ");
    Serial.println(profileDesc[profileModeSelection].profileLabel);
    return;
}

// Main routine that prints information to connected serial monitor when the gun enters Pause Mode.
void PrintResults()
{
    if(millis() - lastPrintMillis < 100) {
        return;
    }

    if(!Serial.dtr()) {
        stateFlags |= StateFlagsDtrReset;
        return;
    }

    PrintPreferences();
    /*
    Serial.print(finalX);
    Serial.print(" (");
    Serial.print(MoveXAxis);
    Serial.print("), ");
    Serial.print(finalY);
    Serial.print(" (");
    Serial.print(MoveYAxis);
    Serial.print("), H ");
    Serial.println(mySamco.H());*/

    //Serial.print("conMove ");
    //Serial.print(conMoveXAxis);
    //Serial.println(conMoveYAxis);
    
    if(stateFlags & StateFlag_PrintSelectedProfile) {
        stateFlags &= ~StateFlag_PrintSelectedProfile;
        PrintSelectedProfile();
        PrintIrSensitivity();
        PrintRunMode();
        PrintCal();
        PrintExtras();
    }
        
    lastPrintMillis = millis();
}

// what does this do again? lol
void PrintCalInterval()
{
    if(millis() - lastPrintMillis < 100 || dockedCalibrating) {
        return;
    }
    PrintCal();
    lastPrintMillis = millis();
}

// helper in case this changes
float CalScalePrefToFloat(uint16_t scale)
{
    return (float)scale / 1000.0f;
}

// helper in case this changes
uint16_t CalScaleFloatToPref(float scale)
{
    return (uint16_t)(scale * 1000.0f);
}

// Subroutine that prints all stored preferences information in a table
void PrintPreferences()
{
    if(!(stateFlags & StateFlag_PrintPreferences) || !Serial.dtr()) {
        return;
    }

    stateFlags &= ~StateFlag_PrintPreferences;
    
    PrintNVPrefsError();

    if(stateFlags & StateFlag_PrintPreferencesStorage) {
        stateFlags &= ~StateFlag_PrintPreferencesStorage;
        PrintNVStorage();
    }
    
    Serial.print("Default Profile: ");
    Serial.println(profileDesc[SamcoPreferences::preferences.profile].profileLabel);
    
    Serial.println("Profiles:");
    for(unsigned int i = 0; i < SamcoPreferences::preferences.profileCount; ++i) {
        // report if a profile has been cal'd
        if(profileData[i].xCenter && profileData[i].yCenter) {
            size_t len = strlen(profileDesc[i].buttonLabel) + 2;
            Serial.print(profileDesc[i].buttonLabel);
            Serial.print(": ");
            if(profileDesc[i].profileLabel && profileDesc[i].profileLabel[0]) {
                Serial.print(profileDesc[i].profileLabel);
                len += strlen(profileDesc[i].profileLabel);
            }
            while(len < 26) {
                Serial.print(' ');
                ++len;
            }
            Serial.print("Center: ");
            Serial.print(profileData[i].xCenter);
            Serial.print(",");
            Serial.print(profileData[i].yCenter);
            Serial.print(" Scale: ");
            Serial.print(CalScalePrefToFloat(profileData[i].xScale), 3);
            Serial.print(",");
            Serial.print(CalScalePrefToFloat(profileData[i].yScale), 3);
            Serial.print(" IR: ");
            Serial.print((unsigned int)profileData[i].irSensitivity);
            Serial.print(" Mode: ");
            Serial.println((unsigned int)profileData[i].runMode);
        }
    }
}

// Subroutine that prints current calibration profile
void PrintCal()
{
    Serial.print("Calibration: Center x,y: ");
    Serial.print(xCenter);
    Serial.print(",");
    Serial.print(yCenter);
    Serial.print(" Scale x,y: ");
    Serial.print(xScale, 3);
    Serial.print(",");
    Serial.println(yScale, 3);
}

// Subroutine that prints current runmode
void PrintRunMode()
{
    if(runMode < RunMode_Count) {
        Serial.print("Mode: ");
        Serial.println(RunModeLabels[runMode]);
    }
}

// Prints basic storage device information
// an estimation of storage used, though doesn't take extended prefs into account.
void PrintNVStorage()
{
#ifdef SAMCO_FLASH_ENABLE
    unsigned int required = SamcoPreferences::Size();
#ifndef PRINT_VERBOSE
    if(required < flash.size()) {
        return;
    }
#endif
    Serial.print("NV Storage capacity: ");
    Serial.print(flash.size());
    Serial.print(", required size: ");
    Serial.println(required);
#ifdef PRINT_VERBOSE
    Serial.print("Profile struct size: ");
    Serial.print((unsigned int)sizeof(SamcoPreferences::ProfileData_t));
    Serial.print(", Profile data array size: ");
    Serial.println((unsigned int)sizeof(profileData));
#endif
#endif // SAMCO_FLASH_ENABLE
}

// Prints error to connected serial monitor if no storage device is present
void PrintNVPrefsError()
{
    if(nvPrefsError != SamcoPreferences::Error_Success) {
        Serial.print(NVRAMlabel);
        Serial.print(" error: ");
#ifdef SAMCO_FLASH_ENABLE
        Serial.println(SamcoPreferences::ErrorCodeToString(nvPrefsError));
#else
        Serial.println(nvPrefsError);
#endif // SAMCO_FLASH_ENABLE
    }
}

// Extra settings to print to connected serial monitor when entering Pause Mode
void PrintExtras()
{
    Serial.print("Offscreen button mode enabled: ");
    if(offscreenButton) {
        Serial.println("True");
    } else {
        Serial.println("False");
    }
    #ifdef USES_RUMBLE
        Serial.print("Rumble enabled: ");
        if(rumbleActive) {
            Serial.println("True");
        } else {
            Serial.println("False");
        }
    #endif // USES_RUMBLE
    #ifdef USES_SOLENOID
        Serial.print("Solenoid enabled: ");
        if(solenoidActive) {
            Serial.println("True");
            Serial.print("Rapid fire enabled: ");
            if(autofireActive) {
                Serial.println("True");
            } else {
                Serial.println("False");
            }
            Serial.print("Burst fire enabled: ");
            if(burstFireActive) {
                Serial.println("True");
            } else {
                Serial.println("False");
            }
        } else {
            Serial.println("False");
        }
    #endif // USES_SOLENOID
    #ifdef ARDUINO_ARCH_RP2040
    #ifdef DUAL_CORE
        Serial.println("Running on dual cores.");
    #else
        Serial.println("Running on one core.");
    #endif // DUAL_CORE
    #endif // ARDUINO_ARCH_RP2040
    Serial.print("Firmware version: v");
    Serial.print(PIGS_VERSION, 1);
    Serial.print(" - ");
}

// Loads preferences from EEPROM, then verifies.
void LoadPreferences()
{
    if(!nvAvailable) {
        return;
    }

#ifdef SAMCO_FLASH_ENABLE
    nvPrefsError = SamcoPreferences::Load(flash);
#else
    nvPrefsError = samcoPreferences.Load();
#endif // SAMCO_FLASH_ENABLE
    VerifyPreferences();
}

// Profile sanity checks
void VerifyPreferences()
{
    // center 0 is used as "no cal data"
    for(unsigned int i = 0; i < ProfileCount; ++i) {
        if(profileData[i].xCenter >= MouseMaxX || profileData[i].yCenter >= MouseMaxY || profileData[i].xScale == 0 || profileData[i].yScale == 0) {
            profileData[i].xCenter = 0;
            profileData[i].yCenter = 0;
        }

        // if the scale values are large, assign 0 so the values will be ignored
        if(profileData[i].xScale >= 30000) {
            profileData[i].xScale = 0;
        }
        if(profileData[i].yScale >= 30000) {
            profileData[i].yScale = 0;
        }
    
        if(profileData[i].irSensitivity > DFRobotIRPositionEx::Sensitivity_Max) {
            profileData[i].irSensitivity = DFRobotIRPositionEx::Sensitivity_Default;
        }

        if(profileData[i].runMode >= RunMode_Count) {
            profileData[i].runMode = RunMode_Normal;
        }
    }

    // if default profile is not valid, use current selected profile instead
    if(SamcoPreferences::preferences.profile >= ProfileCount) {
        SamcoPreferences::preferences.profile = (uint8_t)selectedProfile;
    }
}

// Apply initial preferences, intended to be called only in setup() after LoadPreferences()
// this will apply the preferences data as the initial values
void ApplyInitialPrefs()
{   
    // if default profile is valid then use it
    if(SamcoPreferences::preferences.profile < ProfileCount) {
        // note, just set the value here not call the function to do the set
        selectedProfile = SamcoPreferences::preferences.profile;

        // set the current IR camera sensitivity
        if(profileData[selectedProfile].irSensitivity <= DFRobotIRPositionEx::Sensitivity_Max) {
            irSensitivity = (DFRobotIRPositionEx::Sensitivity_e)profileData[selectedProfile].irSensitivity;
        }

        // set the run mode
        if(profileData[selectedProfile].runMode < RunMode_Count) {
            runMode = (RunMode_e)profileData[selectedProfile].runMode;
        }
    }
}

// Saves profile settings to EEPROM
// Blinks LEDs (if any) on success or failure.
void SavePreferences()
{
    // Unless the user's Docked,
    // Only allow one write per pause state until something changes.
    // Extra protection to ensure the same data can't write a bunch of times.
    if(gunMode != GunMode_Docked) {
        if(!nvAvailable || !(stateFlags & StateFlag_SavePreferencesEn)) {
            return;
        }

        stateFlags &= ~StateFlag_SavePreferencesEn;
    }
    
    // use selected profile as the default
    SamcoPreferences::preferences.profile = (uint8_t)selectedProfile;

#ifdef SAMCO_FLASH_ENABLE
    nvPrefsError = SamcoPreferences::Save(flash);
#else
    nvPrefsError = SamcoPreferences::Save();
#endif // SAMCO_FLASH_ENABLE
    if(nvPrefsError == SamcoPreferences::Error_Success) {
        Serial.print("Settings saved to ");
        Serial.println(NVRAMlabel);
        ExtPreferences(false);
        #ifdef LED_ENABLE
            for(byte i = 0; i < 3; i++) {
                LedUpdate(25,25,255);
                delay(55);
                LedOff();
                delay(40);
            }
        #endif // LED_ENABLE
    } else {
        Serial.println("Error saving Preferences.");
        PrintNVPrefsError();
        #ifdef LED_ENABLE
            for(byte i = 0; i < 2; i++) {
                LedUpdate(255,10,5);
                delay(145);
                LedOff();
                delay(60);
            }
        #endif // LED_ENABLE
    }
}

// Saves &/or loads extended preferences (booleans, pin mappings, general settings, TinyUSB ID) from EEPROM
// boolean determines if it's a save or load operation (defaults to save)
void ExtPreferences(bool isLoad)
{
    uint8_t tempBools = 0b00000000;
    uint8_t *dataBools = &tempBools;

    if(!isLoad) {
        #ifdef USES_RUMBLE
            bitWrite(tempBools, 0, rumbleActive);
        #endif // USES_RUMBLE
        #ifdef USES_SOLENOID
            bitWrite(tempBools, 1, solenoidActive);
        #endif // USES_SOLENOID
        bitWrite(tempBools, 2, autofireActive);
        bitWrite(tempBools, 3, simpleMenu);
        bitWrite(tempBools, 4, holdToPause);
        #ifdef FOURPIN_LED
            bitWrite(tempBools, 5, commonAnode);
        #endif // FOURPIN_LED
        bitWrite(tempBools, 6, lowButtonMode);
    }

    // Temp pin mappings
    int8_t tempMappings[] = {
      customPinsInUse,            // custom pin enabled - disabled by default
      btnTrigger,
      btnGunA,
      btnGunB,
      btnCalibrate,
      btnStart,
      btnSelect,
      btnGunUp,
      btnGunDown,
      btnGunLeft,
      btnGunRight,
      btnPedal,
      btnHome,
      btnPump,
      #ifdef USES_RUMBLE
      rumblePin,
      #else
      -1,
      #endif // USES_RUMBLE
      #ifdef USES_SOLENOID
      solenoidPin,
      #ifdef USES_TEMP
      tempPin,
      #else
      -1,
      #endif // USES_TEMP
      #else
      -1,
      #endif // USES_SOLENOID
      #ifdef USES_SWITCHES
      #ifdef USES_RUMBLE
      rumbleSwitch,
      #else
      -1,
      #endif // USES_RUMBLE
      #ifdef USES_SOLENOID
      solenoidSwitch,
      #else
      -1,
      #endif // USES_SOLENOID
      autofireSwitch,
      #else
      -1,
      #endif // USES_SWITCHES
      #ifdef FOURPIN_LED
      PinR,
      PinG,
      PinB,
      #else
      -1,
      -1,
      -1,
      #endif // FOURPIN_LED
      #ifdef CUSTOM_NEOPIXEL
      customLEDpin,
      #else
      -1,
      #endif // CUSTOM_NEOPIXEL
      #ifdef USES_ANALOG
      analogPinX,
      analogPinY,
      #else
      -1,
      -1,
      #endif // USES_ANALOG
      -127
    };
    int8_t *dataMappings = tempMappings;

    uint16_t tempSettings[] = {
      #ifdef USES_RUMBLE
      rumbleIntensity,
      rumbleInterval,
      #endif // USES_RUMBLE
      #ifdef USES_SOLENOID
      solenoidNormalInterval,
      solenoidFastInterval,
      solenoidLongInterval,
      #endif // USES_SOLENOID
      #ifdef CUSTOM_NEOPIXEL
      customLEDcount,
      #endif // CUSTOM_NEOPIXEL
      autofireWaitFactor,
      pauseHoldLength
    };
    uint16_t *dataSettings = tempSettings;

    if(!isLoad) {
        nvPrefsError = SamcoPreferences::SaveExtended(dataBools, dataMappings, dataSettings);
        if(nvPrefsError == SamcoPreferences::Error_Success) {
            Serial.println("Saved!");
        } else {
            Serial.println("Error!");
        }
    } else {
        SamcoPreferences::LoadExtended(dataBools, dataMappings, dataSettings);

        // Set Bools
        #ifdef USES_RUMBLE
            rumbleActive = bitRead(tempBools, 0);
        #endif // USES_RUMBLE
        #ifdef USES_SOLENOID
            solenoidActive = bitRead(tempBools, 1);
        #endif // USES_SOLENOID
        autofireActive = bitRead(tempBools, 2);
        simpleMenu = bitRead(tempBools, 3);
        holdToPause = bitRead(tempBools, 4);
        #ifdef FOURPIN_LED
            commonAnode = bitRead(tempBools, 5);
        #endif // FOURPIN_LED
        lowButtonMode = bitRead(tempBools, 6);

        // Set pins, if allowed.
        customPinsInUse = tempMappings[0];
        if(customPinsInUse) {
            btnTrigger = tempMappings[1];
            btnGunA = tempMappings[2];
            btnGunB = tempMappings[3];
            btnCalibrate = tempMappings[4];
            btnStart = tempMappings[5];
            btnSelect = tempMappings[6];
            btnGunUp = tempMappings[7];
            btnGunDown = tempMappings[8];
            btnGunLeft = tempMappings[9];
            btnGunRight = tempMappings[10];
            btnPedal = tempMappings[11];
            btnHome = tempMappings[12];
            btnPump = tempMappings[13];
            #ifdef USES_RUMBLE
            rumblePin = tempMappings[14];
            #endif // USES_RUMBLE
            #ifdef USES_SOLENOID
            solenoidPin = tempMappings[15];
            #ifdef USES_TEMP
            tempPin = tempMappings[16];
            #endif // USES_TEMP
            #endif // USES_SOLENOID
            #ifdef USES_SWITCHES
            #ifdef USES_RUMBLE
            rumbleSwitch = tempMappings[17];
            #endif // USES_RUMBLE
            #ifdef USES_SOLENOID
            solenoidSwitch = tempMappings[18];
            #endif // USES_SOLENOID
            autofireSwitch = tempMappings[19];
            #endif // USES_SWITCHES
            #ifdef FOURPIN_LED
            PinR = tempMappings[20];
            PinG = tempMappings[21];
            PinB = tempMappings[22];
            #endif // FOURPIN_LED
            #ifdef CUSTOM_NEOPIXEL
            customLEDpin = tempMappings[23];
            #endif // CUSTOM_NEOPIXEL
            #ifdef USES_ANALOG
            analogPinX = tempMappings[24];
            analogPinY = tempMappings[25];
            #endif // USES_ANALOG
        }

        // Set other settings
        #ifdef USES_RUMBLE
        rumbleIntensity = tempSettings[0];
        rumbleInterval = tempSettings[1];
        #endif // USES_RUMBLE
        #ifdef USES_SOLENOID
        solenoidNormalInterval = tempSettings[2];
        solenoidFastInterval = tempSettings[3];
        solenoidLongInterval = tempSettings[4];
        #endif // USES_SOLENOID
        #ifdef CUSTOM_NEOPIXEL
        customLEDcount = tempSettings[5];
        #endif // CUSTOM_NEOPIXEL
        autofireWaitFactor = tempSettings[6];
        pauseHoldLength = tempSettings[7];
    }
    if(justBooted && lowButtonMode) {
        UpdateBindings(true);
    } else if(justBooted && !lowButtonMode) {
        UpdateBindings(false);
    }
}

void SelectCalProfileFromBtnMask(uint32_t mask)
{
    // only check if buttons are set in the mask
    if(!mask) {
        return;
    }
    for(unsigned int i = 0; i < ProfileCount; ++i) {
        if(profileDesc[i].buttonMask == mask) {
            SelectCalProfile(i);
            return;
        }
    }
}

void CycleIrSensitivity()
{
    uint8_t sens = irSensitivity;
    if(irSensitivity < DFRobotIRPositionEx::Sensitivity_Max) {
        sens++;
    } else {
        sens = DFRobotIRPositionEx::Sensitivity_Min;
    }
    SetIrSensitivity(sens);
}

void IncreaseIrSensitivity()
{
    uint8_t sens = irSensitivity;
    if(irSensitivity < DFRobotIRPositionEx::Sensitivity_Max) {
        sens++;
        SetIrSensitivity(sens);
    }
}

void DecreaseIrSensitivity()
{
    uint8_t sens = irSensitivity;
    if(irSensitivity > DFRobotIRPositionEx::Sensitivity_Min) {
        sens--;
        SetIrSensitivity(sens);
    }
}

// set a new IR camera sensitivity and apply to the selected profile
void SetIrSensitivity(uint8_t sensitivity)
{
    if(sensitivity > DFRobotIRPositionEx::Sensitivity_Max) {
        return;
    }

    if(profileData[selectedProfile].irSensitivity != sensitivity) {
        profileData[selectedProfile].irSensitivity = sensitivity;
        stateFlags |= StateFlag_SavePreferencesEn;
    }

    if(irSensitivity != (DFRobotIRPositionEx::Sensitivity_e)sensitivity) {
        irSensitivity = (DFRobotIRPositionEx::Sensitivity_e)sensitivity;
        dfrIRPos.sensitivityLevel(irSensitivity);
        if(!(stateFlags & StateFlag_PrintSelectedProfile)) {
            PrintIrSensitivity();
        }
    }
}

void PrintIrSensitivity()
{
    Serial.print("IR Camera Sensitivity: ");
    Serial.println((int)irSensitivity);
}

void CancelCalibration()
{
    Serial.println("Calibration cancelled");
    // re-print the profile
    stateFlags |= StateFlag_PrintSelectedProfile;
    // re-apply the cal stored in the profile
    RevertToCalProfile(selectedProfile);
    if(dockedCalibrating) {
        SetMode(GunMode_Docked);
        dockedCalibrating = false;
    } else {
        // return to pause mode
        SetMode(GunMode_Pause);
    } 
}

void PrintSelectedProfile()
{
    Serial.print("Profile: ");
    Serial.println(profileDesc[selectedProfile].profileLabel);
}

// applies loaded gun profile settings
bool SelectCalProfile(unsigned int profile)
{
    if(profile >= ProfileCount) {
        return false;
    }

    if(selectedProfile != profile) {
        stateFlags |= StateFlag_PrintSelectedProfile;
        selectedProfile = profile;
    }

    bool valid = SelectCalPrefs(profile);

    // set IR sensitivity
    if(profileData[profile].irSensitivity <= DFRobotIRPositionEx::Sensitivity_Max) {
        SetIrSensitivity(profileData[profile].irSensitivity);
    }

    // set run mode
    if(profileData[profile].runMode < RunMode_Count) {
        SetRunMode((RunMode_e)profileData[profile].runMode);
    }
 
    #ifdef LED_ENABLE
        SetLedColorFromMode();
    #endif // LED_ENABLE

    // enable save to allow setting new default profile
    stateFlags |= StateFlag_SavePreferencesEn;
    return valid;
}

// applies loaded screen calibration profile
bool SelectCalPrefs(unsigned int profile)
{
    if(profile >= ProfileCount) {
        return false;
    }

    // if center values are set, assume profile is populated
    if(profileData[profile].xCenter && profileData[profile].yCenter) {
        xCenter = profileData[profile].xCenter;
        yCenter = profileData[profile].yCenter;
        
        // 0 scale will be ignored
        if(profileData[profile].xScale) {
            xScale = CalScalePrefToFloat(profileData[profile].xScale);
        }
        if(profileData[profile].yScale) {
            yScale = CalScalePrefToFloat(profileData[profile].yScale);
        }
        return true;
    }
    return false;
}

// revert back to useable settings, even if not cal'd
void RevertToCalProfile(unsigned int profile)
{
    // if the current profile isn't valid
    if(!SelectCalProfile(profile)) {
        // revert to settings from any valid profile
        for(unsigned int i = 0; i < ProfileCount; ++i) {
            if(SelectCalProfile(i)) {
                break;
            }
        }

        // stay selected on the specified profile
        SelectCalProfile(profile);
    }
}

// apply current cal to the selected profile
void ApplyCalToProfile()
{
    profileData[selectedProfile].xCenter = xCenter;
    profileData[selectedProfile].yCenter = yCenter;
    profileData[selectedProfile].xScale = CalScaleFloatToPref(xScale);
    profileData[selectedProfile].yScale = CalScaleFloatToPref(yScale);

    stateFlags |= StateFlag_PrintSelectedProfile;
}

#ifdef LED_ENABLE
// initializes system and 4pin RGB LEDs.
// ONLY to be used from setup()
void LedInit()
{
    // init DotStar and/or NeoPixel to red during setup()
    // For the onboard NEOPIXEL, if any; it needs to be enabled.
    #ifdef NEOPIXEL_ENABLEPIN
        pinMode(NEOPIXEL_ENABLEPIN, OUTPUT);
        digitalWrite(NEOPIXEL_ENABLEPIN, HIGH);
    #endif // NEOPIXEL_ENABLEPIN
 
    #ifdef DOTSTAR_ENABLE
        dotstar.begin();
    #endif // DOTSTAR_ENABLE

    #ifdef NEOPIXEL_PIN
        neopixel.begin();
    #endif // NEOPIXEL_PIN
    #ifdef CUSTOM_NEOPIXEL
        if(customLEDpin >= 0) {
            externPixel->begin();
        }
    #endif // CUSTOM_NEOPIXEL
 
    #ifdef ARDUINO_NANO_RP2040_CONNECT
    pinMode(LEDR, OUTPUT);
    pinMode(LEDG, OUTPUT);
    pinMode(LEDB, OUTPUT);
    #endif // NANO_RP2040
    LedUpdate(255, 0, 0);
}

// 32-bit packed color value update across all LED units
void SetLedPackedColor(uint32_t color)
{
#ifdef DOTSTAR_ENABLE
    dotstar.setPixelColor(0, Adafruit_DotStar::gamma32(color & 0x00FFFFFF));
    dotstar.show();
#endif // DOTSTAR_ENABLE
#ifdef NEOPIXEL_PIN
    neopixel.setPixelColor(0, Adafruit_NeoPixel::gamma32(color & 0x00FFFFFF));
    neopixel.show();
#endif // NEOPIXEL_PIN
#ifdef CUSTOM_NEOPIXEL
    if(customLEDpin >= 0) {
        externPixel->fill(0, Adafruit_NeoPixel::gamma32(color & 0x00FFFFFF));
        externPixel->show();
    }
#endif // CUSTOM_NEOPIXEL
#ifdef FOURPIN_LED
    if(ledIsValid) {
        byte r = highByte(color >> 8);
        byte g = highByte(color);
        byte b = lowByte(color);
        if(commonAnode) {
            r = ~r;
            g = ~g;
            b = ~b;
        }
        analogWrite(PinR, r);
        analogWrite(PinG, g);
        analogWrite(PinB, b);
    }
#endif // FOURPIN_LED
#ifdef ARDUINO_NANO_RP2040_CONNECT
    byte r = highByte(color >> 8);
    byte g = highByte(color);
    byte b = lowByte(color);
    r = ~r;
    g = ~g;
    b = ~b;
    analogWrite(LEDR, r);
    analogWrite(LEDG, g);
    analogWrite(LEDB, b);
#endif // NANO_RP2040
}

void LedOff()
{
    LedUpdate(0, 0, 0);
}

// Generic R/G/B value update across all LED units
void LedUpdate(byte r, byte g, byte b)
{
    #ifdef DOTSTAR_ENABLE
        dotstar.setPixelColor(0, r, g, b);
        dotstar.show();
    #endif // DOTSTAR_ENABLE
    #ifdef NEOPIXEL_PIN
        neopixel.setPixelColor(0, r, g, b);
        neopixel.show();
    #endif // NEOPIXEL_PIN
    #ifdef CUSTOM_NEOPIXEL
        if(customLEDpin >= 0) {
            for(byte i = 0; i < customLEDcount; i++) {
                externPixel->setPixelColor(i, r, g, b);
            }
            externPixel->show();
        }
    #endif // CUSTOM_NEOPIXEL
    #ifdef FOURPIN_LED
        if(ledIsValid) {
            if(commonAnode) {
                r = ~r;
                g = ~g;
                b = ~b;
            }
            analogWrite(PinR, r);
            analogWrite(PinG, g);
            analogWrite(PinB, b);
        }
    #endif // FOURPIN_LED
    #ifdef ARDUINO_NANO_RP2040_CONNECT
        #ifdef FOURPIN_LED
        // Nano's builtin is a common anode, so we use that logic by default if it's enabled on the external 4-pin;
        // otherwise, invert the values.
        if((ledIsValid && !commonAnode) || !ledIsValid) {
            r = ~r;
            g = ~g;
            b = ~b;
        }
        #else
            r = ~r;
            g = ~g;
            b = ~b;
        #endif // FOURPIN_LED
        analogWrite(LEDR, r);
        analogWrite(LEDG, g);
        analogWrite(LEDB, b);
    #endif // NANO_RP2040
}

// Macro that sets LEDs color depending on the mode it's set to
void SetLedColorFromMode()
{
    switch(gunMode) {
    case GunMode_CalHoriz:
    case GunMode_CalVert:
    case GunMode_CalCenter:
        SetLedPackedColor(CalModeColor);
        break;
    case GunMode_Pause:
        SetLedPackedColor(profileDesc[selectedProfile].color);
        break;
    case GunMode_Run:
        if(lastSeen) {
            LedOff();
        } else {
            SetLedPackedColor(IRSeen0Color);
        }
        break;
    default:
        break;
    }
}
#endif // LED_ENABLE

// Pause mode offscreen trigger mode toggle widget
// Blinks LEDs (if any) according to enabling or disabling this obtuse setting for finnicky/old programs that need right click as an offscreen shot substitute.
void OffscreenToggle()
{
    offscreenButton = !offscreenButton;
    if(offscreenButton) {                                         // If we turned ON this mode,
        Serial.println("Enabled Offscreen Button!");
        #ifdef LED_ENABLE
            SetLedPackedColor(WikiColor::Ghost_white);            // Set a color,
        #endif // LED_ENABLE
        #ifdef USES_RUMBLE
            digitalWrite(rumblePin, HIGH);                        // Set rumble on
            delay(125);                                           // For this long,
            digitalWrite(rumblePin, LOW);                         // Then flick it off,
            delay(150);                                           // wait a little,
            digitalWrite(rumblePin, HIGH);                        // Flick it back on
            delay(200);                                           // For a bit,
            digitalWrite(rumblePin, LOW);                         // and then turn it off,
        #else
            delay(450);
        #endif // USES_RUMBLE
        #ifdef LED_ENABLE
            SetLedPackedColor(profileDesc[selectedProfile].color);// And reset the LED back to pause mode color
        #endif // LED_ENABLE
        return;
    } else {                                                      // Or we're turning this OFF,
        Serial.println("Disabled Offscreen Button!");
        #ifdef LED_ENABLE
            SetLedPackedColor(WikiColor::Ghost_white);            // Just set a color,
            delay(150);                                           // Keep it on,
            LedOff();                                             // Flicker it off
            delay(100);                                           // for a bit,
            SetLedPackedColor(WikiColor::Ghost_white);            // Flicker it back on
            delay(150);                                           // for a bit,
            LedOff();                                             // And turn it back off
            delay(200);                                           // for a bit,
            SetLedPackedColor(profileDesc[selectedProfile].color);// And reset the LED back to pause mode color
        #endif // LED_ENABLE
        return;
    }
}

// Pause mode autofire factor toggle widget
// Does a test fire demonstrating the autofire speed being toggled
void AutofireSpeedToggle(byte setting)
{
    // If a number is passed, assume this is from Serial and directly set it.
    if(setting >= 2 && setting <= 4) {
        autofireWaitFactor = setting;
        Serial.print("Autofire speed level ");
        Serial.println(setting);
        return;
    // Else, this is a button toggle, so cycle.
    } else {
        switch (autofireWaitFactor) {
            case 2:
                autofireWaitFactor = 3;
                Serial.println("Autofire speed level 2.");
                break;
            case 3:
                autofireWaitFactor = 4;
                Serial.println("Autofire speed level 3.");
                break;
            case 4:
                autofireWaitFactor = 2;
                Serial.println("Autofire speed level 1.");
                break;
        }
        #ifdef LED_ENABLE
            SetLedPackedColor(WikiColor::Magenta);                    // Set a color,
        #endif // LED_ENABLE
        #ifdef USES_SOLENOID
            for(byte i = 0; i < 5; i++) {                             // And demonstrate the new autofire factor five times!
                digitalWrite(solenoidPin, HIGH);
                delay(solenoidFastInterval);
                digitalWrite(solenoidPin, LOW);
                delay(solenoidFastInterval * autofireWaitFactor);
            }
        #endif // USES_SOLENOID
        #ifdef LED_ENABLE
            SetLedPackedColor(profileDesc[selectedProfile].color);    // And reset the LED back to pause mode color
        #endif // LED_ENABLE
        return;
    }
}

/*
// Unused since runtime burst fire toggle was removed from pause mode, and is accessed from the serial command M8x1
void BurstFireToggle()
{
    burstFireActive = !burstFireActive;                           // Toggle burst fire mode.
    if(burstFireActive) {  // Did we flick it on?
        Serial.println("Burst firing enabled!");
        #ifdef LED_ENABLE
            SetLedPackedColor(WikiColor::Orange);
        #endif
        #ifdef USES_SOLENOID
            for(byte i = 0; i < 4; i++) {
                digitalWrite(solenoidPin, HIGH);                  // Demonstrate it by flicking the solenoid on/off three times!
                delay(solenoidFastInterval);                      // (at a fixed rate to distinguish it from autofire speed toggles)
                digitalWrite(solenoidPin, LOW);
                delay(solenoidFastInterval * 2);
            }
        #endif // USES_SOLENOID
        #ifdef LED_ENABLE
            SetLedPackedColor(profileDesc[selectedProfile].color);// And reset the LED back to pause mode color
        #endif // LED_ENABLE
        return;
    } else {  // Or we flicked it off.
        Serial.println("Burst firing disabled!");
        #ifdef LED_ENABLE
            SetLedPackedColor(WikiColor::Orange);
        #endif // LED_ENABLE
        #ifdef USES_SOLENOID
            digitalWrite(solenoidPin, HIGH);                      // Just hold it on for a second.
            delay(300);
            digitalWrite(solenoidPin, LOW);                       // Then off.
        #endif // USES_SOLENOID
        #ifdef LED_ENABLE
            SetLedPackedColor(profileDesc[selectedProfile].color);// And reset the LED back to pause mode color
        #endif // LED_ENABLE
        return;
    }
}
*/

#ifdef USES_RUMBLE
// Pause mode rumble enabling widget
// Does a cute rumble pattern when on, or blinks LEDs (if any)
void RumbleToggle()
{
    rumbleActive = !rumbleActive;                                 // Toggle
    if(rumbleActive) {                                            // If we turned ON this mode,
        Serial.println("Rumble enabled!");
        #ifdef LED_ENABLE
            SetLedPackedColor(WikiColor::Salmon);                 // Set a color,
        #endif // LED_ENABLE
        digitalWrite(rumblePin, HIGH);                            // Pulse the motor on to notify the user,
        delay(300);                                               // Hold that,
        digitalWrite(rumblePin, LOW);                             // Then turn off,
        #ifdef LED_ENABLE
            SetLedPackedColor(profileDesc[selectedProfile].color);// And reset the LED back to pause mode color
        #endif // LED_ENABLE
        return;
    } else {                                                      // Or if we're turning it OFF,
        Serial.println("Rumble disabled!");
        #ifdef LED_ENABLE
            SetLedPackedColor(WikiColor::Salmon);                 // Set a color,
            delay(150);                                           // Keep it on,
            LedOff();                                             // Flicker it off
            delay(100);                                           // for a bit,
            SetLedPackedColor(WikiColor::Salmon);                 // Flicker it back on
            delay(150);                                           // for a bit,
            LedOff();                                             // And turn it back off
            delay(200);                                           // for a bit,
            SetLedPackedColor(profileDesc[selectedProfile].color);// And reset the LED back to pause mode color
        #endif // LED_ENABLE
        return;
    }
}
#endif // USES_RUMBLE

#ifdef USES_SOLENOID
// Pause mode solenoid enabling widget
// Does a cute solenoid engagement, or blinks LEDs (if any)
void SolenoidToggle()
{
    solenoidActive = !solenoidActive;                             // Toggle
    if(solenoidActive) {                                          // If we turned ON this mode,
        Serial.println("Solenoid enabled!");
        #ifdef LED_ENABLE
            SetLedPackedColor(WikiColor::Yellow);                 // Set a color,
        #endif // LED_ENABLE
        digitalWrite(solenoidPin, HIGH);                          // Engage the solenoid on to notify the user,
        delay(300);                                               // Hold it that way for a bit,
        digitalWrite(solenoidPin, LOW);                           // Release it,
        #ifdef LED_ENABLE
            SetLedPackedColor(profileDesc[selectedProfile].color);    // And reset the LED back to pause mode color
        #endif // LED_ENABLE
        return;
    } else {                                                      // Or if we're turning it OFF,
        Serial.println("Solenoid disabled!");
        #ifdef LED_ENABLE
            SetLedPackedColor(WikiColor::Yellow);                 // Set a color,
            delay(150);                                           // Keep it on,
            LedOff();                                             // Flicker it off
            delay(100);                                           // for a bit,
            SetLedPackedColor(WikiColor::Yellow);                 // Flicker it back on
            delay(150);                                           // for a bit,
            LedOff();                                             // And turn it back off
            delay(200);                                           // for a bit,
            SetLedPackedColor(profileDesc[selectedProfile].color);// And reset the LED back to pause mode color
        #endif // LED_ENABLE
        return;
    }
}
#endif // USES_SOLENOID

#ifdef USES_SOLENOID
// Subroutine managing solenoid state and temperature monitoring (if any)
void SolenoidActivation(int solenoidFinalInterval)
{
    if(solenoidFirstShot) {                                       // If this is the first time we're shooting, it's probably safe to shoot regardless of temps.
        unsigned long currentMillis = millis();                   // Initialize timer.
        previousMillisSol = currentMillis;                        // Calibrate the timer for future calcs.
        digitalWrite(solenoidPin, HIGH);                          // Since we're shooting the first time, just turn it on aaaaand fire.
        return;                                                   // We're done here now.
    }
    #ifdef USES_TEMP                                              // *If the build calls for a TMP36 temperature sensor,
        if(tempPin >= 0) { // If a temp sensor is installed and enabled,
            int tempSensor = analogRead(tempPin);
            tempSensor = (((tempSensor * 3.3) / 4096) - 0.5) * 100; // Convert reading from mV->3.3->12-bit->Celsius
            #ifdef PRINT_VERBOSE
                Serial.print("Current Temp near solenoid: ");
                Serial.print(tempSensor);
                Serial.println("*C");
            #endif
            if(tempSensor < tempNormal) { // Are we at (relatively) normal operating temps?
                unsigned long currentMillis = millis();
                if(currentMillis - previousMillisSol >= solenoidFinalInterval) {
                    previousMillisSol = currentMillis;
                    digitalWrite(solenoidPin, !digitalRead(solenoidPin)); // run the solenoid into the state we've just inverted it to.
                    return;
                } else { // If we pass the temp check but fail the timer check, we're here too quick.
                    return;
                }
            } else if(tempSensor < tempWarning) { // If we failed the room temp check, are we beneath the shutoff threshold?
                if(digitalRead(solenoidPin)) {    // Is the valve being pulled now?
                    unsigned long currentMillis = millis();           // If so, we should release it on the shorter timer.
                    if(currentMillis - previousMillisSol >= solenoidFinalInterval) {
                        previousMillisSol = currentMillis;
                        digitalWrite(solenoidPin, !digitalRead(solenoidPin)); // Flip, flop.
                        return;
                    } else { // OR, Passed the temp check, STILL here too quick.
                        return;
                    }
                } else { // The solenoid's probably off, not on right now. So that means we should wait a bit longer to fire again.
                    unsigned long currentMillis = millis();
                    if(currentMillis - previousMillisSol >= solenoidWarningInterval) { // We're keeping it low for a bit longer, to keep temps stable. Try to give it a bit of time to cool down before we go again.
                        previousMillisSol = currentMillis;
                        digitalWrite(solenoidPin, !digitalRead(solenoidPin));
                        return;
                    } else { // OR, We passed the temp check but STILL got here too quick.
                        return;
                    }
                }
            } else { // Failed both temp checks, so obviously it's not safe to fire.
                #ifdef PRINT_VERBOSE
                    Serial.println("Solenoid over safety threshold; not activating!");
                #endif
                digitalWrite(solenoidPin, LOW);                       // Make sure it's off if we're this dangerously close to the sun.
                return;
            }
        } else { // No temp sensor, so just go ahead.
            unsigned long currentMillis = millis();                   // Start the timer.
            if(currentMillis - previousMillisSol >= solenoidFinalInterval) { // If we've waited long enough for this interval,
                previousMillisSol = currentMillis;                    // Since we've waited long enough, calibrate the timer
                digitalWrite(solenoidPin, !digitalRead(solenoidPin)); // run the solenoid into the state we've just inverted it to.
                return;                                               // Aaaand we're done here.
            } else {                                                  // If we failed the timer check, we're here too quick.
                return;                                               // Get out of here, speedy mc loserpants.
            }
        }
    #else                                                         // *The shorter version of this function if we're not using a temp sensor.
        unsigned long currentMillis = millis();                   // Start the timer.
        if(currentMillis - previousMillisSol >= solenoidFinalInterval) { // If we've waited long enough for this interval,
            previousMillisSol = currentMillis;                    // Since we've waited long enough, calibrate the timer
            digitalWrite(solenoidPin, !digitalRead(solenoidPin)); // run the solenoid into the state we've just inverted it to.
            return;                                               // Aaaand we're done here.
        } else {                                                  // If we failed the timer check, we're here too quick.
            return;                                               // Get out of here, speedy mc loserpants.
        }
    #endif
}
#endif // USES_SOLENOID

#ifdef USES_RUMBLE
// Subroutine managing rumble state
void RumbleActivation()
{
    if(rumbleHappening) {                                         // Are we in a rumble command rn?
        unsigned long currentMillis = millis();                   // Calibrate a timer to set how long we've been rumbling.
        if(currentMillis - previousMillisRumble >= rumbleInterval) { // If we've been waiting long enough for this whole rumble command,
            digitalWrite(rumblePin, LOW);                         // Make sure the rumble is OFF.
            rumbleHappening = false;                              // This rumble command is done now.
            rumbleHappened = true;                                // And just to make sure, to prevent holding == repeat rumble commands.
        }
        return;                                                   // Alright we done here (if we did this before already)
    } else {                                                      // OR, we're rumbling for the first time.
        previousMillisRumble = millis();                          // Mark this as the start of this rumble command.
        analogWrite(rumblePin, rumbleIntensity);                  // Set the motor on.
        rumbleHappening = true;                                   // Mark that we're in a rumble command rn.
        return;                                                   // Now geddoutta here.
    }
}
#endif // USES_RUMBLE

// Solenoid 3-shot burst firing subroutine
void BurstFire()
{
    if(burstFireCount < 4) {  // Are we within the three shots alotted to a burst fire command?
        #ifdef USES_SOLENOID
            if(!digitalRead(solenoidPin) &&  // Is the solenoid NOT on right now, and the counter hasn't matched?
            (burstFireCount == burstFireCountLast)) {
                burstFireCount++;                                 // Increment the counter.
            }
            if(!digitalRead(solenoidPin)) {  // Now, is the solenoid NOT on right now?
                SolenoidActivation(solenoidFastInterval * 2);     // Hold it off a bit longer,
            } else {                         // or if it IS on,
                burstFireCountLast = burstFireCount;              // sync the counters since we completed one bullet cycle,
                SolenoidActivation(solenoidFastInterval);         // And start trying to activate the dingus.
            }
        #endif // USES_SOLENOID
        return;
    } else {  // If we're at three bullets fired,
        burstFiring = false;                                      // Disable the currently firing tag,
        burstFireCount = 0;                                       // And set the count off.
        return;                                                   // Let's go back.
    }
}

// Updates the button array with new pin mappings and control bindings, if any.
void UpdateBindings(bool offscreenEnable)
{
    // Updates pins
    // TODO: might still need to use pointers to the pins in the first place instead of lowkey memcopying,
    // but at least this is less aneurysm-inducing than what it used to be.
    LightgunButtons::ButtonDesc[BtnIdx_Trigger].pin = btnTrigger;
    LightgunButtons::ButtonDesc[BtnIdx_A].pin = btnGunA;
    LightgunButtons::ButtonDesc[BtnIdx_B].pin = btnGunB;
    LightgunButtons::ButtonDesc[BtnIdx_Calibrate].pin = btnCalibrate;
    LightgunButtons::ButtonDesc[BtnIdx_Start].pin = btnStart;
    LightgunButtons::ButtonDesc[BtnIdx_Select].pin = btnSelect;
    LightgunButtons::ButtonDesc[BtnIdx_Up].pin = btnGunUp;
    LightgunButtons::ButtonDesc[BtnIdx_Down].pin = btnGunDown;
    LightgunButtons::ButtonDesc[BtnIdx_Left].pin = btnGunLeft;
    LightgunButtons::ButtonDesc[BtnIdx_Right].pin = btnGunRight;
    LightgunButtons::ButtonDesc[BtnIdx_Pedal].pin = btnPedal;
    LightgunButtons::ButtonDesc[BtnIdx_Pump].pin = btnPump;
    LightgunButtons::ButtonDesc[BtnIdx_Home].pin = btnHome;

    // Updates button functions for low-button mode
    if(offscreenEnable) {
        LightgunButtons::ButtonDesc[1].reportType2 = LightgunButtons::ReportType_Keyboard;
        LightgunButtons::ButtonDesc[1].reportCode2 = playerStartBtn;
        LightgunButtons::ButtonDesc[2].reportType2 = LightgunButtons::ReportType_Keyboard;
        LightgunButtons::ButtonDesc[2].reportCode2 = playerSelectBtn;
        LightgunButtons::ButtonDesc[3].reportCode = playerStartBtn;
        LightgunButtons::ButtonDesc[4].reportCode = playerSelectBtn;
    } else {
        LightgunButtons::ButtonDesc[1].reportType2 = LightgunButtons::ReportType_Mouse;
        LightgunButtons::ButtonDesc[1].reportCode2 = MOUSE_RIGHT;
        LightgunButtons::ButtonDesc[2].reportType2 = LightgunButtons::ReportType_Mouse;
        LightgunButtons::ButtonDesc[2].reportCode2 = MOUSE_MIDDLE;
        LightgunButtons::ButtonDesc[3].reportCode = playerStartBtn;
        LightgunButtons::ButtonDesc[4].reportCode = playerSelectBtn;
    }
}

#ifdef DEBUG_SERIAL
void PrintDebugSerial()
{
    // only print every second
    if(millis() - serialDbMs >= 1000 && Serial.dtr()) {
#ifdef EXTRA_POS_GLITCH_FILTER
        Serial.print("bad final count ");
        Serial.print(badFinalCount);
        Serial.print(", bad move count ");
        Serial.println(badMoveCount);
#endif // EXTRA_POS_GLITCH_FILTER
        Serial.print("mode ");
        Serial.print(gunMode);
        Serial.print(", IR pos fps ");
        Serial.print(irPosCount);
        Serial.print(", loop/sec ");
        Serial.print(frameCount);

        Serial.print(", Mouse X,Y ");
        Serial.print(conMoveXAxis);
        Serial.print(",");
        Serial.println(conMoveYAxis);
        
        frameCount = 0;
        irPosCount = 0;
        serialDbMs = millis();
    }
}
#endif // DEBUG_SERIAL