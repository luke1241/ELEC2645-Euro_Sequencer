//Library / class inclusions
#include "mbed.h"
#include "N5110.h"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>
#include <map>
#include "Joystick.h"
#include "DebounceIn.h"

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

#define JOYSTICK_X
#define JOYSTICK_Y
#define JOYSTICK_BTN


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
    bool glide = 0;
};

//Variable declarations for sequencer
Step sequence[MAX_SEQUENCE_LENGTH];
int sequenceLength = 16;
int gateLength = 20;
bool accentMode = 1;
static int currStep = 0;

//Settings Menu Variables
int currentMenuItem = 0;


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
float pitch_to_voltage(int pitch);

void idle_state();
void run_state();
void edit_state();
void settings_state();

/* STATE MACHINE OVERVIEW
->idle     <->
  edit     <-> run(step 1 -> step 2 -> ... -> step n ->>)
  settings <->

*/

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
    //Activates internal pull down resistor on interrupt pin
    
    //Initialise cv and gate voltage outputs to 0
    cvOut = 0.0;
    gateOut = 0.0;

    //init sequence TEMP
    init_sequence();

    //LCD Initialisation
    lcd.init(LPH7366_1);
    lcd.setContrast(0.5);  
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
    cvOut = 0.0;
    gateOut = 0.0;


    lcd.clear();
    lcd.printString(" SEQ IDLE", 0, 0);
    lcd.printString(" ============ ", 0, 1);
    sprintf(lcdBuffer, " Steps: %i", sequenceLength);
    lcd.printString(lcdBuffer, 0, 3);
    sprintf(lcdBuffer, " Gate: %ims", gateLength);
    lcd.printString(lcdBuffer, 0, 4);
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
        gateOut.write(0);
        if(accentMode){
            accentOut.write(0);
        }

        lcd.clear();
        lcd.printString(" SEQ RUNNING", 0, 0);
        lcd.printString(" ============ ", 0, 1);
        sprintf(lcdBuffer, " Step: %i", currStep + 1);
        lcd.printString(lcdBuffer, 0, 2);
        std::string pitchLine = " Pitch: " + pitchStrings[sequence[currStep].pitch];
        const char *pString = pitchLine.c_str();
        lcd.printString(pString, 0, 3);                     //https://stackoverflow.com/questions/7352099/stdstring-to-char
        sprintf(lcdBuffer, " R:%i A:%i G:%i", sequence[currStep].rest, sequence[currStep].accent, sequence[currStep].glide);
        lcd.printString(lcdBuffer, 0, 5);
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
        lcd.printString(" SEQ RUNNING", 0, 0);
        lcd.printString(" ============ ", 0, 1);
        lcd.printString(" Wait For CLK", 0, 4);
        lcd.refresh();
    }
    
    sleep();
}

void edit_state(){
    currStep = 0;

    stateLED.write(3);

    //Placeholder edit state display
    lcd.clear();
    lcd.printString(" SEQ EDIT", 0, 0);
    lcd.printString(" ============ ", 0, 1);
    sprintf(lcdBuffer, " Steps: %i", sequenceLength);
    lcd.printString(lcdBuffer, 0, 3);
    sprintf(lcdBuffer, " Gate: %ims", gateLength);
    lcd.printString(lcdBuffer, 0, 4);
    lcd.refresh();
    sleep();
}

void settings_state(){
    currStep = 0;

    stateLED.write(6);

    lcd.clear();
    lcd.printString(" SEQ SETTINGS", 0, 0);
    lcd.printString(" ============ ", 0, 1);

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
    sequence[14].pitch  = c1;
    sequence[15].pitch  = g2;

    sequence[0].rest    = 0;
    sequence[1].rest    = 1;
    sequence[2].rest    = 0;
    sequence[3].rest    = 0;
    sequence[4].rest    = 0;
    sequence[5].rest    = 0;
    sequence[6].rest    = 1;
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
}
