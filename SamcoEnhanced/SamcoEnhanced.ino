/*!
 * @file SamcoEnhanced.ino
 * @brief SAMCO Enhanced +(plus) - 4IR LED Lightgun sketch w/ support for Rumble motor, Solenoid force feedback,
 * and hardware switches.
 * Based on Prow's Enhanced Fork from https://github.com/Prow7/ir-light-gun,
 * Which in itself is based on the 4IR Beta "Big Code Update" SAMCO project from https://github.com/samuelballantyne/IR-Light-Gun
 *
 * @copyright Samco, https://github.com/samuelballantyne, June 2020
 * @copyright Mike Lynch, July 2021
 * @copyright That One Seong, https://github.com/SeongGino, October 2023
 * @copyright GNU Lesser General Public License
 *
 * @author [Sam Ballantyne](samuelballantyne@hotmail.com)
 * @author Mike Lynch
 * @author [That One Seong](SeongsSeongs@gmail.com)
 * @version V1.1
 * @date 2023
 */

 /* Default button assignments
 *
 *  Reload (Button C) + Start will send an Esc keypress.
 *  Reload (Button C) + Select will enter Pause mode.
 * 
 *  In Pause mode:
 *  A, B, Start, Select: select a profile
 *  Start + Down: Normal gun mode (averaging disabled)
 *  Start + Up: Normal gun with averaging, toggles between the 2 averaging modes (use serial monitor to see the setting)
 *  Start + A: Processing mode for use with the Processing sketch
 *  B + Down: Decrease IR camera sensitivity (use serial monitor to see the setting)
 *  B + Up: Increase IR camera sensitivity (use serial monitor to see the setting)
 *  Reload: Exit pause mode
 *  Trigger: Begin calibration
 *  Start + Select: save settings to non-volatile memory
 *
 *  Note that the buttons in pause mode (and to enter pause mode) activate when the last button of
 *  the comination releases.
 *  This is used to detect and differentiate button combinations vs a single button press.
*/

 /* HOW TO CALIBRATE:
 *  
 *  Note: Prow renamed "offset" from the original sketch to "scale" which is a better term for what the setting is.
 *  The center calibration step determines the offset compensation required for accurate positioning.
 * 
 *  Step 1: Press Reload + Select to enter pause mode.
 *          Optional: Press a button to select a profile: A, B, Start, or Select
 *  Step 2: Pull Trigger to begin calibration.
 *  Step 3: Shoot cursor at center of the Screen and hold the trigger down for 1/3 of a second.
 *  Step 4: Mouse should lock to vertical axis. Use A/B buttons (can be held down) buttons to adjust mouse vertical
 *          range. A will increase, B will decrease. Track the top and bottom edges of the screen while adjusting.
 *  Step 5: Pull Trigger
 *  Step 6: Mouse should lock to horizontal axis. Use A/B buttons (can be held down) to adjust mouse horizontal
 *          range. A will increase, B will decrease. Track the left and right edges of the screen while adjusting.
 *  Step 7: Pull Trigger to finish and return to run mode. Values will apply to the selected profile.
 *  Step 8: Recommended: Confirm calibration is good. Enter pause mode and press Start and Select
 *          to write calibration to non-volatile memory.
 *  Step 9: Optional: Open serial monitor and update xCenter, yCenter, xScale & yScale values in the
 *          profile data array below.
 * 
 *  Calibration can be cancelled (return to pause mode) during any step by pressing Reload or Start or Select.
*/

#include <Arduino.h>
#include <SamcoBoard.h>

// include TinyUSB or HID depending on USB stack option
#if defined(USE_TINYUSB)
#include <Adafruit_TinyUSB.h>
#elif defined(CFG_TUSB_MCU)
#error Incompatible USB stack. Use Arduino or Adafruit TinyUSB.
#else
// Arduino USB stack
#include <HID.h>
#endif

#include <Keyboard.h>
#include <Wire.h>
#ifdef DOTSTAR_ENABLE
#include <Adafruit_DotStar.h>
#endif // DOTSTAR_ENABLE
#ifdef NEOPIXEL_PIN
#include <Adafruit_NeoPixel.h>
#endif
#ifdef SAMCO_FLASH_ENABLE
#include <Adafruit_SPIFlashBase.h>
#endif // SAMCO_FLASH_ENABLE
#include <AbsMouse5.h>
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

// enable extra serial debug during run mode
//#define PRINT_VERBOSE 1
//#define DEBUG_SERIAL 1
//#define DEBUG_SERIAL 2
//#define DEBUG_SOL 1         // used this to test solenoid without cam. fucking EMI problems

// extra position glitch filtering, 
// not required after discoverving the DFRobotIRPositionEx atomic read technique
//#define EXTRA_POS_GLITCH_FILTER

    // IMPORTANT ADDITIONS HERE ------------------------------------------------------------------------------- (*****THE HARDWARE SETTINGS YOU WANNA TWEAK!*****)
  // Set this to 1 if your build uses hardware switches, or comment out (//) to only set at boot time.
