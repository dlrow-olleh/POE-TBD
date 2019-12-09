/**********************************************************************
  DUAL ARM PLOTTER WITH BELT DRIVE
  Code by lingib
  Last update 16 August 2017

  ----------
  CONCEPT
  ----------
  This plotter is essentially a 2DOF (two degrees of freedom) robotic arm. Unlike
  traditonal arms, which have "shoulder" and "elbow" motors, this plotter has two
  shoulder motors. The second motor controls the "elbow" position by means of a
  lightweight mechanical linkage.

  Software routines mask the natual tendency of the plotter to produce curved lines.

  Simple belt drive gearing improves the plotter resolution by a factor of 4 and
  allows both shoulder pulleys to share a common shaft.

  Advantages;
  1: Eliminating the heavy elbow motor improves the response time. All we are moving
  are lightweight linkages.

  2: Construction is simplified ... all you need are a hacksaw and wood-working drills.

  3: Extremely low cost as very few parts.

  -------------
  CODE UPDATES
  -------------
  Version 1: 
  The motors in the original version are alternately stepped which occasionally creates 
  a one-step error. 

  Version 2:
  Simultaneous motor stepping implemented. This results in smoother lines.

  Version 3:
  The "process();" function is now "process(string);". 
  
  While this change appears minor it allows the use of raw Inkscape gcode when creating test 
  patterns. Complex patterns and circles are now possible. Compare the T5,T6 test patterns in 
  this version with those in the previous versions to see the difference. The F() macro 
  function is used to conserve memory space.
   
  Use a text editor that supports "regular expressions", such as "notepad++", to repackage 
  each line of Inkscape gcode to read "process(F( gcode ));" as shown below:

  Find: ^   Replace With: process\(F\("   Click Replace All
  Find: $   Replace With: "\)\);          Click Replace All

  The F & Z codes may also be deleted to conserve memory.
  
  ----------
  COPYRIGHT
  ----------
  This code is free software: you can redistribute it and/or
  modify it under the terms of the GNU General Public License as published
  by the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License. If
  not, see <http://www.gnu.org/licenses/>.
***************************************************************************/

// -------------------------------
// GLOBALS
// -------------------------------

// ----- constants
#define PI 3.1415926535897932384626433832795
#define HALF_PI 1.5707963267948966192313216916398
#define TWO_PI 6.283185307179586476925286766559
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105

// ----- Bit set/clear/check/toggle macros
#define SET(x,y) (x |=(1<<y))
#define CLR(x,y) (x &= (~(1<<y)))
#define CHK(x,y) (x & (1<<y))
#define TOG(x,y) (x^=(1<<y))

//Registers defined for the particular Arduino the code is for. You could look at the Atmel data sheet for your particular Arduino to learn what the registers control, and what the values mean.
//Those particular statements are setting up timer events.

// ----- motor definitions
#define STEPS_PER_DEG 22400/360     //motor=200 steps/rev; 16 x microstepping; 4 x belt drive
#define NUDGE STEPS_PER_DEG/4       //rotate the motor 0.25 degrees (change number to suit)
#define CW 1                        //motor directions
#define CCW 0
#define DIR1 5                      //Arduino ports
#define DIR2 6
#define STEP1 2
#define STEP2 3
#define STEPPERS_ENABLE_PIN 8

bool
DIRECTION1 = CW,
DIRECTION2 = CCW,
STEP_M1 = false,
STEP_M2 = false;

long
PULSE_WIDTH = 4,                    //easydriver step pulse-width (uS)
DELAY_MIN = 2500,                   //minimum inter-step delay (uS) between motor steps (controls speed)
DELAY1,                             //inter-step delay for motor1 (uS)
DELAY2,                             //inter-step delay for motor2 (uS)
STEPS1,                             //motor1 steps from 12 o'clock to reach an XY co-ordinate
STEPS2;                             //motor2 steps from 12 o'clock to reach an XY co-ordinate

// ----- plotter definitions
#define BAUD 9600
#define XOFF 0x13                   //pause transmission (19 decimal)
#define XON 0x11                    //resume transmission (17 decimal)
#define PEN 3

float
OFFSET = 254,                       //motor offset along x_axis
YAXIS = 465,                        //motor heights above (0,0)
LENGTH = 200,                       //length of each arm-segment
SCALE_FACTOR = 1,                   //drawing scale (1 = 100%)
ARC_MAX = 1;                        //maximum arc-length (controls smoothness)

int
/*
   XY plotters only deal in integer steps.
*/
THIS_X = 0,                         //this X co-ordinate (rounded)
THIS_Y = 0,                         //this Y co-ordinate (rounded)
LAST_X = 0,                         //last X co-ordinate (rounded)
LAST_Y = 0;                         //last Y co-ordinate (rounded)

// ----- gcode definitions
#define STRING_SIZE 128             //string size

char
BUFFER[STRING_SIZE + 1],
INPUT_CHAR;

String
INPUT_STRING,
SUB_STRING;

int
INDEX = 0,                        //buffer index
START,                            //used for sub_string extraction
FINISH;

float
X,                                //gcode float values held here
Y,
I,
J;

// -----------------------
// SETUP
// -----------------------
void setup()
{
  // ----- initialise motor1
  pinMode(STEPPERS_ENABLE_PIN, OUTPUT);
 
  digitalWrite(STEPPERS_ENABLE_PIN, LOW);
  
  pinMode(DIR1, OUTPUT);
  pinMode(STEP1, OUTPUT);
  digitalWrite(DIR1, CW);
  delayMicroseconds(PULSE_WIDTH);
  digitalWrite(STEP1, HIGH);

  // ----- initialise motor2
  pinMode(STEPPERS_ENABLE_PIN, OUTPUT);
  digitalWrite(STEPPERS_ENABLE_PIN, LOW);
  pinMode(DIR2, OUTPUT);
  pinMode(STEP2, OUTPUT);
  digitalWrite(DIR2, CW);
  delayMicroseconds(PULSE_WIDTH);
  digitalWrite(STEP2, HIGH);

  // ----- initialise STEPS1, STEPS2 for co-ordinate (0,0)
  calculate_steps(0, 0);

  // ----- pen-lift
  pinMode(PEN, OUTPUT);                                       //D3
  TCCR2A = _BV(COM2B1) | _BV(COM2B0) | _BV(WGM20);            //PWM
  TCCR2B = _BV(WGM22) | _BV(CS22) | _BV(CS21) | _BV(CS20);    //div 1024
  OCR2A = 156;                                                //20mS period
  OCR2B = 148;                                                //2mS period (pen-up)

  /*
    The above pen-lift comprises a standard servo which requires a 1 mS pulse
    for pen down, or a 2mS pulse for pen up, with a fixed period of 20mS.

    The Arduino "bit value" macro, #define _BV(x) (1 << x), is used to
    set the Timer2 mode to "phase-correct PWM" with a variable "top-limit".
    In this mode the timer counts up to the value entered into register OCR2A
    then back down to zero.

    The following values were used to obtain a 20mS period at pin D3:
      clock:        16 MHz
    prescaler:      1024
    top-limit (OCR2A):  156
    period:       16MHz/1024/(156*2) = 50Hz (20mS)

    If you enter a value into register OCR2B that is LESS than the value in
    register OCR2A then timer2 will will pass through the value in OCR2B
    twice ... once on the way up ... and once on the way down. The duration
    of the output pulse on pin D3 is the time that the count in OCR2A is
    greater than the value in OCR2B.

    A value of 148 entered into OCR2B creates a 1mS pulse:
    period:       156-148)*20mS/156 = 1mS (pen-up)

    A value of 140 entered into OCR2B creates a 2mS pulse):
    period:     156-140)*20mS/156 = 2mS (pen-down)
  */

  // ----- plotter setup
  memset(BUFFER, '\0', sizeof(BUFFER));     //fill with string terminators
  INPUT_STRING.reserve(STRING_SIZE);
  INPUT_STRING = "";

  // ----- establish serial link
  Serial.begin(BAUD);

  // ----- flush the buffers
  Serial.flush();                           //clear TX buffer
  while (Serial.available()) Serial.read(); //clear RX buffer

  // ----- display commands
  menu();
}

