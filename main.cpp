//Library / class inclusions
#include "mbed.h"
#include "N5110.h"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>
#include <map>
#include "Encoder.h"

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
#define GATE_PIN PA_1

#define STATE_LED_R PC_5
#define STATE_LED_G PC_6
#define STATE_LED_B PC_8

#define ENCODER_A PC_9
#define ENCODER_B PB_8
#define ENCODER_SW PB_9
#define ENCODER_STEP_RATE 0.5


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
/*
enum class Pitch {
    C1,
    Db1,
    D1,
    Eb1,
    E1,
    F1,
    Gb1,
    G1,
    Ab1,
    A1,
    Bb1,
    B1,
    C2,
    Db2,
    D2,
    Eb2,
    E2,
    F2,
    Gb2,
    G2,
    Ab2,
    A2,
    Bb2,
    B2,
    C3,
    Db3,
    D3,
    Eb3,
    E3,
    F3,
    Gb3,
    G3,
    Ab3,
    A3,
    Bb3,
    B3
};
*/
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

//Step struct hold data for each step
struct Step {
    std::string pitch = "C1";
    bool rest = 0;
    bool accent = 0;
    bool glide = 0;
};

//Variable declarations for sequencer
Step sequence[MAX_SEQUENCE_LENGTH];
int sequenceLength = 8;
int gateLength = 20;
static int currStep = 0;
int clockDiv = 1;

//uint8_t keyboardVal = 0b01010101;     uint8_t for 8 bit int



//Serial port for debugging
BufferedSerial serial(USBTX, USBRX, 9600);

//Input/Output/Interrupt object declarations
InterruptIn clockIn(CLOCK_PIN);
InterruptIn stopBtn(STOP_PIN);
InterruptIn runBtn(RUN_PIN);
InterruptIn editBtn(EDIT_PIN);
InterruptIn settingsBtn(SETTINGS_PIN);

AnalogOut cvOut(CV_PIN);
DigitalOut gateOut(GATE_PIN);

BusOut stateLED(STATE_LED_R, STATE_LED_G, STATE_LED_B);

int encoderDelta = 0;
bool encoderPress = 0;
bool encoderRelease = 0;
float encoderVal = 0.0;
Encoder encoder(PC_9, PB_8, PB_9);

//LCD declaration
N5110 lcd(PC_7, PA_9, PB_10, PB_5, PB_3, PA_10);
char lcdBuffer[14] = {0};

//Global flags
volatile int g_clock_flag = 0;



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
    //init sequence TEMP
    sequence[0].pitch = "C1";
    sequence[1].pitch = "D#1";
    sequence[2].pitch = "G1";
    sequence[3].pitch = "C2";
    sequence[4].pitch = "C1";
    sequence[5].pitch = "D#1";
    sequence[6].pitch = "G2";
    sequence[7].pitch = "C2";
    sequence[8].pitch = "C3";
    sequence[9].pitch = "A#2";
    sequence[10].pitch = "G2";
    sequence[11].pitch = "E2";
    sequence[12].pitch = "C2";
    sequence[13].pitch = "A#1";
    sequence[14].pitch = "G1";
    sequence[15].pitch = "E1";

    stopBtn.rise(&stop_isr);
    stopBtn.mode(PullNone);

    runBtn.rise(&run_isr);
    runBtn.mode(PullNone);

    editBtn.rise(&edit_isr);
    editBtn.mode(PullNone);

    settingsBtn.rise(&settings_isr);
    settingsBtn.mode(PullNone);
    //Sets interrupt object to call clock isr on rising edge
    clockIn.rise(&clock_isr);
    //Activates internal pull down resistor on interrupt pin
    clockIn.mode(PullDown);
    
    //Initialise cv and gate voltage outputs to 0
    cvOut = 0.0;
    gateOut = 0.0;

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
    currStep = 0;
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

void init_sequence(){
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
    stateLED.write(4);

    //currStep = 0;

    if(g_clock_flag) {                                  //If clock pulse is received
        g_clock_flag = 0;                               //Reset clock flag
        if(currStep >= sequenceLength - 1) {            //Reset to first step if last step of sequence (or sequence has overrun through error)
            currStep = 0;
        }
        else {                                          //Else increment step
            currStep += 1;
        }
    }

    cvOut.write(pitch[sequence[currStep].pitch] * DAC_SEMITONE);
    gateOut.write(1);
    ThisThread::sleep_for(std::chrono::milliseconds(gateLength));           //https://stackoverflow.com/questions/4184468/sleep-for-milliseconds
    gateOut.write(0);
    
    /*
    lcd.clear();
    lcd.printString(" SEQ RUNNING", 0, 0);
    lcd.printString(" ============ ", 0, 1);
    sprintf(lcdBuffer, " Step: %i", currStep);
    lcd.printString(lcdBuffer, 0, 2);
    std::string pitchLine = " Pitch: " + sequence[currStep].pitch;
    const char *pString = pitchLine.c_str();
    lcd.printString(pString, 0, 3);                     //https://stackoverflow.com/questions/7352099/stdstring-to-char
    sprintf(lcdBuffer, " R:%i A:%i G:%i", sequence[currStep].rest, sequence[currStep].accent, sequence[currStep].glide);
    lcd.printString(lcdBuffer, 0, 5);
    lcd.refresh();
    */
    lcd.clear();
    lcd.refresh();
    

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

    encoder.read(encoderDelta, encoderPress, encoderRelease);

    if(encoderDelta > 0) {
        encoderVal+= ENCODER_STEP_RATE;
    }
    else if(encoderDelta < 0) {
        encoderVal-= ENCODER_STEP_RATE;
    }

    if(encoderVal < 0) {
        encoderVal = 0;
    }


    

    lcd.clear();
    lcd.printString(" SEQ SETTINGS", 0, 0);
    lcd.printString(" ============ ", 0, 1);
    sprintf(lcdBuffer, " Val: %i", int(encoderVal));
    lcd.printString(lcdBuffer, 0, 3);
    lcd.refresh();
    sleep();
}