#define USES_SWITCHES 1
#ifdef USES_SWITCHES // If your build uses hardware switches,
    // Here's where they should be defined!
    const byte autofireSwitch = 18;                   // What's the pin number of the autofire switch? Digital.
    const byte rumbleSwitch = 19;                     // What's the pin number of the rumble switch? Digital.
    const byte solenoidSwitch = 20;                   // What's the pin number of the solenoid switch? Digital.
#endif

  // Set this to 1 if your build uses a TMP36 temperature sensor for a solenoid, or comment out if your solenoid doesn't need babysitting.
//#define USES_TEMP 1
#ifdef USES_TEMP    
    const byte tempPin = A0;                          // What's the pin number of the temp sensor? Needs to be analog.
    const byte tempNormal = 50;                       // Solenoid: Anything below this value is "normal" operating temperature for the solenoid, in Celsius.
    const byte tempWarning = 60;                      // Solenoid: Above normal temps, this is the value up to where we throttle solenoid activation, in Celsius.
#endif                                                // **Anything above ^this^ is considered too dangerous, will disallow any further engagement.


  // Which extras should be activated? Set here if your build doesn't use toggle switches.
bool solenoidActivated = false;                       // Are we allowed to use a solenoid? Default to off.
bool autofireActivated = false;                       // Is solenoid firing in autofire (rapid) mode? false = default single shot, true = autofire
bool rumbleActivated = false;                         // Are we allowed to do rumble? Default to off.


  // Pins setup - where do things be plugged into like? Uses GPIO codes ONLY! See also: https://learn.adafruit.com/adafruit-itsybitsy-rp2040/pinouts
const byte rumblePin = 24;                            // What's the pin number of the rumble output? Needs to be digital.
const byte solenoidPin = 25;                          // What's the pin number of the solenoid output? Needs to be digital.
const byte btnTrigger = 6;                            // Programmer's note: made this just to simplify the trigger pull detection, guh.
const byte btnGunA = 7;                               // <-- GCon 1-spec
const byte btnGunB = 8;                               // <-- GCon 1-spec
const byte btnGunC = 9;                               // Everything below are for GCon 2-spec only 
const byte btnStart = 10;
const byte btnSelect = 11;
const byte btnGunUp = 1;
const byte btnGunDown = 0;
const byte btnGunLeft = 4;
const byte btnGunRight = 5;
const byte btnPedal = 12;                             // If you're using a physical Time Crisis-style pedal, this is for that.


  // Adjustable aspects:
const byte rumbleMotorIntensity = 0;                  // This is actually inverted; 0 is 100%, 1 is roughly half power.
const unsigned int rumbleInterval = 110;              // How long to wait for the whole rumble command, in ms.
const unsigned int solenoidNormalInterval = 55;       // Interval for solenoid activation, in ms.
const unsigned int solenoidFastInterval = 40;         // Interval for faster solenoid activation, in ms.
const unsigned int solenoidLongInterval = 500;        // for single shot, how long to wait until we start spamming the solenoid? In ms.

//--------------------------------------------------------------------------------------------------------------------------------------
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
    BtnIdx_Reload,
    BtnIdx_Pedal
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
    BtnMask_Reload = 1 << BtnIdx_Reload,
    BtnMask_Pedal = 1 << BtnIdx_Pedal
};

// Button descriptor
// The order of the buttons is the order of the button bitmask
// must match ButtonIndex_e order, and the named bitmask values for each button
// see LightgunButtons::Desc_t, format is: 
// {pin, report type, report code (ignored for internal), debounce time, debounce mask, label}
const LightgunButtons::Desc_t LightgunButtons::ButtonDesc[] = {
    {btnTrigger, LightgunButtons::ReportType_Mouse, MOUSE_LEFT, 20, BTN_AG_MASK, "Trigger"},
    {btnGunA, LightgunButtons::ReportType_Mouse, MOUSE_RIGHT, 20, BTN_AG_MASK2, "A"},
    {btnGunB, LightgunButtons::ReportType_Mouse, MOUSE_MIDDLE, 20, BTN_AG_MASK2, "B"},
    {btnStart, LightgunButtons::ReportType_Keyboard, '1', 35, BTN_AG_MASK2, "Start"},
    {btnSelect, LightgunButtons::ReportType_Keyboard, '5', 35, BTN_AG_MASK2, "Select"},
    {btnGunUp, LightgunButtons::ReportType_Keyboard, KEY_UP_ARROW, 35, BTN_AG_MASK2, "Up"},
    {btnGunDown, LightgunButtons::ReportType_Keyboard, KEY_DOWN_ARROW, 35, BTN_AG_MASK2, "Down"},
    {btnGunLeft, LightgunButtons::ReportType_Keyboard, KEY_LEFT_ARROW, 35, BTN_AG_MASK2, "Left"},
    {btnGunRight, LightgunButtons::ReportType_Keyboard, KEY_RIGHT_ARROW, 35, BTN_AG_MASK2, "Right"},
    {btnGunC, LightgunButtons::ReportType_Mouse, MOUSE_BUTTON4, 20, BTN_AG_MASK2, "Reload"},
    {btnPedal, LightgunButtons::ReportType_Mouse, MOUSE_BUTTON5, 20, BTN_AG_MASK2, "Pedal"}
};