//--------------------------------------------------------------------------
// MAIN LOOP
//--------------------------------------------------------------------------
void loop() {

  // ----- get the next instruction
  while (Serial.available()) {
    INPUT_CHAR = (char)Serial.read();         //read character
    Serial.write(INPUT_CHAR);                 //echo character to the screen
    BUFFER[INDEX++] = INPUT_CHAR;             //add char to buffer
    if (INPUT_CHAR == '\n') {                 //check for line feed
      Serial.flush();                         //clear TX buffer
      Serial.write(XOFF);                     //pause transmission
      INPUT_STRING = BUFFER;                  //convert to string
      process(INPUT_STRING);                  //interpret string and perform task
      memset(BUFFER, '\0', sizeof(BUFFER));   //fill buffer with string terminators
      INDEX = 0;                              //point to buffer start
      INPUT_STRING = "";                      //empty string
      Serial.flush();                         //clear TX buffer
      Serial.write(XON);                      //resume transmission
    }
  }
}

//---------------------------------------------------------------------------
// MENU
//---------------------------------------------------------------------------
/*
   The Arduino F() flash macro is used to conserve RAM.
*/
void menu() {
  Serial.println(F(""));
  Serial.println(F("  ------------------------------------------------------"));
  Serial.println(F("                         MENU"));
  Serial.println(F("  ------------------------------------------------------"));
  Serial.println(F("    MENU ............... menu"));
  Serial.println(F("    G00 X## Y## ........ goto XY (pen-up)"));
  Serial.println(F("    G01 X## Y## ........ goto XY (pen-down)"));
  Serial.println(F("    T1 ................. move pen to 0,0"));
  Serial.println(F("    T2 S##.## .......... set drawing Scale (1=100%)"));
  Serial.println(F("    T3 ................. pen up"));
  Serial.println(F("    T4 ................. pen down"));
  Serial.println(F("    T5 ................. test pattern: ABC"));
  Serial.println(F("    T6 ................. test pattern: target"));
  Serial.println(F("    T7 ................. test pattern: radials"));
  Serial.println(F("  ------------------------------------------------------"));
}

