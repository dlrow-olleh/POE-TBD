/*
 Code by lingib
 Last update 15 December 2016
 
 --------
 ABOUT
 --------
 This  "terminal" program has been designed to work with each of my 
 plotters that are published on http://www.instructables.com
 
 These plotters include:
 CNC Robot Plotter
 CNC Hanging Plotter
 CNC Drum Plotter (all software versions) 
 
 This code prevents characters getting lost through buffering and
 should, with minor changes, work with any gcode receiver that
 utilizes XON/XOFF handshaking.
 
 It assumes that the gcode file that you wish to plot is in your 
 "processing folder". If the file is in another folder then use 
 "relative addressing" to locate the gcode file.
 
 When using relative addressing precede each DOS path symbol (\) 
 with an escape character (\). Examples:
 
 "filename" means the file is inside the processing folder.
 ".\\filename" also means the file is inside the processing folder.
 "..\\filename" means the file is one level up.
 "..\\folder1\\filename means file is in folder1 which is one level up.
 
 ----------
 COPYRIGHT
 ----------
 This is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This software is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License. If 
 not, see <http://www.gnu.org/licenses/>.
 */

// ---------------------------------------------------
// DEFINITIONS
// ---------------------------------------------------
// ----- create port object
import processing.serial.*;         //library
Serial port;                        //create object from Serial class

// ----- flow control codes
final char XON = 0x11;              //plotter requests more code
final char XOFF= 0x13;              //stop sending code to plotter

// ----- file processing variables
String filename = "";               //file containing gcode
String keyboard = "";               //keyboard buffer
String[] command;                   //array of text commands
int number_commands;                //number of commands
int index = 0;                      //array index
String message = "";                //used for instructions

// ----- flags
boolean debug_on = false;            
//true: display serial port and file contents
//false:debug disabled

boolean filename_valid = false;     
//true: filename has been entered
//false: filename cleared

boolean keyboard_mode = true;       
//true: gcode comes from the keyboard
//false: gcode comes from a file

boolean display_menu = false;
//true: menu required
//false: menu not required

boolean plotter_paused = true;
//true: keyboard mode
//false: able to plot

boolean ok_to_send = true;
//true: send
//false: waiting for XON character from plotter

boolean finished_plotting = false;
//true: display finished plotting message
//false: display get initial filename message