// button count constant
constexpr unsigned int ButtonCount = sizeof(LightgunButtons::ButtonDesc) / sizeof(LightgunButtons::ButtonDesc[0]);

// button runtime data arrays
LightgunButtonsStatic<ButtonCount> lgbData;

// button object instance
LightgunButtons buttons(lgbData, ButtonCount);

/*
// WIP, some sort of generic button handler table for pause mode
// pause button function
typedef void (*PauseModeBtnFn_t)();

// pause mode function
typedef struct PauseModeFnEntry_s {
    uint32_t buttonMask;
    PauseModeBtnFn_t pfn;
} PauseModeFnEntry_t;
*/

// button combo to send an escape keypress
constexpr uint32_t EscapeKeyBtnMask = BtnMask_Reload | BtnMask_Start;

// button combo to enter pause mode
constexpr uint32_t EnterPauseModeBtnMask = BtnMask_Reload | BtnMask_Select;

// press any button to enter pause mode from Processing mode (this is not a button combo)
constexpr uint32_t EnterPauseModeProcessingBtnMask = BtnMask_A | BtnMask_B | BtnMask_Reload;

// button combo to exit pause mode back to run mode
constexpr uint32_t ExitPauseModeBtnMask = BtnMask_Reload;

// press any button to cancel the calibration (this is not a button combo)
constexpr uint32_t CancelCalBtnMask = BtnMask_Reload | BtnMask_Start | BtnMask_Select;

// button combo to skip the center calibration step
constexpr uint32_t SkipCalCenterBtnMask = BtnMask_A;

// button combo to save preferences to non-volatile memory
constexpr uint32_t SaveBtnMask = BtnMask_Start | BtnMask_Select;

// button combo to increase IR sensitivity
constexpr uint32_t IRSensitivityUpBtnMask = BtnMask_B | BtnMask_Up;

// button combo to decrease IR sensitivity
constexpr uint32_t IRSensitivityDownBtnMask = BtnMask_B | BtnMask_Down;

// button combinations to select a run mode
constexpr uint32_t RunModeNormalBtnMask = BtnMask_Start | BtnMask_A;
constexpr uint32_t RunModeAverageBtnMask = BtnMask_Start | BtnMask_B;
constexpr uint32_t RunModeProcessingBtnMask = BtnMask_Start | BtnMask_Right;

// colour when no IR points are seen
constexpr uint32_t IRSeen0Color = WikiColor::Amber;

// colour when calibrating
constexpr uint32_t CalModeColor = WikiColor::Red;

// number of profiles
constexpr unsigned int ProfileCount = 4;

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

// profiles ---------------------------------------------------------------------------------------------- (*****THE LAST THING YOU'LL WANT TO ADJUST IS HERE!*****)
// defaults can be populated here, or not worry about these values and just save to flash/EEPROM
// if you have original Samco calibration values, multiply by 4 for the center position and
// scale is multiplied by 1000 and stored as an unsigned integer, see SamcoPreferences::Calibration_t
SamcoPreferences::ProfileData_t profileData[ProfileCount] = {
    {1605, 935, 2137, 1522, DFRobotIRPositionEx::Sensitivity_Default, RunMode_Average, 0, 0},
    {1695, 957, 2121, 1981, DFRobotIRPositionEx::Sensitivity_Default, RunMode_Average, 0, 0},
    {1723, 890, 2073, 2036, DFRobotIRPositionEx::Sensitivity_Default, RunMode_Average, 0, 0},
    {1683, 948, 2130, 2053, DFRobotIRPositionEx::Sensitivity_Default, RunMode_Average, 0, 0}
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
    {BtnMask_A, WikiColor::Cerulean_blue, "A", "TV Fisheye Lens"},
    {BtnMask_B, WikiColor::Cornflower_blue, "B", "TV Wide-angle Lens"},
    {BtnMask_Start, WikiColor::Green, "Start", "TV"},
    {BtnMask_Select, WikiColor::Green_Lizard, "Select", "Monitor"}
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
// Boring values for the solenoid timing stuff:
unsigned long previousMillisSol = 0;             // our timer (holds the last time since a successful interval pass)
bool solenoidFirstShot = false;                  // default to off, but actually set this the first time we shoot.
#ifdef USES_TEMP
    int tempSensor;                              // Temp sensor changes over time, so just initialize the variable here ig.
    const unsigned int solenoidWarningInterval = solenoidFastInterval * 3; // for if solenoid is getting toasty.
#endif

// For autofire:
bool triggerHeld = false;                        // Trigger SHOULDN'T be being pulled by default, right?

// For rumble:
unsigned long previousMillisRumble = 0;          // our time for the rumble motor (two different timers? oh my!)
unsigned long previousMillisRumbTot = 0;         // our time for each rumble command (yep, that's three timers now! joy)
bool rumbleHappening = false;                    // To keep track on if this is a rumble command or not.
bool rumbleHappened = false;                     // If we're holding, this marks we sent a rumble command already.
// We need the rumbleHappening because we can be in a rumble command without the rumble state being on (emulating variable motor force)

unsigned int lastSeen = 0;

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
#ifdef ARDUINO_ADAFRUIT_ITSYBITSY_RP2040
DFRobotIRPositionEx dfrIRPos(Wire1);
#else
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
    GunMode_Pause = 4
};
GunMode_e gunMode = GunMode_Init;   // initial mode

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