//--------------------------------------------------------------------------
// PROCESS
//--------------------------------------------------------------------------
void process(String string) {

  // ----- convert string to upper case
  INPUT_STRING = string;
  INPUT_STRING.toUpperCase();

  // ----------------------------------
  // G00   linear move with pen_up
  // ----------------------------------
  if (INPUT_STRING.startsWith("G00")) {

    // ----- extract X
    START = INPUT_STRING.indexOf('X');
    if (!(START < 0)) {
      FINISH = START + 8;
      SUB_STRING = INPUT_STRING.substring(START + 1, FINISH + 1);
      X = SUB_STRING.toFloat();
    }

    // ----- extract Y
    START = INPUT_STRING.indexOf('Y');
    if (!(START < 0)) {
      FINISH = START + 8;
      SUB_STRING = INPUT_STRING.substring(START + 1, FINISH + 1);
      Y = SUB_STRING.toFloat();
    }

    pen_up();
    move_to(X, Y);
  }

  // ----------------------------------
  // G01   linear move with pen_down
  // ----------------------------------
  if (INPUT_STRING.startsWith("G01")) {

    // ----- extract X
    START = INPUT_STRING.indexOf('X');
    if (!(START < 0)) {
      FINISH = START + 8;
      SUB_STRING = INPUT_STRING.substring(START + 1, FINISH + 1);
      X = SUB_STRING.toFloat();
    }

    // ----- extract Y
    START = INPUT_STRING.indexOf('Y');
    if (!(START < 0)) {
      FINISH = START + 8;
      SUB_STRING = INPUT_STRING.substring(START + 1, FINISH + 1);
      Y = SUB_STRING.toFloat();
    }

    pen_down();
    move_to(X, Y);
  }

  // ----------------------------------
  // G02   clockwise arc with pen_down
  // ----------------------------------
  if (INPUT_STRING.startsWith("G02")) {

    // ----- extract X
    START = INPUT_STRING.indexOf('X');
    if (!(START < 0)) {
      FINISH = INPUT_STRING.indexOf('.', INPUT_STRING.indexOf('X'));
      SUB_STRING = INPUT_STRING.substring(START + 1, FINISH + 7);
      X = SUB_STRING.toFloat();
    }

    // ----- extract Y
    START = INPUT_STRING.indexOf('Y');
    if (!(START < 0)) {
      FINISH = INPUT_STRING.indexOf('.', INPUT_STRING.indexOf('Y'));
      SUB_STRING = INPUT_STRING.substring(START + 1, FINISH + 7);
      Y = SUB_STRING.toFloat();
    }

    // ----- extract I
    START = INPUT_STRING.indexOf('I');
    if (!(START < 0)) {
      FINISH = INPUT_STRING.indexOf('.', INPUT_STRING.indexOf('I'));
      SUB_STRING = INPUT_STRING.substring(START + 1, FINISH + 7);
      I = SUB_STRING.toFloat();
    }

    // ----- extract J
    START = INPUT_STRING.indexOf('J');
    if (!(START < 0)) {
      FINISH = INPUT_STRING.indexOf('.', INPUT_STRING.indexOf('J'));
      SUB_STRING = INPUT_STRING.substring(START + 1, FINISH + 7);
      J = SUB_STRING.toFloat();
    }

    pen_down();
    draw_arc_cw(X, Y, I, J);
  }

  // ------------------------------------------
  // G03   counter-clockwise arc with pen_down
  // ------------------------------------------
  if (INPUT_STRING.startsWith("G03")) {

    // ----- extract X
    START = INPUT_STRING.indexOf('X');
    if (!(START < 0)) {
      FINISH = INPUT_STRING.indexOf('.', INPUT_STRING.indexOf('X'));
      SUB_STRING = INPUT_STRING.substring(START + 1, FINISH + 7);
      X = SUB_STRING.toFloat();
    }

    // ----- extract Y
    START = INPUT_STRING.indexOf('Y');
    if (!(START < 0)) {
      FINISH = INPUT_STRING.indexOf('.', INPUT_STRING.indexOf('Y'));
      SUB_STRING = INPUT_STRING.substring(START + 1, FINISH + 7);
      Y = SUB_STRING.toFloat();
    }

    // ----- extract I
    START = INPUT_STRING.indexOf('I');
    if (!(START < 0)) {
      FINISH = INPUT_STRING.indexOf('.', INPUT_STRING.indexOf('I'));
      SUB_STRING = INPUT_STRING.substring(START + 1, FINISH + 7);
      I = SUB_STRING.toFloat();
    }

    // ----- extract J
    START = INPUT_STRING.indexOf('J');
    if (!(START < 0)) {
      FINISH = INPUT_STRING.indexOf('.', INPUT_STRING.indexOf('J'));
      SUB_STRING = INPUT_STRING.substring(START + 1, FINISH + 7);
      J = SUB_STRING.toFloat();
    }

    pen_down();
    draw_arc_ccw(X, Y, I, J);
  }

  // ----------------------------------
  // MENU
  // ----------------------------------
  if (INPUT_STRING.startsWith("MENU")) {
    menu();
  }

  // ----------------------------------
  // T1   position the pen over 0,0
  // ----------------------------------
  if (INPUT_STRING.startsWith("T1")) {

    // ----- variables
    int step;           //loop counter
    int steps = NUDGE;  //steps motor is to rotate

    // ----- instructions
    Serial.println(F(""));
    Serial.println(F("  ----------------------------------------------"));
    Serial.println(F("    Position the pen over the 0,0 co-ordinate:"));
    Serial.println(F("  ----------------------------------------------"));
    Serial.println(F("    X-axis:             Y-axis:"));
    Serial.println(F("   'A'  'S'            'K'  'L'"));
    Serial.println(F("   <-    ->            <-    ->"));
    Serial.println(F("    Exit = 'E'"));

    // ----- flush the buffer
    while (Serial.available() > 0) Serial.read();

    // ----- control motors with 'A', 'S', 'K', and 'L' keys

    char keystroke = ' ';
    while (keystroke != 'E') {  //press 'E' key to exit

      // ----- check for keypress
      if (Serial.available() > 0) {
        keystroke = (char) Serial.read();
      }

      // ----- select task
      switch (keystroke) {
        case 'a':
        case 'A': {
            // ----- rotate motor1 CW
            for (step = 0; step < steps; step++) {
              step1_cw();
            }
            keystroke = ' ';    //otherwise motor will continue to rotate
            break;
          }
        case 's':
        case 'S': {
            // ------ rotate motor1 CCW
            for (step = 0; step < steps; step++) {
              step1_ccw();
            }
            keystroke = ' ';
            break;
          }
        case 'k':
        case 'K': {
            // ----- rotate motor2 CW
            for (step = 0; step < steps; step++) {
              step2_cw();
            }
            keystroke = ' ';
            break;
          }
        case 'l':
        case 'L': {
            // ----- rotate motor2 CCW
            for (step = 0; step < steps; step++) {
              step2_ccw();
            }
            keystroke = ' ';
            break;
          }
        case 'e':
        case 'E': {
            // ----- exit
            Serial.println(F(" "));
            Serial.println(F("  Calibration complete ..."));
            keystroke = 'E';
            break;
          }
        // ----- default for keystroke
        default: {
            break;
          }
      }
    }

    // ----- initialise counters for co-ordinate (0,0)
    calculate_steps(0, 0);        //initialise STEPS1, STEPS2
    THIS_X = 0;                   //current X co-ordinate
    THIS_Y = 0;                   //current Y co-ordinate
    LAST_X = 0;                   //previous X co-ordinate
    LAST_Y = 0;                   //previous Y-co-ordinate
  }

  // ----------------------------------
  // T2   set scale factor
  // ----------------------------------
  if (INPUT_STRING.startsWith("T2")) {
    Serial.println("T2");

    START = INPUT_STRING.indexOf('S');
    if (!(START < 0)) {
      FINISH = START + 6;
      SUB_STRING = INPUT_STRING.substring(START + 1, FINISH);
      SCALE_FACTOR = SUB_STRING.toFloat();
      Serial.print(F("Drawing now ")); Serial.print(SCALE_FACTOR * 100); Serial.println(F("%"));
    }
    else {
      Serial.println(F("Invalid scale factor ... try again. (1 = 100%)"));
    }
  }

  // ----------------------------------
  // T3   pen up
  // ----------------------------------
  if (INPUT_STRING.startsWith("T3")) {
    pen_up();
  }

  // ----------------------------------
  // T4   pen down
  // ----------------------------------
  if (INPUT_STRING.startsWith("T4")) {
    pen_down();
  }

  // ----------------------------------
  // T5   ABC test pattern
  // ----------------------------------
  if (INPUT_STRING.startsWith("T5")) {
    abc();
  }

  // ----------------------------------
  // T6   target test pattern
  // ----------------------------------
  if (INPUT_STRING.startsWith("T6")) {
    target();
  }

  // ----------------------------------
  // T7   radial line test pattern
  // ----------------------------------
  if (INPUT_STRING.startsWith("T7")) {
    radials();
  }
}

// -------------------------------
// MOVE_TO
// -------------------------------
/*
   We need to make this plotter think that it is an XY plotter
*/
void move_to(float x, float y) {

  // ----- apply scale factor
  THIS_X = round(x * SCALE_FACTOR);
  THIS_Y = round(y * SCALE_FACTOR);

  // ----- draw a line between these co-ordinates
  draw_line(LAST_X, LAST_Y, THIS_X, THIS_Y);

  // ----- remember last rounded co-ordinate
  LAST_X = THIS_X;
  LAST_Y = THIS_Y;
}

