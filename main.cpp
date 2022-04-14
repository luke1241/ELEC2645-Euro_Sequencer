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
   #define makes it easier to change quickly in development
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


//Global constant definitions
#define MAX_SEQUENCE_LENGTH 64
#define DAC_SEMITONE 0.0252525252525

//Enum for states
enum class State {                                      //https://www.aleksandrhovhannisyan.com/blog/finite-state-machine-fsm-tutorial-implementing-an-fsm-in-c/
    ST_IDLE,
    ST_RUN,
    ST_EDIT,
    ST_SETTINGS
};

//Init state to IDLE
static State g_state = State::ST_IDLE;

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

std::string no_yes[2] = {"N", "Y"};



//Map of pitches and corresponding AnalogOut vals

std::map<std::string, int> pitch {
    {"C1", 0},
    {"C#1", 1},
    {"D1", 2},
    {"D#1", 3},
    {"E1", 4},
    {"F1", 5},
    {"F#1", 6},
    {"G1", 7},
    {"G#1", 8},
    {"A1", 9},
    {"A#1", 10},
    {"B1", 11},
    {"C2", 12},
    {"C#2", 13},
    {"D2", 14},
    {"D#2", 15},
    {"E2", 16},
    {"F2", 17},
    {"F#2", 18},
    {"G2", 19},
    {"G#2", 20},
    {"A2", 21},
    {"A#2", 22},
    {"B2", 23},
    {"C3", 24},
    {"C#3", 25},
    {"D3", 26},
    {"D#3", 27},
    {"E3", 28},
    {"F3", 29},
    {"F#3", 30},
    {"G3", 31},
    {"G#3", 32},
    {"A3", 33},
    {"A#3", 34},
    {"B3", 35}
};

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

//Step struct hold data for each step
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

int contrast = 50;
int brightness = 50;
int screenInvert = 0;

//Controller objects
Controller sequenceLength_controller(sequenceLength, 8, 1, MAX_SEQUENCE_LENGTH, 1, "");
Controller gateLength_controller(gateLength, 20, 10, 200, 10, "ms");
Controller accentMode_controller(accentMode, 0, 0, 1, 1, "");
Controller brightness_controller(brightness, 50, 0, 100, 1, "%");
Controller contrast_controller(contrast, 50, 0, 100, 1, "%");
Controller invert_controller(screenInvert, 0, 0, 1, 1, "");


//Edit menu variables
int selectedItem = 0;

//Settings Menu Variables
int currentMenuItem = 0;
bool menuState = 0;
//std::string menuItemStrings[6] = {" No. Steps", " Gate Length", " Accent Mode", " Brightness", " Contrast", " Calibration"};

std::pair<Controller, std::string> menuItems[6] = {
    make_pair(sequenceLength_controller, " No. Steps"),
    make_pair(gateLength_controller, " Gate Length"),
    make_pair(accentMode_controller, " Accent Mode"),
    make_pair(brightness_controller, " Brightness"),
    make_pair(contrast_controller, " Contrast"),
    make_pair(invert_controller, " Invert")
};

/*
typedef std::pair<std::string, int&> str_ref_pair;

std::vector<std::pair<std::string, int>> menuItems = {make_pair(" No. Steps", sequenceLength)};
*/




//uint8_t keyboardVal = 0b01010101;     uint8_t for 8 bit int

//Input/Output/Interrupt object declarations
DebounceIn clockIn(CLOCK_PIN, PullDown);
InterruptIn stopBtn(STOP_PIN);
InterruptIn runBtn(RUN_PIN);
InterruptIn editBtn(EDIT_PIN);
InterruptIn settingsBtn(SETTINGS_PIN);

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
DigitalIn joystick_btn(JOYSTICK_BTN);

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
//void changeSetting(std::string name, std::string unit, int &var, int minVal, int maxVal);
void updateB_C();

void idle_state();
void run_state();
void edit_state();
void settings_state();

/* STATE MACHINE OVERVIEW
->idle     <->
  edit     <-> run(step 1 -> step 2 -> ... -> step n ->>)
  settings <->

*/

//LCD Sprites
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