#ifdef DOTSTAR_ENABLE
// note if the colours don't match then change the colour format from BGR
// apparently different lots of DotStars may have different colour ordering ¯\_(ツ)_/¯
Adafruit_DotStar dotstar(1, DOTSTAR_DATAPIN, DOTSTAR_CLOCKPIN, DOTSTAR_BGR);
#endif // DOTSTAR_ENABLE

#ifdef NEOPIXEL_PIN
Adafruit_NeoPixel neopixel(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
#endif // NEOPIXEL_PIN

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
#define SAMCO_NO_HW_TIMER_UPDATE()
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

// USB HID Report ID
enum HID_RID_e{
  HID_RID_KEYBOARD = 1,
  HID_RID_MOUSE
};

// HID report descriptor using TinyUSB's template
uint8_t const desc_hid_report[] = {
  TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_RID_KEYBOARD)),
  TUD_HID_REPORT_DESC_ABSMOUSE5(HID_REPORT_ID(HID_RID_MOUSE))
};

Adafruit_USBD_HID usbHid;

int __USBGetKeyboardReportID()
{
    return HID_RID_KEYBOARD;
}

// AbsMouse5 instance
AbsMouse5_ AbsMouse5(HID_RID_MOUSE);

#else
// AbsMouse5 instance
AbsMouse5_ AbsMouse5(1);
#endif

void setup() {
    // ADDITIONS HERE
    pinMode(rumblePin, OUTPUT);
    pinMode(solenoidPin, OUTPUT);
    #ifdef USES_SWITCHES
        pinMode(autofireSwitch, INPUT_PULLUP);
        pinMode(rumbleSwitch, INPUT_PULLUP);
        pinMode(solenoidSwitch, INPUT_PULLUP);
    #endif

    // init DotStar and/or NeoPixel to red during setup()
#ifdef DOTSTAR_ENABLE
    dotstar.begin();
    dotstar.setPixelColor(0, 150, 0, 0);
    dotstar.show();
#endif // DOTSTAR_ENABLE
#ifdef NEOPIXEL_ENABLEPIN
    pinMode(NEOPIXEL_ENABLEPIN, OUTPUT);
    digitalWrite(NEOPIXEL_ENABLEPIN, HIGH);
#endif // NEOPIXEL_ENABLEPIN
#ifdef NEOPIXEL_PIN
    neopixel.begin();
    neopixel.setPixelColor(0, 255, 0, 0);
    neopixel.show();
#endif // NEOPIXEL_PIN

#ifdef ARDUINO_ADAFRUIT_ITSYBITSY_RP2040
    // ensure Wire1 SDA and SCL are correct
    Wire1.setSDA(2);
    Wire1.setSCL(3);
#endif

    // initialize buttons
    buttons.Begin();

#ifdef SAMCO_FLASH_ENABLE
    // init flash and load saved preferences
    nvAvailable = flash.begin();
#endif // SAMCO_FLASH_ENABLE
    
    if(nvAvailable) {
        LoadPreferences();
    }

    // use values from preferences
    ApplyInitialPrefs();

    // Start IR Camera with basic data format
    dfrIRPos.begin(DFROBOT_IR_IIC_CLOCK, DFRobotIRPositionEx::DataFormat_Basic, irSensitivity);
    
#ifdef USE_TINYUSB
    usbHid.setPollInterval(2);
    usbHid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
    //usb_hid.setStringDescriptor("TinyUSB HID Composite");

    usbHid.begin();
#endif

    Serial.begin(115200);
    
    AbsMouse5.init(MouseMaxX, MouseMaxY, true);
   
    // sanity to ensure the cal prefs is populated with at least 1 entry
    // in case the table is zero'd out
    if(profileData[selectedProfile].xCenter == 0) {
        profileData[selectedProfile].xCenter = xCenter;
    }
    if(profileData[selectedProfile].yCenter == 0) {
        profileData[selectedProfile].yCenter = yCenter;
    }
    if(profileData[selectedProfile].xScale == 0) {
        profileData[selectedProfile].xScale = CalScaleFloatToPref(xScale);
    }
    if(profileData[selectedProfile].yScale == 0) {
        profileData[selectedProfile].yScale = CalScaleFloatToPref(yScale);
    }
    
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

    // this will turn off the DotStar/RGB LED and ensure proper transition to Run
    SetMode(GunMode_Run);
}

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