// ------------------------------------------------------------------------
// DRAW LINE
// ------------------------------------------------------------------------
/*
  This routine assumes that we are talking to a linear XY plotter.

  The algorithm automatically maps all "octants" to "octant 0" and
  automatically swaps the XY coordinates if dY is greater than dX. A swap
  flag determines which motor moves for any combination X,Y inputs. The swap
  algorithm is further optimised by realising that dY is always positive
  in quadrants 0,1 and that dX is always positive in "quadrants" 0,3.

  Each intermediate XY co-ordinate is plotted which results in a straight line
*/
void draw_line(int x1, int y1, int x2, int y2) {

  // ----- locals
  int
  x = x1,                             //current X-axis position
  y = y1,                             //current Y-axis position
  dy,                                 //line slope
  dx,
  slope,
  longest,                            //axis lengths
  shortest,
  maximum,
  error,                              //bresenham thresholds
  threshold;

  // ----- find longest and shortest axis
  dy = y2 - y1;                         //vertical distance
  dx = x2 - x1;                         //horizontal distance
  longest = max(abs(dy), abs(dx));      //longest axis
  shortest = min(abs(dy), abs(dx));     //shortest axis

  // ----- scale Bresenham values by 2*longest
  error = -longest;                     //add offset to so we can test at zero
  threshold = 0;                        //test now done at zero
  maximum = (longest << 1);             //multiply by two
  slope = (shortest << 1);              //multiply by two ... slope equals (shortest*2/longest*2)

  // ----- initialise the swap flag
  /*
     The XY axes are automatically swapped by using "longest" in
     the "for loop". XYswap is used to decode the motors.
  */
  bool XYswap = true;                    //used for motor decoding
  if (abs(dx) >= abs(dy)) XYswap = false;

  // ----- pretend we are always in octant 0
  /*
     The current X-axis and Y-axis positions will now be incremented (decremented) each time
     through the loop. These intermediate steps are parsed to the plot(x,y) function which calculates
     the number of steps required to reach each of these intermediate co-ordinates. This effectively
     linearises the plotter and eliminates unwanted curves.
  */
  for (int i = 0; i < longest; i++) {

    // ----- move left/right along X axis
    if (XYswap) {   //swap
      if (dy < 0) {
        y--;
      } else {
        y++;
      }
    } else {    //no swap
      if (dx < 0) {
        x--;
      } else {
        x++;
      }
    }

    // ----- move up/down Y axis
    error += slope;
    if (error > threshold) {
      error -= maximum;

      // ----- move up/down along Y axis
      if (XYswap) {  //swap
        if (dx < 0) {
          x--;
        } else {
          x++;
        }
      } else {  //no swap
        if (dy < 0) {
          y--;
        } else {
          y++;
        }
      }
    }

    // ----- plot the next rounded XY co-ordinate
    plot(x, y);
  }
}

//----------------------------------------------------------------------------------------
// PLOT
//----------------------------------------------------------------------------------------
/*
  This routine effectively linearises the plotter and eliminates unwanted curves by
  stepping motor1 and motor2 to the XY co-ordinate that is nearest the target location.
  The global step-counters STEPS1, STEPS2 are automatically updated.
*/
void plot(int x, int y ) {

  // ----- locals
  float
  x_axis = (float)x,                  //calculate_steps() requires a float
  y_axis = (float)y;

  long
  current_steps1 = STEPS1,            //current motor steps
  current_steps2 = STEPS2,
  steps1,                             //extra motor steps to reach this co-ordinate
  steps2;

  bool
  direction1,                         //motor directions
  direction2;

  long
  current_time,                       //system time
  previous_time1,                     //previous system time for motor1
  previous_time2;                     //previous system time for motor2

  // ----- calculate extra motor steps
  calculate_steps(x_axis, y_axis);              //calculate fresh values for STEPS1, STEPS2
  steps1 = abs(STEPS1 - current_steps1);        //extra steps required
  steps2 = abs(STEPS2 - current_steps2);        //extra steps required

  // ----- calculate the motor directions
  DIRECTION1 = (STEPS1 > current_steps1) ? CW : CCW;
  DIRECTION2 = (STEPS2 > current_steps2) ? CW : CCW;

  // ----- calculate motor delays for the extra steps
  calculate_delays(steps1, steps2);

  // ----- preload the timers and counters
  previous_time1 = micros();                      //reset the timer
  previous_time2 = micros();                      //reset the timer

  // ----- step the motors
  /*
     steps1 and steps2 are now used as down-counters
  */
  while ((steps1 != 0) || (steps2 != 0)) {        //stop when both counters equal zero
    // ----- prepare motor1
    if (steps1 > 0) {                             //prevent additional step ... it occasionally happens!
      current_time = micros();
      if (current_time - previous_time1 > DELAY1) {
        previous_time1 = current_time;              //reset timer
        steps1--;                                   //decrement counter1
        STEP_M1 = true;
      }
    }
    // ----- prepare motor2
    if (steps2 > 0) {                             //prevent additional step ... it occasionally happens!
      current_time = micros();
      if (current_time - previous_time2 > DELAY2) {
        previous_time2 = current_time;              //reset timer
        steps2--;                                   //decrement counter2
        STEP_M2 = true;
      }
    }
    // ----- now step the motors
    if ((STEP_M1) || (STEP_M2)) {
      step_motors();
      STEP_M1 = false;                            //reset the flags
      STEP_M2 = false;
    }
  }
}

//----------------------------------------------------------------------------------------
// STEP MOTORS
//----------------------------------------------------------------------------------------
/*
   If required, both motors can step simultaneously which should eliminate potential
   one-step errors and result in a smoother output.
*/
void step_motors() {

  // ----- locals
  enum {dir1, step1, dir2, step2};                                //define bit positions
  byte pattern = PORTD;                                           //read current state PORTD

  // ----- set motor directions
  (DIRECTION1 == CW) ? SET(pattern, dir1) : CLR(pattern, dir1);
  (DIRECTION2 == CW) ? SET(pattern, dir2) : CLR(pattern, dir2);
  PORTD = pattern;
  delayMicroseconds(PULSE_WIDTH);                                 //wait for direction lines to stabilise

  // ----- create leading edge of step pulse(s)
  (STEP_M1) ? SET(pattern, step1) : CLR(pattern, step1);  //prepare step pulse
  (STEP_M2) ? SET(pattern, step2) : CLR(pattern, step2);
  PORTD = pattern;                                                //step the motors
  delayMicroseconds(PULSE_WIDTH);                                 //mandatory delay

  // ----- create trailing-edge of step-pulse(s)
  pattern = CLR(pattern, step1);
  pattern = CLR(pattern, step2);
  PORTD = pattern;
}

//--------------------------------------------------------------------
// STEP1_CW        (step actuator motor1 clockwise)
//--------- -----------------------------------------------------------
void step1_cw() {

  //----- set direction
  digitalWrite(DIR1, CW);
  delayMicroseconds(PULSE_WIDTH);

  //----- step motor
  digitalWrite(STEP1, HIGH);
  delayMicroseconds(PULSE_WIDTH);
  digitalWrite(STEP1, LOW);
  delayMicroseconds(PULSE_WIDTH);
}

//--------------------------------------------------------------------
// STEP1_CCW        (step actuator motor1 counter-clockwise)
//--------- -----------------------------------------------------------
void step1_ccw() {

  //----- set direction
  digitalWrite(DIR1, CCW);
  delayMicroseconds(PULSE_WIDTH);

  //----- step motor
  digitalWrite(STEP1, HIGH);
  delayMicroseconds(PULSE_WIDTH);
  digitalWrite(STEP1, LOW);
  delayMicroseconds(PULSE_WIDTH);
}