int main()
{
    

    stopBtn.rise(&stop_isr);
    stopBtn.mode(PullNone);

    runBtn.rise(&run_isr);
    runBtn.mode(PullNone);

    editBtn.rise(&edit_isr);
    editBtn.mode(PullNone);

    settingsBtn.rise(&settings_isr);
    settingsBtn.mode(PullNone);
    //Sets interrupt object to call clock isr on rising edge
    clockIn.fall(&clock_isr, 1ms);
    
    joystick.init();
    joystick_btn.mode(PullUp);
    
    //Initialise cv and gate voltage outputs to 0
    cvOut = 0.0;
    gateOut = 0.0;

    //init sequence TEMP
    init_sequence();

    //LCD Initialisation
    lcd.init(LPH7366_1);
    lcd.setContrast(0.1);  
    lcd.setBrightness(0.5);

    lcd.clear();

    //LCD load screen
    lcd.printString(" ============ ", 0, 1);
    lcd.printString(" EURO SEQ ", 0, 2);
    lcd.printString(" ============ ", 0, 3);
    lcd.refresh();
    ThisThread::sleep_for(1s);

    while (true) {
        switch(g_state){
            case State::ST_RUN:                                                  //RUN state
                run_state();
                break;
            case State::ST_EDIT:                                                 //EDIT state
                edit_state();
                break;
            case State::ST_SETTINGS:                                             //SETTINGS state
                settings_state();
                break;
            default:                                                             //IDLE state
                idle_state();
                break;
        }
        


        //Sleep until state change
        sleep();
    }
}
//Clock isr definition


void stop_isr(){
    g_state = State::ST_IDLE;
    currStep = 0;
}

//Run isr definition
void run_isr(){
    g_state = State::ST_RUN;
    g_run_flag = 1;
}

void edit_isr(){
    g_state = State::ST_EDIT;
    currStep = 0;
}

void settings_isr(){
    g_state = State::ST_SETTINGS;
    currStep = 0;
}

void clock_isr(){
    g_clock_flag = 1;
}

void idle_state(){
    stateLED.write(1);

    currStep = 0;
    cvOut.write(0.0);
    gateOut.write(0);
    accentOut.write(0);


    lcd.clear();
    lcd.printString("IDLE", 0, 0);
    lcd.printString("Steps:", 0, 2);
    sprintf(lcdBuffer, " %i", sequenceLength);
    lcd.printString(lcdBuffer, 0, 3);
    lcd.printString("Gate:", 0, 4);
    sprintf(lcdBuffer, " %ims", gateLength);
    lcd.printString(lcdBuffer, 0, 5);

    lcd.drawLine(0, 10, 84, 10, FILL_BLACK);
    lcd.drawSprite(42, 11, 37, 42, (int*)Pause);

    lcd.refresh();
    sleep();
}

void run_state(){
    if(g_clock_flag) {                                  //If clock pulse is received
        g_clock_flag = 0;                               //Reset clock flag
        accentOut.write(0);

        cvOut.write(sequence[currStep].pitch * DAC_SEMITONE);

        gateOut.write(!sequence[currStep].rest);
        accentOut.write(sequence[currStep].accent);
        ThisThread::sleep_for(std::chrono::milliseconds(gateLength));           //https://stackoverflow.com/questions/4184468/sleep-for-milliseconds

        if(!sequence[currStep].hold){
            gateOut.write(0);
        }

        if(accentMode){
            accentOut.write(0);
        }

        lcd.clear();
        sprintf(lcdBuffer, "RUN   Step %i", currStep + 1);
        lcd.printString(lcdBuffer, 0, 0);
        std::string pitchLine = "Note:" + pitchStrings[sequence[currStep].pitch];
        const char *pString = pitchLine.c_str();
        lcd.printString(pString, 0, 2);                     //https://stackoverflow.com/questions/7352099/stdstring-to-char
        sprintf(lcdBuffer, "Rst : %s", no_yes[sequence[currStep].rest].c_str());
        lcd.printString(lcdBuffer, 0, 3);
        sprintf(lcdBuffer, "Acc : %s", no_yes[sequence[currStep].accent].c_str());
        lcd.printString(lcdBuffer, 0, 4);
        sprintf(lcdBuffer, "Hold: %s", no_yes[sequence[currStep].hold].c_str());
        lcd.printString(lcdBuffer, 0, 5);

        lcd.drawLine(0, 10, 84, 10, FILL_BLACK);
        lcd.drawSprite(42, 11, 37, 42, (int*)Play);

        lcd.refresh();

        if(currStep >= sequenceLength - 1) {            //Reset to first step if last step of sequence (or sequence has overrun through error)
            currStep = 0;
        }
        else {                                          //Else increment step
            currStep += 1;
        }

        

    }

    if(g_run_flag) {
        g_run_flag = 0;

        currStep = 0;

        stateLED.write(4);

        lcd.clear();
        lcd.printString("RUN", 0, 0);
        lcd.drawLine(0, 10, 84, 10, FILL_BLACK);
        lcd.printString(" Wait For CLK", 0, 4);
        lcd.refresh();
    }
    
    sleep();
}

