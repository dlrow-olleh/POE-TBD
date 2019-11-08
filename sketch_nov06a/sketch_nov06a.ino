#include <FABRIK2D.h>
#include <Servo.h>

// For a 2DOF arm, we have 2 links and 2+1 joints, 
// where the end effector counts as one joint in this case.
int lengths[] = {203, 127}; // Length of shoulder and elbow in mm.
Fabrik2D fabrik2D(2, lengths); // 3 Joints in total

// Servos should be positioned so that when all servo angles are
// equal to 90 degrees, the manipulator should point straight up.
Servo shoulder;
Servo elbow;
char userInput;

void setup() {
  shoulder.attach(10);
  elbow.attach(3);
  fabrik2D.setTolerance(0.5);
}

void loop() {

  if(Serial.available()> 0){ 
    userInput = Serial.read();               // read user input
      if(userInput == 'square'){ 
        fabrik2D.solve(100,50,lengths);
        int shoulderAngle = fabrik2D.getAngle(0)* 57296 / 1000; // In degrees
        int elbowAngle = fabrik2D.getAngle(1)* 57296 / 1000;
        shoulder.write(min(180, max(0, shoulderAngle + 180/2)));
        elbow.write(min(180, max(0, elbowAngle + 180/2)));
        fabrik2D.solve(300,50,lengths);
        shoulderAngle = fabrik2D.getAngle(0)* 57296 / 1000; // In degrees
        elbowAngle = fabrik2D.getAngle(1)* 57296 / 1000;
        shoulder.write(min(180, max(0, shoulderAngle + 180/2)));
        elbow.write(min(180, max(0, elbowAngle + 180/2)));
        fabrik2D.solve(300,100,lengths);
        shoulderAngle = fabrik2D.getAngle(0)* 57296 / 1000; // In degrees
        elbowAngle = fabrik2D.getAngle(1)* 57296 / 1000;
        shoulder.write(min(180, max(0, shoulderAngle + 180/2)));
        elbow.write(min(180, max(0, elbowAngle + 180/2)));
        fabrik2D.solve(100,100,lengths);
        shoulderAngle = fabrik2D.getAngle(0)* 57296 / 1000; // In degrees
        elbowAngle = fabrik2D.getAngle(1)* 57296 / 1000;
        shoulder.write(min(180, max(0, shoulderAngle + 180/2)));
        elbow.write(min(180, max(0, elbowAngle + 180/2)));   
      }
  
  
  

}
}