//--------------------------------------------------------------------
// STEP2_CW        (step actuator motor2 clockwise)
//--------- -----------------------------------------------------------
void step2_cw() {

  //----- set direction
  digitalWrite(DIR2, CW);
  delayMicroseconds(PULSE_WIDTH);

  //----- step motor
  digitalWrite(STEP2, HIGH);
  delayMicroseconds(PULSE_WIDTH);
  digitalWrite(STEP2, LOW);
  delayMicroseconds(PULSE_WIDTH);
}

//--------------------------------------------------------------------
// STEP2_CCW        (step actuator motor2 counter-clockwise)
//--------- -----------------------------------------------------------
void step2_ccw() {

  //----- set direction
  digitalWrite(DIR2, CCW);
  delayMicroseconds(PULSE_WIDTH);

  //----- step motor
  digitalWrite(STEP2, HIGH);
  delayMicroseconds(PULSE_WIDTH);
  digitalWrite(STEP2, LOW);
  delayMicroseconds(PULSE_WIDTH);
}

//----------------------------------------------------------------------------------------
// CALCULATE STEPS
//----------------------------------------------------------------------------------------
/*
  This routine calculates the the number of motor steps to reach (x,y) from 12 o'clock .
  Global variables STEPS1, STEPS2 hold the required number of steps
*/
void calculate_steps(float x, float y) {

  // ----- locals
  float
  distance,             //pen distance to motors
  angle1,               //motor1 angle
  angle2;               //motor2 angle

  // ----- calculate distances
  distance = sqrt((OFFSET - x) * (OFFSET - x) + (YAXIS - y) * (YAXIS - y));

  // ----- calculate motor1 angle when pen at (x,y)
  if (x > OFFSET) {
    angle1 = PI + acos(distance / (2 * LENGTH)) - atan((x - OFFSET) / (YAXIS - y)); //radians
  } else {
    angle1 = PI + acos(distance / (2 * LENGTH)) + atan((OFFSET - x) / (YAXIS - y)); //radians
  }

  // ----- calculate motor2 angle when pen at (x,y)
  if (x > OFFSET) {
    angle2 = PI - acos(distance / (2 * LENGTH)) - atan((x - OFFSET) / (YAXIS - y)); //radians
  } else {
    angle2 = PI - acos(distance / (2 * LENGTH)) + atan((OFFSET - x) / (YAXIS - y)); //radians
  }

  // ----- calculate steps required to reach (x,y) from 12 o'clock
  STEPS1 = int(angle1 * RAD_TO_DEG * STEPS_PER_DEG);
  STEPS2 = int(angle2 * RAD_TO_DEG * STEPS_PER_DEG);
}

//---------------------------------------------------------------------------
// CALCULATE DELAYS
//---------------------------------------------------------------------------
/*
   Assigns values to DELAY1, DELAY2 ready for next pen move
*/
void calculate_delays(long steps1, long steps2) {

  // ----- locals
  float
  rotate_time;

  long
  min_steps,
  max_steps,
  delay_max;

  // ----- find max and min number of steps
  max_steps = max(steps1, steps2);

  min_steps = min(steps1, steps2);

  // ----- calculate the total time for to complete the move
  rotate_time = (float)(max_steps * DELAY_MIN);

  // ----- calculate delay for motor with min_steps
  if (min_steps < 1) min_steps = 1;   //prevent possible divide by zero
  delay_max = (long)(rotate_time / ((float)min_steps));

  // ----- assign delays to each motor
  DELAY1 = (steps1 > steps2) ? DELAY_MIN : delay_max;
  DELAY2 = (steps1 > steps2) ? delay_max : DELAY_MIN;
}

//----------------------------------------------------------------------------
// DRAW ARC CLOCKWISE (G02)
//----------------------------------------------------------------------------
void draw_arc_cw(float x, float y, float i, float j) {

  // ----- inkscape sometimes produces some crazy values for i,j
  if ((i < -100) || (i > 100) || (j < -100) || (j > 100)) {
    move_to(x, y);
  } else {

    // ----- variables
    float
    thisX = LAST_X / SCALE_FACTOR, //current unscaled X co-ordinate
    thisY = LAST_Y / SCALE_FACTOR, //current unscaled Y co-ordinate
    nextX = x,                    //next X co-ordinate
    nextY = y,                    //next Y co-ordinate
    newX,                         //interpolated X co-ordinate
    newY,                         //interpolated Y co-ordinate
    I = i,                        //horizontal distance thisX from circle center
    J = j,                        //vertical distance thisY from circle center
    circleX = thisX + I,          //circle X co-ordinate
    circleY = thisY + J,          //circle Y co-ordinate
    delta_x,                      //horizontal distance between thisX and nextX
    delta_y,                      //vertical distance between thisY and nextY
    chord,                        //line_length between lastXY and nextXY
    radius,                       //circle radius
    alpha,                        //interior angle of arc
    beta,                         //fraction of alpha
    arc,                          //subtended by alpha
    current_angle,                //measured CCW from 3 o'clock
    next_angle;                   //measured CCW from 3 o'clock

    // ----- calculate arc
    delta_x = thisX - nextX;
    delta_y = thisY - nextY;
    chord = sqrt(delta_x * delta_x + delta_y * delta_y);
    radius = sqrt(I * I + J * J);
    alpha = 2 * asin(chord / (2 * radius)); //see construction lines
    arc = alpha * radius;         //radians

    // ----- sub-divide alpha
    int segments = 1;
    if (arc > ARC_MAX) {
      segments = (int)(arc / ARC_MAX);
      beta = alpha / segments;
    } else {
      beta = alpha;
    }

    // ----- calculate current angle
    /*
      atan2() angles between 0 and PI are CCW +ve from 3 o'clock.
      atan2() angles between 2*PI and PI are CW -ve relative to 3 o'clock
    */
    current_angle = atan2(-J, -I);
    if (current_angle <= 0) current_angle += 2 * PI;        //angles now 360..0 degrees CW

    // ----- plot intermediate CW co-ordinates
    next_angle = current_angle;                             //initialise angle
    for (int segment = 1; segment < segments; segment++) {
      next_angle -= beta;                                   //move CW around circle
      if (next_angle < 0) next_angle += 2 * PI;             //check if angle crosses zero
      newX = circleX + radius * cos(next_angle);            //standard circle formula
      newY = circleY + radius * sin(next_angle);
      move_to(newX, newY);
    }

    // ----- draw final line
    move_to(nextX, nextY);
  }
}

