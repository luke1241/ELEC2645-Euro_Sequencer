//Library / class inclusions
#include "mbed.h"
#include "N5110.h"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>
#include <string>
#include <map>
#include "Joystick.h"
#include "DebounceIn.h"
#include "Controller.h"

/* Definitions are used for constant values that will not change on run-time
 *  #define makes it easier to change value quickly in development
 */

//Pin definitions
#define CLOCK_PIN PC_12
#define STOP_PIN PC_0
#define RUN_PIN PC_1
#define EDIT_PIN PC_2
#define SETTINGS_PIN PC_3
#define CV_PIN PA_4
#define GATE_PIN PA_14
#define ACCENT_PIN PA_15

#define STATE_LED_R PC_5
#define STATE_LED_G PC_6
#define STATE_LED_B PC_8

#define JOYSTICK_X PA_0
#define JOYSTICK_Y PB_0
#define JOYSTICK_BTN PA_8

//Parameter definitions
#define MAX_SEQUENCE_LENGTH 64
#define DAC_SEMITONE 0.0252525252525
#define CALIBRATION_INC 0.01
#define MENU_WAIT_TIME 200ms

//Enum for states
enum class State {                                      //https://www.aleksandrhovhannisyan.com/blog/finite-state-machine-fsm-tutorial-implementing-an-fsm-in-c/
    ST_IDLE,
    ST_RUN,
    ST_EDIT,
    ST_SETTINGS
};

//Initialise state variable to IDLE
static State g_state = State::ST_IDLE;

//Enum of pitches, enables pitches to be reffered to by their musical names in code, rather than integer values
enum Pitch {
    c1,
    db1,
    d1,
    eb1,
    e1,
    f1,
    gb1,
    g1,
    ab1,
    a1,
    bb1,
    b1,
    c2,
    db2,
    d2,
    eb2,
    e2,
    f2,
    gb2,
    g2,
    ab2,
    a2,
    bb2,
    b2,
    c3,
    db3,
    d3,
    eb3,
    e3,
    f3,
    gb3,
    g3,
    ab3,
    a3,
    bb3,
    b3
};

//String array for converting boolean value into yes/no string output
std::string no_yes[2] = {"N", "Y"};

//Array of strings containing names of notes, which will be printed to the display
std::string pitchStrings[36] {
    "C1",
    "C#1",
    "D1",
    "D#1",
    "E1",
    "F1",
    "F#1",
    "G1",
    "G#1",
    "A1",
    "A#1",
    "B1",
    "C2",
    "C#2",
    "D2",
    "D#2",
    "E2",
    "F2",
    "F#2",
    "G2",
    "G#2",
    "A2",
    "A#2",
    "B2",
    "C3",
    "C#3",
    "D3",
    "D#3",
    "E3",
    "F3",
    "F#3",
    "G3",
    "G#3",
    "A3",
    "A#3",
    "B3"
};

//Step struct hold data for each step, used instead of class as it purely stores data
struct Step {
    Pitch pitch = Pitch::c1;
    bool rest = 0;
    bool accent = 0;
    bool hold = 0;
    bool glide = 0;
};

//Variable declarations for sequencer
Step sequence[MAX_SEQUENCE_LENGTH];
int sequenceLength = 16;
int gateLength = 20;
int accentMode = 1;
static int currStep = 0;

int calibrate = 0;
int reset = 0;



//Edit menu variables
int selectedItem = 0;

//Settings Menu Variables
int currentMenuItem = 0;
bool menuState = 0;

//Controller objects for settings menu
Controller sequenceLength_controller(sequenceLength, 8, 1, MAX_SEQUENCE_LENGTH, 1, "");
Controller gateLength_controller(gateLength, 20, 10, 200, 10, "ms");
Controller accentMode_controller(accentMode, 0, 0, 1, 1, "");
Controller calibrate_controller(calibrate, 0, 0, 0, 0, "");
Controller reset_controller(reset, 0, 0, 0, 0, "");