// ---------------------------------------------------
// SETUP
// ---------------------------------------------------
void setup() { 

  // ----- create draw window
  size(300, 100); 
  textSize(12);                                 //set text size to 12
  textAlign(CENTER);
  background(#D3D3D3);                          //light gray
  fill(0);                                      //black text

  // ----- connect to the arduino
  if (debug_on) {
    printArray(Serial.list());                   //note the array number for your arduino port ...
  }
  String arduino_port = Serial.list()[0];        // ... and change the [2] to match
  port = new Serial(this, arduino_port, 9600);   //open 9600 baud serial connection
  port.clear();                                  //clear send buffer

  // ----- display start message
  message = "enter a command ...\nor\n\"right-click\" this box to start plotting";
  clear();
  background(#D3D3D3);
  text(message, 150, 30);

  // ----- display the menu
  port.write("menu\n");                         //ask plotter for the menu
}

// ---------------------------------------------------
// DRAW (main loop)
// ---------------------------------------------------
void draw() {

  // -----------------------------------------------------
  // send keypresses to the plotter
  // -----------------------------------------------------
  /* left/right-clicking switches between keyboard/plotter modes */
  if (mousePressed && (mouseButton == LEFT)) {
    delay(500);    //debounce delay
    keyboard_mode = true;             //send keypresses to the plotter  
    display_menu = true;              //causes menu to be displayed
    plotter_paused = true;            //pause plotting
  }

  // -----------------------------------------------------
  // send file contents to the plotter
  // -----------------------------------------------------
  /* left/right-clicking switches between keyboard/plotter modes */
  if (mousePressed && (mouseButton == RIGHT)) { 
    delay(500);    //debounce delay
    keyboard_mode = false;            //send file contents to the plotter
    display_menu = true;              //display menu whenever we enter keyboard mode
    plotter_paused = false;           //pause plotting
  }

  // -----------------------------------------------------
  // display the menu whenever we enter the keyboard mode
  // -----------------------------------------------------
  if ((keyboard_mode) && (display_menu)) {
    // ----- message
    message = "enter a command ...\nor\n\"right-click\" this box to continue plotting";
    clear();
    background(#D3D3D3);
    text(message, 150, 30);
    // ----- request menu from the plotter
    port.write("menu\n");            //use "menu\n" with Drum_Plotter_V3.ino

    /* 
     use "M0\n" with my other plotters:
     These include:
     Drum_Plotter.ino
     Drum_Plotter_V2.ino
     Hanging_Plotter.ino
     Robot_Plotter.ino
     */

    println("");
    display_menu = false;            //one time action
  }

  // -----------------------------------------------------
  // get initial file name
  // -----------------------------------------------------
  if ((keyboard_mode == false) && (!filename_valid) && (!finished_plotting)) {
    // ----- message
    message = "enter the \"filename\" you wish to plot\nor\n\"left-click\" this box to enter a command"; 
    clear();
    background(#D3D3D3);
    text(message, 150, 30);
    
    /* The KEY PRESSED routine intercepts your keypresses and performs this task */
  }

  // -----------------------------------------------------
  // get next file name
  // -----------------------------------------------------
  if ((keyboard_mode == false) && (!filename_valid) && (finished_plotting)) {
    // ----- message
    message = "finished plotting ...\n\nenter another \"filename\" or\n\"left-click\" this box to enter a command"; 
    clear();
    background(#D3D3D3);
    text(message, 150, 30);
    
    /* The KEY PRESSED routine intercepts your keypresses and performs this task */
  }

  // -----------------------------------------------------
  // plot the file
  // -----------------------------------------------------

  /* 
   left mouse-click: pauses the plotter for pen color changes
   right mouse-click: resumes plotting
   */

  if  ((ok_to_send) && (filename_valid) && (index < number_commands) && (!plotter_paused)) {

    // ----- message
    message = "now plotting ....\n\n\"left-click\" this box to pause\nor enter a command";
    clear();
    background(#D3D3D3);
    text(message, 150, 20);

    // ----- send the file "line-by-line" "character-by-character"
    char[] character = new char[command[index].length()];             //create a character array for the current command
    for (int i = 0; i < command[index].length(); i++) {               //extract the command characters into the array
      character[i] = command[index].charAt(i);
    }

    // ----- send the command to the plotter
    for (int i = 0; i < (command[index].length()); i++) { 
      port.write(command[index].charAt(i));
    }
    port.write('\n');                                                 //complete the command

    /* delay() between characters can go here if required */

    // ----- generate a local XOFF rather than wait for the plotter
    ok_to_send = false;                                               //local XOFF
    port.clear();                                                     //clear transmit buffer

    /* ok_to-send flag is set when SERIAL EVENT receives an XON character from the plotter */

    // ----- point to the next command line    
    index++;

    // ----- set/reset the flags
    if (index >= number_commands) {
      filename = "";
      filename_valid = false;
      index = 0;
      plotter_paused = false;
      finished_plotting = true;
    }
  }
}

// ---------------------------------------------------
// SERIAL EVENT (called whenever a character arrives)
// ---------------------------------------------------
void serialEvent (Serial port) {
  char letter = port.readChar();
  if (letter == XON) ok_to_send = true;      //plotter ready for next command
  print(letter);                            //echo character from arduino
}

// ---------------------------------------------------
// KEY PRESSED (called each key press)
// ---------------------------------------------------
void keyPressed() {
  // ----- validate each keypress. Also prevents control characters from showing.
  switch (key) {
  case 'a': 
  case 'b': 
  case 'c':
  case 'd': 
  case 'e': 
  case 'f': 
  case 'g': 
  case 'h': 
  case 'i': 
  case 'j': 
  case 'k': 
  case 'l': 
  case 'm': 
  case 'n': 
  case 'o': 
  case 'p': 
  case 'q': 
  case 'r': 
  case 's': 
  case 't': 
  case 'u': 
  case 'v': 
  case 'w': 
  case 'x': 
  case 'y': 
  case 'z':
  case 'A': 
  case 'B': 
  case 'C': 
  case 'D': 
  case 'E': 
  case 'F': 
  case 'G': 
  case 'H': 
  case 'I': 
  case 'J': 
  case 'K': 
  case 'L': 
  case 'M': 
  case 'N': 
  case 'O': 
  case 'P': 
  case 'Q': 
  case 'R': 
  case 'S': 
  case 'T': 
  case 'U': 
  case 'V': 
  case 'W': 
  case 'X': 
  case 'Y': 
  case 'Z':  
  case '1': 
  case '2': 
  case '3': 
  case '4': 
  case '5': 
  case '6': 
  case '7': 
  case '8': 
  case '9': 
  case '0': 
  case ' ':
  case '.':
  case '_': 
  case '\\': 
  case '\r':
  case '\n':
    {
      // ----- send each keypress to the plotter
      if (keyboard_mode == true) {
        port.write(key);                        //plotter will echo each keypress
      }

      // ----- get filename while in plotter mode
      if ((keyboard_mode == false) && (!filename_valid)) {    
        keyboard += key;
        print(key);                                   //local keypress echo
        if ((key == '\r')||(key=='\n')) {             //enter key pressed
          filename = keyboard.trim();                 //remove white space including \r\n

          // ----- read the file
          command = loadStrings(filename);            //create an array of command strings
          number_commands = command.length;           //find number of command strings

          // ----- display the file contents
          if (debug_on) {
            for (int i=0; i<number_commands; i++) {
              println(command[i]);                    //local print
            }
            println("");                              //screen formatting
          }

          // ----- housekeeping
          keyboard = "";
          filename_valid = true;                     //make "get filename" a one time action
        }
      }
    }
  default: 
    {
      break;
    }
  }
}