void edit_state(){

    stateLED.write(3);

    gateOut.write(1);
    cvOut.write(sequence[currStep].pitch * DAC_SEMITONE);

    joystickDir = joystick.get_direction();

    if(!joystick_btn.read()) {
        if(selectedItem < 3){
            selectedItem++;
        }
        else {
            selectedItem = 0;
        }
    }

    switch (joystickDir) {
        case N: {
            switch (selectedItem) {
                case 0: {
                    int pitch = static_cast<int>(sequence[currStep].pitch);         //unwind, https://stackoverflow.com/questions/3475152/why-cant-i-increment-a-variable-of-an-enumerated-type
                    if (pitch < 35) {
                        pitch++;
                    }
                    sequence[currStep].pitch = static_cast<Pitch>(pitch);
                    break;
                }
                case 1: {
                    sequence[currStep].rest = true;
                    break;
                }
                case 2: {
                    sequence[currStep].accent = true;
                    break;
                }
                case 3: {
                    sequence[currStep].hold = true;
                    break;
                }
            }
            break;
        }
        case S: {
            switch (selectedItem) {
                case 0: {
                    int pitch = static_cast<int>(sequence[currStep].pitch);
                    if (pitch > 0) {
                        pitch--;
                    }
                    sequence[currStep].pitch = static_cast<Pitch>(pitch);
                    break;
                }
                case 1: {
                    sequence[currStep].rest = false;
                    break;
                }
                case 2: {
                    sequence[currStep].accent = false;
                    break;
                }
                case 3: {
                    sequence[currStep].hold = false;
                }
            }
            break;
        }
        case E: {
            if(currStep < sequenceLength - 1) {            //Reset to first step if last step of sequence (or sequence has overrun through error)
            currStep++;
            }
            break;
        }
        case W: {
            if(currStep > 0) {            //Reset to first step if last step of sequence (or sequence has overrun through error)
                currStep--;
            }
            break;
        }
        default:{
            break;
        }
    }

    lcd.clear();
    sprintf(lcdBuffer, "EDIT  Step %i", currStep + 1);
    lcd.printString(lcdBuffer, 0, 0);
    std::string pitchLine = " Pitch: " + pitchStrings[sequence[currStep].pitch];
    const char *pString = pitchLine.c_str();
    lcd.printString(pString, 0, 2);
    sprintf(lcdBuffer, " R:%s  A:%s  H:%s", no_yes[sequence[currStep].rest].c_str(), no_yes[sequence[currStep].accent].c_str(), no_yes[sequence[currStep].hold].c_str());
    lcd.printString(lcdBuffer, 0, 4);
    
    lcd.drawLine(0, 10, 84, 10, FILL_BLACK);

    switch(selectedItem) {
        case 0: {
            //lcd.drawRect(0, 24, 84, 8, FILL_BLACK)
            
            for (int y = 15; y < 25; y++) {
                for (int x = 0; x < 84; x++) {
                    bool pixel = lcd.getPixel(x, y);
                    lcd.setPixel(x, y, !pixel);
                }
            }
        
            //lcd.drawLine(0, 26, 84, 26, FILL_BLACK);
            break;
        }
        case 1: {
            //lcd.drawRect(0, 32, 41, 8, FILL_BLACK);
            
            for (int y = 31; y < 41; y++) {
                for (int x = 0; x < 28; x++) {
                    bool pixel = lcd.getPixel(x, y);
                    lcd.setPixel(x, y, !pixel);
                }
            }
          
            //lcd.drawLine(0, 42, 41, 42, FILL_BLACK);
            break;
        }
        case 2: {
            //lcd.drawRect(41, 32, 41, 8, FILL_BLACK);
            
            for (int y = 31; y < 41; y++) {
                for (int x = 30; x < 58; x++) {
                    bool pixel = lcd.getPixel(x, y);
                    lcd.setPixel(x, y, !pixel);
                }
            }
           
            //lcd.drawLine(41, 42, 84, 42, FILL_BLACK);
            break;
        }
        case 3: {
            for (int y = 31; y < 41; y++) {
                for (int x = 60; x < 84; x++) {
                    bool pixel = lcd.getPixel(x, y);
                    lcd.setPixel(x, y, !pixel);
                }
            }
           
            //lcd.drawLine(41, 42, 84, 42, FILL_BLACK);
            break;
        }
    }
    
    lcd.refresh();
    
    ThisThread::sleep_for(150ms);
}