//Array of pairs containing setting controller object and corresponding 'name' to be displayed
std::pair<Controller, std::string> settings[5] = {
    make_pair(sequenceLength_controller, " No. Steps"),
    make_pair(gateLength_controller, " Gate Length"),
    make_pair(accentMode_controller, " Accent Mode"),
    make_pair(calibrate_controller, " Calibrate"),
    make_pair(reset_controller, " Reset")
};

//Interrupt object declarations
DebounceIn clockIn(CLOCK_PIN, PullDown);
InterruptIn stopBtn(STOP_PIN, PullNone);
InterruptIn runBtn(RUN_PIN, PullNone);
InterruptIn editBtn(EDIT_PIN, PullNone);
InterruptIn settingsBtn(SETTINGS_PIN, PullNone);

//Output object declarations
AnalogOut cvOut(CV_PIN);
DigitalOut gateOut(GATE_PIN);
DigitalOut accentOut(ACCENT_PIN);
BusOut stateLED(STATE_LED_R, STATE_LED_G, STATE_LED_B);


//LCD declaration
N5110 lcd(PC_7, PA_9, PB_10, PB_5, PB_3, PA_10);
char lcdBuffer[14] = {0};

//Joystick declaration
Joystick joystick(JOYSTICK_Y, JOYSTICK_X);
Direction joystickDir;
DigitalIn joystick_btn(JOYSTICK_BTN, PullUp);

//Global flags
volatile int g_clock_flag = 0;
volatile int g_run_flag = 0;


//ISR declarations
void stop_isr();
void run_isr();
void edit_isr();
void settings_isr();
void clock_isr();

//Function declarations
void init_sequence();

void reset_sequencer();
void calibrate_sequencer();

void idle_state();
void run_state();
void edit_state();
void settings_state();