//----------------------------------------------------------------------------
// DRAW ARC COUNTER-CLOCKWISE (G03)
//----------------------------------------------------------------------------
/*
   We know the start and finish co-ordinates which allows us to calculate the
   chord length. We can also calculate the radius using the biarc I,J values.
   If we bisect the chord the center angle becomes 2*asin(chord/(2*radius)).
   The arc length may now be calculated using the formula arc_length = radius*angle.
*/
void draw_arc_ccw(float x, float y, float i, float j) {

  // ----- inkscape sometimes produces some crazy values for i,j
  if ((i < -100) || (i > 100) || (j < -100) || (j > 100)) {
    move_to(x, y);
  } else {

    // ----- variables
    float
    thisX = LAST_X / SCALE_FACTOR,  //current unscaled X co-ordinate
    thisY = LAST_Y / SCALE_FACTOR,  //current unscaled Y co-ordinate
    nextX = x,                      //next X co-ordinate
    nextY = y,                      //next Y co-ordinate
    newX,                           //interpolated X co-ordinate
    newY,                           //interpolated Y co-ordinate
    I = i,                          //horizontal distance thisX from circle center
    J = j,                          //vertical distance thisY from circle center
    circleX = thisX + I,            //circle X co-ordinate
    circleY = thisY + J,            //circle Y co-ordinate
    delta_x,                        //horizontal distance between thisX and nextX
    delta_y,                        //vertical distance between thisY and nextY
    chord,                          //line_length between lastXY and nextXY
    radius,                         //circle radius
    alpha,                          //interior angle of arc
    beta,                           //fraction of alpha
    arc,                            //subtended by alpha
    current_angle,                  //measured CCW from 3 o'clock
    next_angle;                     //measured CCW from 3 o'clock

    // ----- calculate arc
    delta_x = thisX - nextX;
    delta_y = thisY - nextY;
    chord = sqrt(delta_x * delta_x + delta_y * delta_y);
    radius = sqrt(I * I + J * J);
    alpha = 2 * asin(chord / (2 * radius));     //see construction lines
    arc = alpha * radius;                       //radians

    // ----- sub-divide alpha
    int segments = 1;
    if (arc > ARC_MAX) {
      segments = (int)(arc / ARC_MAX);
      beta = alpha / segments;
    } else {
      beta = alpha;
    }

    // ----- calculate current angle
    /*
        tan2() angles between 0 and PI are CCW +ve from 3 o'clock.
        atan2() angles between 2*PI and PI are CW -ve relative to 3 o'clock
    */
    current_angle = atan2(-J, -I);
    if (current_angle <= 0) current_angle += 2 * PI;        //angles now 360..0 degrees CW

    // ----- plot intermediate CCW co-ordinates
    next_angle = current_angle;                             //initialise angle
    for (int segment = 1; segment < segments; segment++) {
      next_angle += beta;                                   //move CCW around circle
      if (next_angle > 2 * PI) next_angle -= 2 * PI;        //check if angle crosses zero
      newX = circleX + radius * cos(next_angle);            //standard circle formula
      newY = circleY + radius * sin(next_angle);
      move_to(newX, newY);
    }

    // ----- draw final line
    move_to(nextX, nextY);
  }
}

//---------------------------------------------------------------------------
// PEN_UP
// Raise the pen
// Changing the value in OCR2B changes the pulse-width to the SG-90 servo
//---------------------------------------------------------------------------
void pen_up() {
  OCR2B = 148;                //1mS pulse
  delay(250);                 //give pen-lift time to respond
}

//---------------------------------------------------------------------------
// PEN_DOWN
// Lower the pen
// Changing the value in OCR2B changes the pulse-width to the SG-90 servo
//---------------------------------------------------------------------------
void pen_down() {
  OCR2B = 140;                //2mS pulse
  delay(250);                 //give pen-lift time to respond
}