void settings_state(){
    currStep = 0;
    cvOut.write(0);
    gateOut.write(0);
    accentOut.write(0);
    stateLED.write(6);

    joystickDir = joystick.get_direction();
    
    lcd.clear();
    lcd.printString("SETTINGS", 0, 0);
    lcd.drawLine(0, 10, 84, 10, FILL_BLACK);

    const char * menuChar;

    if (menuState) {
        switch(joystickDir) {
            case N: {
                menuItems[currentMenuItem].first.increment();
                break;
            }
            case S: {
                menuItems[currentMenuItem].first.decrement();
                break;
            }
            case E: {
                menuState = 1;
                break;
            }
            case W: {
                menuState = 0;
                break;
            }
            default: {break;}
        }

        menuChar = menuItems[currentMenuItem].second.c_str();
        lcd.printString(menuChar, 0, 2);
        menuChar = menuItems[currentMenuItem].first.get_unit().c_str();
        sprintf(lcdBuffer, " Value: %i%s", menuItems[currentMenuItem].first.get_var(), menuChar);
        lcd.printString(lcdBuffer, 0, 3);

        int barSize = menuItems[currentMenuItem].first.get_percent() * 84;

        lcd.drawRect(0, 40, barSize, 45, FILL_BLACK);

    }
    else {
        switch (joystickDir) {
            case N: {
                if(currentMenuItem > 0) {
                    currentMenuItem--;
                }
                break;
            }
            case S: {
                if(currentMenuItem < (sizeof(menuItems)/sizeof(*menuItems)-1)) {                 //https://stackoverflow.com/questions/4108313/how-do-i-find-the-length-of-an-array
                    currentMenuItem++;
                }
                break;
            }
            case E: {
                menuState = 1;
                break;
            }
            case W: {
                menuState = 0;
                break;
            }
            default: {break;}
        }

        if(currentMenuItem == 0) {
            menuChar = menuItems[currentMenuItem].second.c_str();
            lcd.printString(menuChar, 0, 3);
            menuChar = menuItems[currentMenuItem + 1].second.c_str();
            lcd.printString(menuChar, 0, 4);
        }
        else if (currentMenuItem == (sizeof(menuItems)/sizeof(*menuItems)-1)) {
            menuChar = menuItems[currentMenuItem].second.c_str();
            lcd.printString(menuChar, 0, 3);
            menuChar = menuItems[currentMenuItem - 1].second.c_str();
            lcd.printString(menuChar, 0, 2);
        }
        else {
            menuChar = menuItems[currentMenuItem - 1].second.c_str();
            lcd.printString(menuChar, 0, 2);
            menuChar = menuItems[currentMenuItem].second.c_str();
            lcd.printString(menuChar, 0, 3);
            menuChar = menuItems[currentMenuItem + 1].second.c_str();
            lcd.printString(menuChar, 0, 4);
        }

        for (int y = 24; y < 32; y++) {
            for (int x = 0; x < 84; x++) {
                bool pixel = lcd.getPixel(x, y);
                lcd.setPixel(x, y, !pixel);
            }
        }
    }
        
    


    //changeSetting("No. Steps", "", sequenceLength, 1, MAX_SEQUENCE_LENGTH);



    //lcd.setContrast(contrast / 100.0);

    lcd.refresh();

    updateB_C();

    ThisThread::sleep_for(150ms);
}

void init_sequence(){
    sequence[0].pitch   = c1;
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

    sequence[0].rest    = 0;
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

    sequence[0].accent  = 1;
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

    sequence[0].hold    = 0;
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
/*
void changeSetting(std::string name, std::string unit, int &var, int minVal, int maxVal) {
    joystickDir = joystick.get_direction();

    switch (joystickDir) {
        case E: {
            if (var < maxVal) {
                var++;
            }
            break;
        }
        case W: {
            if(var > minVal) {
                var --;
            }
            break;
        }
        default: break;
    }

    std::string nameString = name + ":";
    const char *nChar = nameString.c_str();
    lcd.printString(nChar, 0, 2);

    sprintf(lcdBuffer, "  %i%s", var, unit.c_str());
    lcd.printString(lcdBuffer, 0, 3);

}
*/

void updateB_C() {
    lcd.setBrightness(brightness / 100.0);
    lcd.setContrast(contrast / 100.0);
    lcd.refresh();
}