//LCD Sprites, stored as 2D arrays
const int Pause[37][42] {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

const int Play[37][42] {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    
};

int main() {
    
    
    stopBtn.rise(&stop_isr);                                        //Linking interrupt pins to correct ISR functions
    runBtn.rise(&run_isr);
    editBtn.rise(&edit_isr);
    settingsBtn.rise(&settings_isr);
    clockIn.fall(&clock_isr, 1ms);
    
    
    joystick.init();                                                //Initialising the joystick
    
    
    cvOut = 0.0;                                                    //Initialise cv and gate voltage outputs to 0
    gateOut = 0.0;

    init_sequence();                                                //Populates first 16 steps of sequence 

    
    lcd.init(LPH7366_1);                                            //LCD Initialisation
    lcd.setContrast(0.5);                                           //Set contrast of LCD
    lcd.setBrightness(0.5);                                         //Set brightness of LCD

    lcd.clear();                                                    //Clear LCD

    
    lcd.printString(" ============ ", 0, 1);                        //Display splash screen
    lcd.printString(" EURO SEQ ", 0, 2);
    lcd.printString(" ============ ", 0, 3);
    lcd.refresh();
    ThisThread::sleep_for(1s);

    while (true) {
        switch(g_state){                                            //Switch based on current state (forms basis of FSM)
            case State::ST_RUN:                                         //RUN state
                run_state();                                                //Each state is contained in its own function to keep code tidy
                break;
            case State::ST_EDIT:                                        //EDIT state
                edit_state();
                break;
            case State::ST_SETTINGS:                                    //SETTINGS state
                settings_state();
                break;
            default:                                                    //IDLE state
                idle_state();
                break;
        }
        


        
        sleep();
    }
}


//ISR function definitions
void stop_isr(){                                                    //Stop ISR
    g_state = State::ST_IDLE;                                       //Sets state to idle
    currStep = 0;                                                   //Resets sequencer to step 0
}


void run_isr(){                                                     //Run ISR
    g_state = State::ST_RUN;                                        //Sets state to run
    g_run_flag = 1;                                                 //Sets run flag high
}

void edit_isr(){                                                    //Edit ISR
    g_state = State::ST_EDIT;                                       //Sets state to edit
    currStep = 0;                                                   //Resets sequencer to step 0
}

void settings_isr(){                                                //Settings ISR
    g_state = State::ST_SETTINGS;                                   //Sets state to settings
    currStep = 0;                                                   //Resets sequencer to step 0
}

void clock_isr(){                                                   //Clock ISR
    g_clock_flag = 1;                                               //Sets clock flag high
}

//State function definitions
void idle_state(){                                                  //Idle State

    stateLED.write(1);                                              //Changes colour of state LED to red

    currStep = 0;                                                   //Reset sequencer and outputs
    cvOut.write(0.0);
    gateOut.write(0);
    accentOut.write(0);


    lcd.clear();                                                    //Clear LCD to display new text
    lcd.printString("IDLE", 0, 0);                                  //Printing idle screen with basic sequence information
    lcd.printString("Steps:", 0, 2);
    sprintf(lcdBuffer, " %i", sequenceLength);
    lcd.printString(lcdBuffer, 0, 3);
    lcd.printString("Gate:", 0, 4);
    sprintf(lcdBuffer, " %ims", gateLength);
    lcd.printString(lcdBuffer, 0, 5);

    lcd.drawLine(0, 10, 84, 10, FILL_BLACK);                        //Underline title
    lcd.drawSprite(42, 11, 37, 42, (int*)Pause);                    //Print pause sprite

    lcd.refresh();                                                  //Refresh LCD to display
    sleep();                                                        //Sleep until interrupt; nothing else needs to happen in this state
}

void run_state(){                                                   //Run state
    if(g_clock_flag) {                                              //If clock pulse is received
        g_clock_flag = 0;                                               //Reset clock flag
        accentOut.write(0);                                             //Reset accent output

        cvOut.write(sequence[currStep].pitch * DAC_SEMITONE);           //Output pitch scaled to the correct voltage

        gateOut.write(!sequence[currStep].rest);                        //If it is not a rest (rest = 0), write 1 to gateOut
        accentOut.write(sequence[currStep].accent);                     //If it is an accent write 1 to accentOut
        ThisThread::sleep_for(std::chrono::milliseconds(gateLength));   //Sleep for the defined gate time, controlled in settings
                                                                                    //https://stackoverflow.com/questions/4184468/sleep-for-milliseconds
        if(!sequence[currStep].hold){                                   //If the step is not held
            gateOut.write(0);                                               //Reset the gate output to 0
        }

        if(accentMode){                                                 //If the accent mode is trigger
            accentOut.write(0);                                             //Reset accent output to 0
        }                                                               //Else accent is high for duration of step (gate mode)

        lcd.clear();                                                                    //Clear LCD ready to display step info
        sprintf(lcdBuffer, "RUN   Step %i", currStep + 1);                              //Format string to contain step number
        lcd.printString(lcdBuffer, 0, 0);                                               //Print formatted string
        std::string pitchLine = "Note:" + pitchStrings[sequence[currStep].pitch];       //Append string with pitch
        const char *pString = pitchLine.c_str();                                        //Convert string to constant char pointer: https://stackoverflow.com/questions/7352099/stdstring-to-char
        lcd.printString(pString, 0, 2);                                                 //Print               
        sprintf(lcdBuffer, "Rst : %s", no_yes[sequence[currStep].rest].c_str());        //Format string with rest value
        lcd.printString(lcdBuffer, 0, 3);                                               //Print
        sprintf(lcdBuffer, "Acc : %s", no_yes[sequence[currStep].accent].c_str());      //Format string with accent value
        lcd.printString(lcdBuffer, 0, 4);                                               //Print
        sprintf(lcdBuffer, "Hold: %s", no_yes[sequence[currStep].hold].c_str());        //Format string with hold value
        lcd.printString(lcdBuffer, 0, 5);                                               //Print

        lcd.drawLine(0, 10, 84, 10, FILL_BLACK);                                        //Underline title
        lcd.drawSprite(42, 11, 37, 42, (int*)Play);                                     //Draw play sprite

        lcd.refresh();                                                                  //Refresh LCD to display

        if(currStep >= sequenceLength - 1) {                            //If last step of sequence (or sequence has overrun through error)
            currStep = 0;                                                   //Reset to first step
        }
        else {                                                          //Else 
            currStep += 1;                                                  //Increment step
        }

        

    }

    if(g_run_flag) {                                                //If this is the first time running run_state()
        g_run_flag = 0;                                                 //Reset flag

        currStep = 0;                                                   //Reset sequence to step 0

        stateLED.write(4);                                              //Set state LED to green

        lcd.clear();                                                    //Clear LCD
        lcd.printString("RUN", 0, 0);                                   //Display waiting message
        lcd.drawLine(0, 10, 84, 10, FILL_BLACK);
        lcd.printString(" Wait", 0, 2);
        lcd.printString(" for", 0, 3);
        lcd.printString(" CLK", 0, 4);

        lcd.drawSprite(42, 11, 37, 42, (int*)Play);                     //Draw play sprite

        lcd.refresh();                                                  //Refresh LCD
    }
    
    sleep();                                                        //Sleep until interrupt
}

void edit_state(){                                                  //Edit state                                          

    stateLED.write(3);                                              //Change state LED colour

    gateOut.write(1);                                               //Sets gate to high, enables note to be heard
    cvOut.write(sequence[currStep].pitch * DAC_SEMITONE);           //Write the current steps scaled pitch voltage

    joystickDir = joystick.get_direction();                         //Get input from joystick

    if(!joystick_btn.read()) {                                      //If joystick button is pressed
        if(selectedItem < 3){                                           //Loop through selected item possibilites
            selectedItem++;
        }
        else {
            selectedItem = 0;
        }
    }

    switch (joystickDir) {                                                      //Switch statement for joystick input
        case N: {                                                                   //North
            switch (selectedItem) {                                                     //Switch based on selected item
                case 0: {                                                                   //Pitch is selected
                    int pitch = static_cast<int>(sequence[currStep].pitch);                 //Convert Pitch enum to int: unwind, https://stackoverflow.com/questions/3475152/why-cant-i-increment-a-variable-of-an-enumerated-type
                    if (pitch < 35) {                                                       //Increment pitch if it is within range
                        pitch++;
                    }
                    sequence[currStep].pitch = static_cast<Pitch>(pitch);                   //pitch = new value converted from int            
                    break;
                }
                case 1: {                                                               //Rest is selected
                    sequence[currStep].rest = true;                                         //Rest = true
                    break;
                }
                case 2: {                                                               //Accent is selected
                    sequence[currStep].accent = true;                                       //Accent = true
                    break;
                }
                case 3: {                                                               //Hold is selected
                    sequence[currStep].hold = true;                                         //Hold = true
                    break;
                }
            }
            break;
        }
        case S: {                                                                   //South
            switch (selectedItem) {                                                     //Switch based on selected item
                case 0: {                                                                   //Pitch is selected
                    int pitch = static_cast<int>(sequence[currStep].pitch);                     //Convert to int
                    if (pitch > 0) {                                                            //Decrement if within range
                        pitch--;
                    }
                    sequence[currStep].pitch = static_cast<Pitch>(pitch);                       //Convert back to enum
                    break;
                }
                case 1: {                                                                   //Rest is selected
                    sequence[currStep].rest = false;                                            //Rest = false
                    break;
                }
                case 2: {                                                                   //Accent is selected
                    sequence[currStep].accent = false;                                          //Rest = false
                    break;
                }
                case 3: {                                                                   //Hold is selected
                    sequence[currStep].hold = false;                                           //Rest = false
                }
            }
            break;
        }
        case E: {                                                                   //East
            if(currStep < sequenceLength - 1) {                                         //If current step is less than the maximum step
            currStep++;                                                                     //Increment to next step
            }
            break;
        }
        case W: {                                                                   //West
            if(currStep > 0) {                                                          //If currentstep is greater than 0
                currStep--;                                                                 //Decrement to previous step
            }
            break;
        }
        default:{                                                                   //Default = all other directions/centre
            break;
        }
    }

    lcd.clear();                                                                                        //Clear LCD ready to display edit state
    sprintf(lcdBuffer, "EDIT  Step %i", currStep + 1);                                                  //Format string with current step int
    lcd.printString(lcdBuffer, 0, 0);                                                                   //Print
    std::string pitchLine = " Pitch: " + pitchStrings[sequence[currStep].pitch];                        //Append string with pitch
    const char *pString = pitchLine.c_str();                                                            //Convert string to const char pointer
    lcd.printString(pString, 0, 2);                                                                     //Print
    sprintf(lcdBuffer, " R:%s  A:%s  H:%s", no_yes[sequence[currStep].rest].c_str(),                    //Format string with rest, accent, hold value
                no_yes[sequence[currStep].accent].c_str(), no_yes[sequence[currStep].hold].c_str());
    lcd.printString(lcdBuffer, 0, 4);                                                                   //Print
    
    lcd.drawLine(0, 10, 84, 10, FILL_BLACK);                                                            //Underline title

    switch(selectedItem) {                                                          //Switch based on selected item
        case 0: {                                                                       //Pitch is selected
            for (int y = 15; y < 25; y++) {                                                 //Loop through pixels in rectangle around displayed pitch value
                for (int x = 0; x < 84; x++) {
                    bool pixel = lcd.getPixel(x, y);                                        //Get value of pixel
                    lcd.setPixel(x, y, !pixel);                                             //Invert pixel colour
                }
            }
            break;
        }
        case 1: {                                                                       //Rest is selected
            for (int y = 31; y < 41; y++) {                                                 //Loop through pixels and invert
                for (int x = 0; x < 28; x++) {
                    bool pixel = lcd.getPixel(x, y);
                    lcd.setPixel(x, y, !pixel);
                }
            }
            break;
        }
        case 2: {                                                                       //Accent is selected
            for (int y = 31; y < 41; y++) {                                                 //Loop through pixels and invert
                for (int x = 30; x < 58; x++) {
                    bool pixel = lcd.getPixel(x, y);
                    lcd.setPixel(x, y, !pixel);
                }
            }
            break;
        }
        case 3: {                                                                       //Hold is selected
            for (int y = 31; y < 41; y++) {                                                 //Loop through pixels and invert
                for (int x = 60; x < 84; x++) {
                    bool pixel = lcd.getPixel(x, y);
                    lcd.setPixel(x, y, !pixel);
                }
            }
            break;
        }
    }
    
    lcd.refresh();                                                                  //Refresh LCD to display
    
    ThisThread::sleep_for(MENU_WAIT_TIME);                                          //Sleep for wait time
}

void settings_state(){                                              //Settings state
    currStep = 0;                                                   //Reset sequence and outputs
    cvOut.write(0);
    gateOut.write(0);
    accentOut.write(0);
    stateLED.write(6);

    joystickDir = joystick.get_direction();                         //Read joystick direction
    
    lcd.clear();                                                    //Clear lcd
    lcd.printString("SETTINGS", 0, 0);                              //Print title and underline
    lcd.drawLine(0, 10, 84, 10, FILL_BLACK);

    const char * menuChar;                                          //char pointer for storing converted strings

    if (menuState) {                                                //If menuState = 1, i.e a setting is selected
        if(settings[currentMenuItem].second == " Calibrate") {          //If statement for settings with unique functions
            calibrate_sequencer();
            menuState = 0;
        }
        else if (settings[currentMenuItem].second == " Reset") {
            reset_sequencer();
            lcd.printString(" Sequencer", 0, 2);
            lcd.printString("   Reset", 0, 3);
            menuState = 0;
            ThisThread::sleep_for(500ms);
        }
        else {
            switch(joystickDir) {                                   //Switch based on joystick direction
                case N: {                                           //North
                    settings[currentMenuItem].first.increment();        //Increment settings value
                    break;
                }
                case S: {                                           //South
                    settings[currentMenuItem].first.decrement();        //Decrement settings value
                    break;
                }
                case E: {                                           //East
                    menuState = 1;                                      //MenuState unchanged
                    break;
                }
                case W: {                                           //West
                    menuState = 0;                                      //Go back to main menu
                    break;
                }
                default: {break;}                                   //Default: other directions/centre
            }

            menuChar = settings[currentMenuItem].second.c_str();                                        //Convert setting 'name' to char pointer
            lcd.printString(menuChar, 0, 2);                                                            //Print
            menuChar = settings[currentMenuItem].first.get_unit().c_str();                              //Convert setting units to char pointer
            sprintf(lcdBuffer, " Value: %i%s", settings[currentMenuItem].first.get_var(), menuChar);    //Append setting value with units
            lcd.printString(lcdBuffer, 0, 3);                                                           //Print

            int barSize = settings[currentMenuItem].first.get_percent() * 84;                           //Calculate bar width

            lcd.drawRect(0, 40, barSize, 45, FILL_BLACK);                                               //Draw bar
        }
    }
    else {                                                          //If menuState = 0, i.e main settings menu
        switch (joystickDir) {                                          //Switch based on joystick direction
            case N: {                                                       //North
                if(currentMenuItem > 0) {                                       //Decrement current menu item if possible
                    currentMenuItem--;
                }
                break;
            }   
            case S: {                                                       //South
                if(currentMenuItem < (sizeof(settings)/sizeof(*settings)-1)) {  //Increment current menu item if less than number of items in array: https://stackoverflow.com/questions/4108313/how-do-i-find-the-length-of-an-array
                    currentMenuItem++;
                }
                break;
            }
            case E: {                                                       //East
                menuState = 1;                                                  //Select setting
                break;
            }
            case W: {                                                       //West
                menuState = 0;                                                  //Does nothing
                break;
            }
            default: {break;}                                               //Default = all other directions
        }

        if(currentMenuItem == 0) {                                              //Special display for if first menu item is selected
            menuChar = settings[currentMenuItem].second.c_str();                //Only prints selected item and 2nd item
            lcd.printString(menuChar, 0, 3);
            menuChar = settings[currentMenuItem + 1].second.c_str();
            lcd.printString(menuChar, 0, 4);
        }
        else if (currentMenuItem == (sizeof(settings)/sizeof(*settings)-1)) {   //Special display for if last menu item is selected
            menuChar = settings[currentMenuItem].second.c_str();                //Only prints 2nd to last and last items
            lcd.printString(menuChar, 0, 3);
            menuChar = settings[currentMenuItem - 1].second.c_str();
            lcd.printString(menuChar, 0, 2);
        }
        else {                                                                  //Standard case
            menuChar = settings[currentMenuItem - 1].second.c_str();            //Prints current selected menu item in centre
            lcd.printString(menuChar, 0, 2);                                    //Prints previous and next item above and below
            menuChar = settings[currentMenuItem].second.c_str();
            lcd.printString(menuChar, 0, 3);
            menuChar = settings[currentMenuItem + 1].second.c_str();
            lcd.printString(menuChar, 0, 4);
        }

        for (int y = 24; y < 32; y++) {                                         //Inverting pixels behind selected menu item to highlight
            for (int x = 0; x < 84; x++) {
                bool pixel = lcd.getPixel(x, y);
                lcd.setPixel(x, y, !pixel);
            }
        }
    }

    lcd.refresh();                                                  //Refresh LCD to display

    ThisThread::sleep_for(MENU_WAIT_TIME);                          //Sleep for menu wait time
}

void init_sequence(){                                               //Function handles initialising sequence
    sequence[0].pitch   = c1;                                       //Load preset pitch pattern
    sequence[1].pitch   = bb3;
    sequence[2].pitch   = g2;
    sequence[3].pitch   = f3;
    sequence[4].pitch   = eb2;
    sequence[5].pitch   = c1;
    sequence[6].pitch   = c1;
    sequence[7].pitch   = c1;
    sequence[8].pitch   = eb3;
    sequence[9].pitch   = eb1;
    sequence[10].pitch  = f3;
    sequence[11].pitch  = g3;
    sequence[12].pitch  = bb1;
    sequence[13].pitch  = g1;
    sequence[14].pitch  = g1;
    sequence[15].pitch  = g2;

    sequence[0].rest    = 0;                                        //Load preset rest pattern
    sequence[1].rest    = 1;
    sequence[2].rest    = 0;
    sequence[3].rest    = 0;
    sequence[4].rest    = 0;
    sequence[5].rest    = 0;
    sequence[6].rest    = 0;
    sequence[7].rest    = 0;
    sequence[8].rest    = 0;
    sequence[9].rest    = 0;
    sequence[10].rest   = 0;
    sequence[11].rest   = 1;
    sequence[12].rest   = 0;
    sequence[13].rest   = 0;
    sequence[14].rest   = 0;
    sequence[15].rest   = 0;

    sequence[0].accent  = 1;                                        //Load preset accent pattern
    sequence[1].accent  = 0;
    sequence[2].accent  = 0;
    sequence[3].accent  = 1;
    sequence[4].accent  = 0;
    sequence[5].accent  = 0;
    sequence[6].accent  = 1;
    sequence[7].accent  = 0;
    sequence[8].accent  = 0;
    sequence[9].accent  = 1;
    sequence[10].accent = 0;
    sequence[11].accent = 0;
    sequence[12].accent = 1;
    sequence[13].accent = 0;
    sequence[14].accent = 0;
    sequence[15].accent = 1;

    sequence[0].hold    = 0;                                        //Load preset hold pattern
    sequence[1].hold    = 0;
    sequence[2].hold    = 0;
    sequence[3].hold    = 0;
    sequence[4].hold    = 0;
    sequence[5].hold    = 1;
    sequence[6].hold    = 1;
    sequence[7].hold    = 0;
    sequence[8].hold    = 0;
    sequence[9].hold    = 0;
    sequence[10].hold   = 0;
    sequence[11].hold   = 0;
    sequence[12].hold   = 0;
    sequence[13].hold   = 1;
    sequence[14].hold   = 0;
    sequence[15].hold   = 0;
}

void reset_sequencer() {                                            //Function handles resetting sequence
    init_sequence();                                                //Loads initial sequence
    for(std::pair<Controller, std::string> setting : settings) {    //Sets all settings to default value
        setting.first.reset();
    }
}

void calibrate_sequencer() {
    int calibrationStage = 0;
    int calibrationComplete = 0;

    float calibrationOffVoltage = 0.0;
    float calibrationOff = 0.0;
    float calibrationVals[3] = {1.0, 2.0, 3.0};

    while(!calibrationComplete) {

        joystickDir = joystick.get_direction();

        lcd.clear();
        lcd.printString("CALIBRATION", 0, 0);
        lcd.drawLine(0, 11, 84, 11, FILL_BLACK);
        
        switch (calibrationStage) {
            case 0: {
                cvOut.write(0);

                switch (joystickDir) {
                    case N: {
                        calibrationOffVoltage += CALIBRATION_INC;
                        break;
                    }
                    case S: {
                        calibrationOffVoltage -= CALIBRATION_INC;
                        break;
                    }
                    default: {break;}
                }

                sprintf(lcdBuffer, "Off: %i", int(calibrationOffVoltage * 100));
                lcd.printString(lcdBuffer, 0, 2);

                if(!joystick_btn.read()) {calibrationStage = 1;}

                break;
            }
            case 1: {
                calibrationOff = calibrationOffVoltage / 3.3;
                break;
            }
            case 2: {

                break;
            }
            case 3: {

                break;
            }
            case 4: {

                break;
            }
        }

        lcd.refresh();
        ThisThread::sleep_for(150ms);
    }
    
}