void loop() {
    SAMCO_NO_HW_TIMER_UPDATE();
    
    // poll/update button states with 1ms interval so debounce mask is more effective
    buttons.Poll(1);
    buttons.Repeat();

    switch(gunMode) {
        case GunMode_Pause:
            if(buttons.pressedReleased == ExitPauseModeBtnMask) {
                SetMode(GunMode_Run);
            } else if(buttons.pressedReleased == BtnMask_Trigger) {
                SetMode(GunMode_CalCenter);
            } else if(buttons.pressedReleased == RunModeNormalBtnMask) {
                SetRunMode(RunMode_Normal);
            } else if(buttons.pressedReleased == RunModeAverageBtnMask) {
                SetRunMode(runMode == RunMode_Average ? RunMode_Average2 : RunMode_Average);
            } else if(buttons.pressedReleased == RunModeProcessingBtnMask) {
                SetRunMode(RunMode_Processing);
            } else if(buttons.pressedReleased == IRSensitivityUpBtnMask) {
                IncreaseIrSensitivity();
            } else if(buttons.pressedReleased == IRSensitivityDownBtnMask) {
                DecreaseIrSensitivity();
            } else if(buttons.pressedReleased == SaveBtnMask) {
                SavePreferences();
            } else {
                SelectCalProfileFromBtnMask(buttons.pressedReleased);
            }

            PrintResults();
            
            break;
        case GunMode_CalCenter:
            AbsMouse5.move(MouseMaxX / 2, MouseMaxY / 2);
            if(buttons.pressedReleased & CancelCalBtnMask) {
                CancelCalibration();
            } else if(buttons.pressedReleased == SkipCalCenterBtnMask) {
                Serial.println("Calibrate Center skipped");
                SetMode(GunMode_CalVert);
            } else if(buttons.pressed & BtnMask_Trigger) {
                // trigger pressed, begin center cal 
                CalCenter();
                // extra delay to wait for trigger to release (though not required)
                SetModeWaitNoButtons(GunMode_CalVert, 500);
            }
            break;
        case GunMode_CalVert:
            if(buttons.pressedReleased & CancelCalBtnMask) {
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
            if(buttons.pressedReleased & CancelCalBtnMask) {
                CancelCalibration();
            } else {
                if(buttons.pressed & BtnMask_Trigger) {
                    ApplyCalToProfile();
                    SetMode(GunMode_Run);
                } else {
                    CalHoriz();
                }
            }
            break;
        default:
            /* ---------------------- LET'S GO --------------------------- */
            switch(runMode) {
            case RunMode_Processing:
                ExecRunModeProcessing();
                break;
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

void ExecRunMode()
{
#ifdef DEBUG_SERIAL
    Serial.print("exec run mode ");
    Serial.println(RunModeLabels[runMode]);
#endif
    moveIndex = 0;
    buttons.ReportEnable();
    for(;;) {
        // ADDITIONS HERE: setting the state of our toggles, if used.
        #ifdef USES_SWITCHES
            rumbleActivated = !digitalRead(rumbleSwitch);
            solenoidActivated = !digitalRead(solenoidSwitch);
            autofireActivated = !digitalRead(autofireSwitch);
            #ifdef PRINT_VERBOSE
                //Serial.print("Rumble state: ");
                //Serial.println(rumbleActivated);
                //Serial.print("Solenoid state: ");
                //Serial.println(solenoidActivated);
                Serial.print("Autofire state: ");
                Serial.println(autofireActivated);
            #endif
        #endif
        buttons.Poll(0);

        SAMCO_NO_HW_TIMER_UPDATE();
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
            
#ifdef DEBUG_SERIAL
            ++irPosCount;
#endif // DEBUG_SERIAL
        }
        // ADDITIONS HERE: The main additions are here.
        #ifdef DEBUG_SOL // I fucking hate EMI...
            if(buttons.pressed == BtnMask_Trigger) {                  // We're using this to fire off the solenoid when the trigger is pulled, regardless of camera.
                SolenoidActivation(solenoidNormalInterval);           // We're just gonna go right to shootin that thang.
            } else {
                if(digitalRead(solenoidPin)) {                        // Has the solenoid remain engaged this cycle?
                    unsigned long currentMillis = millis();           // Start the timer
                    if(currentMillis - previousMillisSol >= solenoidFastInterval) { // I guess if we're not firing, may as well use the fastest shutoff.
                        previousMillisSol = currentMillis;            // Timer calibration, yawn.
                        digitalWrite(solenoidPin, LOW);               // Make sure to turn it off.
                    }
                }
            }
        #else
        if(buttons.debounced == BtnMask_Trigger) {                  // Check if we pressed the Trigger this run.
            if((conMoveYAxis > 0 && conMoveYAxis < MouseMaxY) && (conMoveXAxis > 0 && conMoveXAxis < MouseMaxX)) { // Check if the X or Y axis is in the screen's boundaries, i.e. "off screen".
                if(solenoidActivated) {                             // (Only activate when the solenoid switch is on!)
                    if(!triggerHeld) {                              // If this is the first time we're firing,
                        solenoidFirstShot = true;                   // Since we're not in an auto mode, set the First Shot flag on.
                        SolenoidActivation(0);                      // Just activate the Solenoid already!
                        if(autofireActivated) {                     // If we are in auto mode,
                            solenoidFirstShot = false;              // Immediately set this bit off!
                        }
                    } else if(autofireActivated) {                  // Else, if we've been holding the trigger, is the autofire switch active?
                        if(digitalRead(solenoidPin)) {              // Is the solenoid engaged?
                            SolenoidActivation(solenoidFastInterval); // If so, immediately pass the autofire faster interval to solenoid method
                        } else {                                    // Or if it's not,
                            SolenoidActivation(solenoidFastInterval * 2); // We're holding it for longer.
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
                    } else {                                        // if we don't have the single shot wait flag on (holding the trigger w/out autofire)
                        if(digitalRead(solenoidPin)) {              // Are we engaged right now?
                            SolenoidActivation(solenoidNormalInterval); // Turn it off with this timer.
                        } else {                                    // Or we're not engaged.
                            SolenoidActivation(solenoidNormalInterval * 2); // So hold it that way for twice the normal timer.
                        }
                    }
                } else {
                    #ifdef PRINT_VERBOSE
                        Serial.println("Solenoid not allowed to go off; not reading, skipping!"); // We ain't using the solenoid, so just ignore all that.
                    #endif
                }
                if(rumbleActivated && rumbleHappening && triggerHeld) { // Is rumble activated, AND we're in a rumbling command WHILE the trigger's held?
                    RumbleActivation();                             // Continue processing the rumble command, to prevent infinite rumble while going from on-screen to off mid-command.
                }
            } else {                                                // We're shooting outside of the screen boundaries!
                #ifdef PRINT_VERBOSE
                    Serial.println("Shooting outside of the screen! RELOAD!");
                #endif
                if(rumbleActivated) {                               // Only activate if the rumble switch is enabled!
                    if(!rumbleHappened && !triggerHeld) {           // Is this the first time we're rumbling AND only started pulling the trigger (to prevent starting a rumble w/ trigger hold)?
                        RumbleActivation();                         // Start a rumble command.
                    } else if(rumbleHappening) {                    // We are currently processing a rumble command.
                        RumbleActivation();                         // Keep processing that command then.
                    }                                               // Else, we rumbled already, so don't do anything to prevent infinite rumbling.
                }
                if(digitalRead(solenoidPin)) {                      // If the solenoid is engaged, since we're not shooting the screen, shut off the solenoid a'la an idle cycle
                    unsigned long currentMillis = millis();         // Calibrate current time
                    if(currentMillis - previousMillisSol >= solenoidFastInterval) { // I guess if we're not firing, may as well use the fastest shutoff.
                        previousMillisSol = currentMillis;          // Timer calibration, yawn.
                        digitalWrite(solenoidPin, LOW);             // Turn it off already, dangit.
                    }
                }
            }
            triggerHeld = true;                                     // Signal that we've started pulling the trigger this poll cycle.
        } else {                                                    // ...Or we just didn't press the trigger this cycle.
            triggerHeld = false;                                    // Disable the holding function
            if(solenoidActivated) {                                 // Has the solenoid remain engaged this cycle?
                solenoidFirstShot = false;                          // Make sure this is unset to prevent "sticking" in single shot mode!
                unsigned long currentMillis = millis();             // Start the timer
                if(currentMillis - previousMillisSol >= solenoidFastInterval) { // I guess if we're not firing, may as well use the fastest shutoff.
                    previousMillisSol = currentMillis;              // Timer calibration, yawn.
                    digitalWrite(solenoidPin, LOW);                 // Make sure to turn it off.
                }
            }
            if(rumbleHappening) {                                   // Are we currently in a rumble command? (Implicitly needs rumbleActivated)
                RumbleActivation();                                 // Continue processing our rumble command.
                // (This is to prevent making the lack of trigger pull actually activate a rumble command instead of skipping it like we should.)
            } else if(rumbleHappened) {                             // If rumble has happened,
                rumbleHappened = false;                             // well we're clear now that we've stopped holding.
            }
        }
        #endif

        if(buttons.pressedReleased == EscapeKeyBtnMask) {
            Keyboard.releaseAll();                                  // Clear out keyboard keys (since Seong's default is pressing start, which is num1)
            delay(1);                                               // Wait a bit. Required for the escape keypress to actually register.
            Keyboard.press(KEY_ESC);                                // PUSH THE BUTTON.
            delay(100);                                             // Wait another bit. Required for the escape keypress to actually register, and mitigate sticking.
            Keyboard.release(KEY_ESC);                              // Aaaaand release.
        }

        if(buttons.pressedReleased == EnterPauseModeBtnMask) {
            // MAKE SURE SOLENOID IS OFF:
            digitalWrite(solenoidPin, LOW);
            SetMode(GunMode_Pause);
            buttons.ReportDisable();
            return;
        }


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
        if(buttons.pressedReleased & EnterPauseModeProcessingBtnMask) {
            SetMode(GunMode_Pause);
            return;
        }

        SAMCO_NO_HW_TIMER_UPDATE();
        if(irPosUpdateTick) {
            irPosUpdateTick = 0;
        
            int error = dfrIRPos.basicAtomic(DFRobotIRPositionEx::Retry_2);
            if(error == DFRobotIRPositionEx::Error_Success) {
                mySamco.begin(dfrIRPos.xPositions(), dfrIRPos.yPositions(), dfrIRPos.seen(), MouseMaxX / 2, MouseMaxY / 2);
                UpdateLastSeen();
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
            } else if(error == DFRobotIRPositionEx::Error_IICerror) {
                Serial.println("Device not available!");
            }
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
    }
    
    if(buttons.repeat & BtnMask_B) {
        yScale = yScale + ScaleStep;
    }
    
    if(buttons.repeat & BtnMask_A) {
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
    }

    if(buttons.repeat & BtnMask_B) {
        xScale = xScale + ScaleStep;
    }
    
    if(buttons.repeat & BtnMask_A) {
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
void UpdateLastSeen() {
    if(lastSeen != mySamco.seen()) {
        if(!lastSeen && mySamco.seen()) {
            LedOff();
        } else if(lastSeen && !mySamco.seen()) {
            SetLedPackedColor(IRSeen0Color);
        }
        lastSeen = mySamco.seen();
    }
}

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
    }

    SetLedColorFromMode();
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
    }
        
    lastPrintMillis = millis();
}

void PrintCalInterval()
{
    if(millis() - lastPrintMillis < 100) {
        return;
    }
    PrintCal();
    lastPrintMillis = millis();
}

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

void PrintRunMode()
{
    if(runMode < RunMode_Count) {
        Serial.print("Mode: ");
        Serial.println(RunModeLabels[runMode]);
    }
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

void SavePreferences()
{
    if(!nvAvailable || !(stateFlags & StateFlag_SavePreferencesEn)) {
        return;
    }

    // Only allow one write per pause state until something changes.
    // Extra protection to ensure the same data can't write a bunch of times.
    stateFlags &= ~StateFlag_SavePreferencesEn;
    
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
    } else {
        Serial.println("Error saving Preferences.");
        PrintNVPrefsError();
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
    // return to pause mode
    SetMode(GunMode_Pause);
}

void PrintSelectedProfile()
{
    Serial.print("Profile: ");
    Serial.println(profileDesc[selectedProfile].profileLabel);
}

// select a profile
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

    SetLedColorFromMode();

    // enable save to allow setting new default profile
    stateFlags |= StateFlag_SavePreferencesEn;
    return valid;
}

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
}

void LedOff()
{
#ifdef DOTSTAR_ENABLE
    dotstar.setPixelColor(0, 0);
    dotstar.show();
#endif // DOTSTAR_ENABLE
#ifdef NEOPIXEL_PIN
    neopixel.setPixelColor(0, 0);
    neopixel.show();
#endif // NEOPIXEL_PIN
}

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

// ADDITIONS HERE:
void SolenoidActivation(int solenoidFinalInterval) {
    if(solenoidFirstShot) {                                       // If this is the first time we're shooting, it's probably safe to shoot regardless of temps.
        unsigned long currentMillis = millis();                   // Initialize timer.
        previousMillisSol = currentMillis;                        // Calibrate the timer for future calcs.
        digitalWrite(solenoidPin, HIGH);                          // Since we're shooting the first time, just turn it on aaaaand fire.
        return;                                                   // We're done here now.
    }
    #ifdef USES_TEMP                                              // *If the build calls for a TMP36 temperature sensor,
        tempSensor = analogRead(tempPin);                         // Read the temp sensor.
        tempSensor = (tempSensor * 0.32226563) + 0.5;             // Multiply for accurate Celsius reading from 3.3v signal. (rounded up)
        #ifdef PRINT_VERBOSE
            Serial.print("Current Temp near solenoid: ");
            Serial.print(tempSensor);
            Serial.println("*C");
        #endif
        if(tempSensor < tempNormal) {                             // Are we at (relatively) normal operating temps?
            unsigned long currentMillis = millis();               // Start the timer.
            if(currentMillis - previousMillisSol >= solenoidFinalInterval) { // If we've waited long enough for this interval,
                previousMillisSol = currentMillis;                // Since we've waited long enough, calibrate the timer
                digitalWrite(solenoidPin, !digitalRead(solenoidPin)); // run the solenoid into the state we've just inverted it to.
                return;                                           // Aaaand we're done here.
            } else {                                              // If we pass the temp check but fail the timer check, we're here too quick.
                return;                                           // Get out of here, speedy mc loserpants.
            }
        } else if(tempSensor < tempWarning) {                     // If we failed the room temp check, are we beneath the shutoff threshold?
            if(digitalRead(solenoidPin)) {                        // Is the valve being pulled now?
                unsigned long currentMillis = millis();           // If so, we should release it on the shorter timer.
                if(currentMillis - previousMillisSol >= solenoidFinalInterval) { // We're holding it high for the requested time.
                    previousMillisSol = currentMillis;            // Timer calibrate
                    digitalWrite(solenoidPin, !digitalRead(solenoidPin)); // Flip, flop.
                    return;                                       // Yay.
                } else {                                          // OR, Passed the temp check, STILL here too quick.
                    return;                                       // Yeeted.
                }
            } else {                                              // The solenoid's probably off, not on right now. So that means we should wait a bit longer to fire again.
                unsigned long currentMillis = millis();           // Timerrrrrr.
                if(currentMillis - previousMillisSol >= solenoidWarningInterval) { // We're keeping it low for a bit longer, to keep temps stable. Try to give it a bit of time to cool down before we go again.
                    previousMillisSol = currentMillis;            // Since we've waited long enough, calibrate the timer
                    digitalWrite(solenoidPin, !digitalRead(solenoidPin)); // run the solenoid into the state we've just inverted it to.
                    return;                                       // Doneso.
                } else {                                          // OR, We passed the temp check but STILL got here too quick.
                    return;                                       // Loser.
                }
            }
        } else {                                                  // Failed both temp checks, so obviously it's not safe to fire.
            #ifdef PRINT_VERBOSE
                Serial.println("Solenoid over safety threshold; not activating!");
            #endif
            digitalWrite(solenoidPin, LOW);                       // Make sure it's off if we're this dangerously close to the sun.
            return;                                               // Go away.
        }
    #else                                                     // *The shorter version of this function if we're not using a temp sensor.
        unsigned long currentMillis = millis();               // Start the timer.
        if(currentMillis - previousMillisSol >= solenoidFinalInterval) { // If we've waited long enough for this interval,
            previousMillisSol = currentMillis;                // Since we've waited long enough, calibrate the timer
            digitalWrite(solenoidPin, !digitalRead(solenoidPin)); // run the solenoid into the state we've just inverted it to.
            return;                                           // Aaaand we're done here.
        } else {                                              // If we failed the timer check, we're here too quick.
            return;                                           // Get out of here, speedy mc loserpants.
        }
    #endif
}

void RumbleActivation() {
    if(rumbleHappening) {                                     // Are we in a rumble command rn?
        if(rumbleMotorIntensity > 0) {                        // Are we using anything but full blast? If so, must be using a higher (lower) value, so let's temper that motor a bit
            if(digitalRead(rumblePin)) {                      // Is the motor on now? Must be, so let's flick it off now.
                unsigned long currentMillis = millis();       // Start the timer.
                if(currentMillis - previousMillisRumble >= rumbleMotorIntensity) { // If we've waited long enough for this interval,
                    previousMillisRumble = currentMillis;     // Since we've waited long enough, calibrate the timer
                    digitalWrite(rumblePin, !digitalRead(rumblePin)); // run the motor into an inverted state.
                }
            } else {                                          // I guess not, so let's flick it on for a set short interval.
                unsigned long currentMillis = millis();       // Start the timer.
                if(currentMillis - previousMillisRumble >= rumbleMotorIntensity * 1.2) { // If we've waited long enough for this interval,
                    previousMillisRumble = currentMillis;     // Since we've waited long enough, calibrate the timer
                    digitalWrite(rumblePin, !digitalRead(rumblePin)); // run the motor into the state we've just inverted it to.
                }
            }
        } // If the above didn't go off, guess we're at full blast, so just keep going.

        unsigned long currentMillis = millis();               // Now a second timer to check how long we've been rumbling.
        if(currentMillis - previousMillisRumbTot >= rumbleInterval) { // If we've been waiting long enough for this whole rumble command,
            digitalWrite(rumblePin, LOW);                     // Make sure the rumble is OFF.
            rumbleHappening = false;                          // This rumble command is done now.
            rumbleHappened = true;                            // And just to make sure, to prevent holding == repeat rumble commands.
            #ifdef PRINT_VERBOSE
                Serial.println("We stopped a rumblin'!");
            #endif
        }
        return;                                               // Alright we done here (if we did this before already)
    } else {                                                  // OR, we're rumbling for the first time.
        previousMillisRumbTot = millis();                     // Mark this as the start of this rumble command.
        digitalWrite(rumblePin, HIGH);                        // Make the rumble rumble.
        rumbleHappening = true;                               // Mark that we're in a rumble command rn.
        return;                                               // Now geddoutta here.
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