// ----------------------------------------
// ABC
// ----------------------------------------
void abc() {
process(F("T2 S3"));
process(F("G00 X50.600359 Y23.420344"));
process(F("G02 X50.752716 Y22.976260 I-3.135884 J-1.324038"));
process(F("G02 X50.785093 Y22.730023 I-0.920147 J-0.246237"));
process(F("G02 X50.395324 Y21.695296 I-1.568337 J0.000000"));
process(F("G02 X48.616901 Y20.260423 I-5.033669 J4.419324"));
process(F("G02 X46.381993 Y19.348409 I-4.838496 J8.662483"));
process(F("G02 X44.183295 Y19.054795 I-2.198698 J8.085548"));
process(F("G02 X41.865268 Y19.467670 I0.000000 J6.713555"));
process(F("G02 X40.245550 Y20.503495 I1.545608 J4.201152"));
process(F("G02 X39.219290 Y22.122336 I3.157768 J3.136575"));
process(F("G02 X38.806572 Y24.470408 I6.473066 J2.348072"));
process(F("G02 X39.490101 Y28.182255 I10.420197 J0.000000"));
process(F("G02 X41.412290 Y31.305554 I9.193131 J-3.504638"));
process(F("G02 X44.336973 Y33.441702 I6.709781 J-6.116396"));
process(F("G02 X47.644620 Y34.164064 I3.307648 J-7.211572"));
process(F("G02 X49.085783 Y34.013721 I-0.000000 J-6.982526"));
process(F("G02 X50.133662 Y33.639032 I-0.850084 J-4.030028"));
process(F("G02 X50.927697 Y32.982080 I-1.126976 J-2.170474"));
process(F("G02 X51.144836 Y32.355618 I-0.795126 J-0.626462"));
process(F("G02 X50.979946 Y31.746676 I-1.206859 J0.000000"));
process(F("G02 X50.269784 Y30.858305 I-3.386486 J1.979114"));
process(F("G03 X48.739474 Y32.638692 I-4.305181 J-2.152593"));
process(F("G03 X46.934854 Y33.211228 I-1.804620 J-2.557788"));
process(F("G03 X44.865511 Y32.640761 I-0.000000 J-4.038459"));
process(F("G03 X42.812375 Y30.751354 I3.496454 J-5.859673"));
process(F("G03 X41.521944 Y28.150097 I7.294760 J-5.239488"));
process(F("G03 X41.052544 Y25.024608 I10.170799 J-3.125489"));
process(F("G03 X41.358190 Y23.047268 I6.548917 J0.000000"));
process(F("G03 X42.102608 Y21.709126 I3.656766 J1.158154"));
process(F("G03 X43.314946 Y20.829195 I2.521185 J2.198476"));
process(F("G03 X44.961119 Y20.493773 I1.646173 J3.871797"));
process(F("G03 X47.727663 Y21.168894 I0.000000 J6.006005"));
process(F("G03 X50.600359 Y23.420344 I-4.544548 J8.756936"));
process(F("G01 X50.600359 Y23.420344"));
process(F("G00 X23.454230 Y28.699836"));
process(F("G02 X23.258509 Y29.247403 I2.678175 J1.266042"));
process(F("G02 X23.201437 Y29.711010 I1.854425 J0.463606"));
process(F("G02 X23.715287 Y31.069809 I2.053497 J0.000000"));
process(F("G02 X25.904382 Y32.773699 I5.614239 J-4.954789"));
process(F("G02 X28.704691 Y33.800933 I5.587639 J-10.901752"));
process(F("G02 X31.854753 Y34.164064 I3.150061 J-13.481375"));
process(F("G02 X33.604787 Y33.959798 I0.000000 J-7.598769"));
process(F("G02 X34.771598 Y33.473743 I-0.900681 J-3.805687"));
process(F("G02 X35.615458 Y32.630428 I-1.363090 J-2.207829"));
process(F("G02 X35.889723 Y31.665300 I-1.560988 J-0.965128"));
process(F("G02 X35.303747 Y29.921282 I-2.888311 J0.000000"));
process(F("G02 X32.943707 Y27.776167 I-6.918162 J5.240505"));
process(F("G02 X34.567939 Y27.139583 I-2.104158 J-7.759112"));
process(F("G02 X35.636931 Y26.366360 I-2.132406 J-4.073640"));
process(F("G02 X36.379807 Y25.321353 I-2.380949 J-2.479122"));
process(F("G02 X36.618935 Y24.227336 I-2.383015 J-1.094017"));
process(F("G02 X36.026166 Y22.197076 I-3.773251 J0.000000"));
process(F("G02 X34.061832 Y20.221533 I-5.518247 J3.522571"));
process(F("G02 X31.369264 Y19.027768 I-5.128625 J7.934286"));
process(F("G02 X27.868393 Y18.568653 I-3.500871 J13.117974"));
process(F("G02 X25.793101 Y19.167138 I0.000000 J3.897363"));
process(F("G02 X25.194616 Y20.250700 I0.681659 J1.083563"));
process(F("G02 X25.225337 Y20.646314 I2.562654 J-0.000000"));
process(F("G02 X25.486301 Y22.117485 I35.898712 J-5.609165"));
process(F("G01 X26.857219 Y29.010969"));
process(F("G03 X26.998207 Y29.839815 I-14.262137 J2.852427"));
process(F("G03 X27.022507 Y30.187429 I-2.474194 J0.347614"));
process(F("G03 X26.964582 Y30.506866 I-0.909758 J0.000000"));
process(F("G03 X26.789159 Y30.809689 I-0.989871 J-0.371202"));
process(F("G02 X28.084797 Y31.229045 I4.918140 J-12.983875"));
process(F("G02 X28.587881 Y31.295831 I0.503083 J-1.861405"));
process(F("G02 X29.002366 Y31.107501 I0.000000 J-0.550275"));
process(F("G02 X29.190696 Y30.605510 I-0.574861 J-0.501991"));
process(F("G02 X29.174585 Y30.432163 I-0.940630 J0.000000"));
process(F("G02 X28.869843 Y28.894292 I-74.974595 J14.057713"));
process(F("G01 X27.596154 Y22.506398"));
process(F("G03 X27.407065 Y21.395624 I25.119200 J-4.847563"));
process(F("G03 X27.382252 Y21.057695 I2.288700 J-0.337928"));
process(F("G03 X27.843772 Y20.294140 I0.862387 J0.000000"));
process(F("G03 X29.715731 Y19.832619 I1.871959 J3.565639"));
process(F("G03 X31.526735 Y20.128159 I0.000000 J5.696494"));
process(F("G03 X32.992323 Y20.931299 I-1.502222 J4.480076"));
process(F("G03 X34.026640 Y22.200410 I-2.503366 J3.096265"));
process(F("G03 X34.363240 Y23.585630 I-2.682024 J1.385220"));
process(F("G03 X34.148489 Y24.695436 I-2.975045 J-0.000000"));
process(F("G03 X33.604861 Y25.471860 I-1.940658 J-0.780263"));
process(F("G03 X32.768796 Y25.999000 I-1.964159 J-2.188645"));
process(F("G03 X31.076926 Y26.521924 I-3.621282 J-8.717909"));
process(F("G02 X30.432077 Y26.335937 I-1.252000 J3.130013"));
process(F("G02 X29.696283 Y26.269132 I-0.735794 J4.018591"));
process(F("G02 X29.445459 Y26.286414 I0.000000 J1.828837"));
process(F("G02 X29.064300 Y26.356635 I0.596050 J4.304919"));
process(F("G02 X29.438640 Y27.338746 I5.106656 J-1.384047"));
process(F("G02 X29.764344 Y27.756722 I1.202868 J-0.601434"));
process(F("G02 X30.234517 Y28.000252 I0.776340 J-0.923211"));
process(F("G02 X31.135261 Y28.116468 I0.900744 J-3.432550"));
process(F("G02 X31.378139 Y28.111033 I0.000000 J-5.429361"));
process(F("G02 X31.786692 Y28.087298 I-0.689212 J-15.391372"));
process(F("G03 X33.457741 Y29.806863 I-4.247308 J5.799206"));
process(F("G03 X33.857653 Y31.091652 I-1.863851 J1.284789"));
process(F("G03 X33.240526 Y32.234356 I-1.366508 J-0.000000"));
process(F("G03 X31.174154 Y32.851482 I-2.066372 J-3.150932"));
process(F("G03 X27.293198 Y31.874110 I-0.000000 J-8.193950"));
process(F("G03 X23.454230 Y28.699836 I6.012387 J-11.179941"));
process(F("G01 X23.454230 Y28.699836"));
process(F("G00 X12.370209 Y25.345461"));
process(F("G01 X14.334220 Y25.296848"));
process(F("G03 X16.344033 Y25.341207 I0.000000 J45.552596"));
process(F("G03 X17.416355 Y25.432967 I-0.576039 J13.043259"));
process(F("G02 X17.057498 Y28.851447 I66.264688 J8.684258"));
process(F("G02 X16.969105 Y31.091652 I28.343468 J2.240205"));
process(F("G01 X16.978828 Y31.820863"));
process(F("G02 X16.654582 Y31.415231 I-24.362686 J19.142120"));
process(F("G02 X15.539850 Y30.051310 I-276.590024 J224.919481"));
process(F("G03 X13.629409 Y27.498969 I29.639011 J-24.176118"));
process(F("G03 X12.574388 Y25.724652 I13.226843 J-9.065587"));
process(F("G01 X12.370209 Y25.345461"));
process(F("G00 X11.670166 Y24.198168"));
process(F("G01 X11.475709 Y23.828703"));
process(F("G03 X10.453024 Y21.493002 I14.155155 J-7.589567"));
process(F("G03 X10.250633 Y20.289593 I3.476515 J-1.203409"));
process(F("G03 X10.316083 Y19.943208 I0.949331 J-0.000000"));
process(F("G03 X10.532595 Y19.570105 I1.323570 J0.518696"));
process(F("G03 X10.300257 Y19.489445 I3.137841 J-9.413445"));
process(F("G03 X9.482530 Y19.190914 I42.222649 J-116.924015"));
process(F("G02 X9.097875 Y19.086190 I-0.855158 J2.382242"));
process(F("G02 X8.763041 Y19.054795 I-0.334835 J1.769854"));
process(F("G02 X8.277401 Y19.249753 I0.000000 J0.702344"));
process(F("G02 X8.082443 Y19.706223 I0.436907 J0.456470"));
process(F("G02 X8.333756 Y20.987389 I3.391281 J0.000000"));
process(F("G02 X9.764492 Y23.828703 I18.642339 J-7.606424"));
process(F("G01 X10.065899 Y24.324564"));
process(F("G01 X9.570035 Y24.324564"));
process(F("G03 X8.016212 Y24.170112 I-0.000000 J-7.893143"));
process(F("G03 X7.100438 Y23.828703 I0.608728 J-3.031721"));
process(F("G02 X7.932520 Y24.978194 I1.964955 J-0.546467"));
process(F("G02 X9.560313 Y25.471860 I1.627793 J-2.436874"));
process(F("G01 X10.765943 Y25.442689"));
process(F("G01 X11.028459 Y25.831603"));
process(F("G02 X12.621837 Y28.077029 I38.056380 J-25.317154"));
process(F("G02 X15.695415 Y32.005598 I119.673321 J-90.461712"));
process(F("G02 X16.806236 Y33.260000 I14.322595 J-11.564166"));
process(F("G02 X17.280236 Y33.639032 I1.733405 J-1.681816"));
process(F("G02 X17.834414 Y33.882572 I1.177026 J-1.926044"));
process(F("G02 X19.312306 Y34.222402 I3.908941 J-13.616125"));
process(F("G03 X19.095228 Y32.467101 I23.799291 J-3.834329"));
process(F("G03 X19.030344 Y30.965256 I17.348913 J-1.501845"));
process(F("G03 X19.256050 Y26.994264 I35.045048 J-0.000000"));
process(F("G03 X19.954013 Y22.866141 I38.580488 J4.399934"));
process(F("G03 X20.720782 Y20.795305 I8.056900 J1.805858"));
process(F("G03 X21.499942 Y19.959018 I1.953184 J1.038650"));
process(F("G02 X20.521336 Y19.212088 I-3.388359 J3.424790"));
process(F("G02 X19.934567 Y19.054795 I-0.586769 J1.015804"));
process(F("G02 X18.944735 Y19.694699 I-0.000000 J1.085511"));
process(F("G02 X17.562198 Y24.324564 I16.634711 J7.488692"));
process(F("G02 X15.955536 Y24.193605 I-3.198664 J29.321420"));
process(F("G02 X14.324497 Y24.149555 I-1.631039 J30.174575"));
process(F("G01 X11.670166 Y24.198168"));
process(F("G00 X0.0000 Y0.0000"));
process(F("T2 S1"));
}

//----------------------------------------------------------------------------
// TARGET test pattern
//----------------------------------------------------------------------------
void target() {
process(F("T2 S3"));
process(F("G00 X51.309849 Y6.933768"));
process(F("G01 X7.893822 Y50.349788"));
process(F("G00 X7.893823 Y50.349788"));
process(F("G01 X51.309852 Y50.349788"));
process(F("G01 X51.309852 Y6.933760"));
process(F("G01 X7.893823 Y6.933760"));
process(F("G01 X7.893823 Y50.349788"));
process(F("G00 X43.948985 Y28.588440"));
process(F("G02 X39.778044 Y18.518899 I-14.240483 J0.000001"));
process(F("G02 X29.708503 Y14.347958 I-10.069542 J10.069542"));
process(F("G02 X19.638962 Y18.518898 I-0.000000 J14.240483"));
process(F("G02 X15.468020 Y28.588440 I10.069542 J10.069543"));
process(F("G02 X16.552012 Y34.038037 I14.240483 J0.000001"));
process(F("G02 X19.638961 Y38.657983 I13.156491 J-5.449596"));
process(F("G02 X24.258906 Y41.744932 I10.069543 J-10.069542"));
process(F("G02 X29.708503 Y42.828924 I5.449597 J-13.156491"));
process(F("G02 X39.778045 Y38.657982 I-0.000001 J-14.240483"));
process(F("G02 X43.948985 Y28.588440 I-10.069543 J-10.069541"));
process(F("G01 X43.948985 Y28.588440"));
process(F("G00 X51.309849 Y50.349788"));
process(F("G01 X7.893822 Y6.933768"));
process(F("G00 X0.0000 Y0.0000"));
process(F("T2 S1"));
}

//----------------------------------------------------------------------------
// RADIALS test pattern
//----------------------------------------------------------------------------
void radials() {

  // ----- move to the centre of the square
  pen_up();
  move_to(100, 100);

  // ----- draw octant 0 radials
  pen_down();
  move_to(150, 100);
  pen_up();
  move_to(100, 100);

  pen_down();
  move_to(150, 125);
  pen_up();
  move_to(100, 100);

  pen_down();
  move_to(150, 150);
  pen_up();
  move_to(100, 100);

  // ----- draw octant 1 radials
  pen_down();
  move_to(125, 150);
  pen_up();
  move_to(100, 100);

  pen_down();
  move_to(100, 150);
  pen_up();
  move_to(100, 100);

  // ----- draw octant 2 radials
  pen_down();
  move_to(75, 150);
  pen_up();
  move_to(100, 100);

  pen_down();
  move_to(50, 150);
  pen_up();
  move_to(100, 100);

  // ----- draw octant 3 radials
  pen_down();
  move_to(50, 125);
  pen_up();
  move_to(100, 100);

  pen_down();
  move_to(50, 100);
  pen_up();
  move_to(100, 100);

  // ----- draw octant 4 radials
  pen_down();
  move_to(50, 75);
  pen_up();
  move_to(100, 100);

  pen_down();
  move_to(50, 50);
  pen_up();
  move_to(100, 100);

  // ----- draw octant 5 radials
  pen_down();
  move_to(75, 50);
  pen_up();
  move_to(100, 100);

  pen_down();
  move_to(100, 50);
  pen_up();
  move_to(100, 100);

  // ----- draw octant 6 radials
  pen_down();
  move_to(125, 50);
  pen_up();
  move_to(100, 100);

  pen_down();
  move_to(150, 50);
  pen_up();
  move_to(100, 100);

  // ----- draw octant 7 radials
  pen_down();
  move_to(150, 75);
  pen_up();
  move_to(100, 100);
  pen_up();

  // ----- draw box
  move_to(50, 50);
  pen_down();
  move_to(50, 150);
  move_to(150, 150);
  move_to(150, 50);
  move_to(50, 50);
  pen_up();

  // home --------------
  move_to(0.0000, 0.0000);
}